#include "Application/Debug/Telemetry.h"
#include "Application/Control/MotionLine.h"
#include "Application/Control/MotionManager.h"
#include "Application/Control/MotionWheel.h"
#include "Application/Control/Nav.h"
#include "Application/Debug/TelemFrame.h"
#include "Application/Core/CarRole.h"
#include "Application/State/Heading.h"
#include "Application/State/Odometry.h"
#include "Hardware/Comms/Serial.h"
#include "Hardware/Sensors/Graydetect.h"
#include "ti_msp_dl_config.h"
#include <math.h>
#include <stddef.h>

#define TELEMETRY_TICK_HZ 100U

/* 每秒可发送字节数。8N1 每字节 10 位时，取 SysConfig 波特率不写死。 */
#define TELEMETRY_UART_BYTES_PER_SECOND ((uint32_t)BLUETOOTH_UART_BAUD_RATE / 10U)

/*
 * DMA 后发送不再阻塞主循环，带宽预算从旧的 20% 放宽。留 30% 余量给命令回应、
 * K230 帧和 TX 缓冲波动，其余用于遥测——上限因此远高于阻塞版。
 */
#define TELEMETRY_BANDWIDTH_PERCENT 70U

/* 一个通道的定义：名字、单位码、取值函数。表序即列序，与位定义一一对应。 */
typedef struct
{
    uint16_t bit;
    const char *name;
    uint8_t unit;
    float (*read)(void);
} Telemetry_Channel_t;

/* 模式当前值转成 float：整数编码，网页可映射回文本。见下方 Telemetry_ModeCode。 */
static float Telemetry_ReadTL(void)   { return MotionWheel_GetTargetSpeedL(); }
static float Telemetry_ReadLV(void)   { return Odometry_GetSpeedL(); }
static float Telemetry_ReadPL(void)   { return MotionWheel_GetLeftCommandPWM(); }
static float Telemetry_ReadTR(void)   { return MotionWheel_GetTargetSpeedR(); }
static float Telemetry_ReadRV(void)   { return Odometry_GetSpeedR(); }
static float Telemetry_ReadPR(void)   { return MotionWheel_GetRightCommandPWM(); }
static float Telemetry_ReadYaw(void)
{
    return (Heading_IsReady() != 0U) ? Heading_GetYaw() : NAN;
}

static float Telemetry_ReadNavE(void)
{
    return (Heading_IsReady() != 0U) ? Nav_GetAngleErrorDeg() : NAN;
}
static float Telemetry_ReadLerr(void) { return MotionLine_GetLineError(); }
static float Telemetry_ReadGray(void) { return (float)Graydetect_GetState(); }
static float Telemetry_ReadLD(void)   { return Odometry_GetDistanceLMM(); }
static float Telemetry_ReadRD(void)   { return Odometry_GetDistanceRMM(); }

/* 顺序必须与 TELEMETRY_CH_* 的位序一致：schema、sample、行长估算都遍历它。 */
static const Telemetry_Channel_t s_channels[] = {
    { TELEMETRY_CH_TL,   "TL",   TELEM_UNIT_MMPS, Telemetry_ReadTL   },
    { TELEMETRY_CH_LV,   "LV",   TELEM_UNIT_MMPS, Telemetry_ReadLV   },
    { TELEMETRY_CH_PL,   "PL",   TELEM_UNIT_PWM,  Telemetry_ReadPL   },
    { TELEMETRY_CH_TR,   "TR",   TELEM_UNIT_MMPS, Telemetry_ReadTR   },
    { TELEMETRY_CH_RV,   "RV",   TELEM_UNIT_MMPS, Telemetry_ReadRV   },
    { TELEMETRY_CH_PR,   "PR",   TELEM_UNIT_PWM,  Telemetry_ReadPR   },
    { TELEMETRY_CH_YAW,  "yaw",  TELEM_UNIT_DEG,  Telemetry_ReadYaw  },
    { TELEMETRY_CH_NAVE, "navE", TELEM_UNIT_DEG,  Telemetry_ReadNavE },
    { TELEMETRY_CH_LERR, "lerr", TELEM_UNIT_RAW,  Telemetry_ReadLerr },
    { TELEMETRY_CH_GRAY, "gray", TELEM_UNIT_BITS, Telemetry_ReadGray },
    { TELEMETRY_CH_LD,   "LD",   TELEM_UNIT_MM,   Telemetry_ReadLD   },
    { TELEMETRY_CH_RD,   "RD",   TELEM_UNIT_MM,   Telemetry_ReadRD   },
};

#define TELEMETRY_CHANNEL_COUNT \
    (sizeof(s_channels) / sizeof(s_channels[0]))

static uint8_t s_rateHz;
static uint16_t s_fieldMask;
static uint8_t s_tickAccumulator;
static uint8_t s_schemaPending;
static uint32_t s_elapsedMs;
static uint8_t s_sequence;

/*
 * 遥测帧出口。主车走 Serial1（→ 电脑，首字节 0xAA）；从车走 HC05 主从链路
 * Serial2，并把首字节改成 0xAB，与主车 0xAA 区分——CRC 只覆盖 VER..PAYLOAD，
 * 不含 magic，改首字节不破坏校验。主车按 0xAB 原样透传给电脑，网页据此把从车
 * 数据分流到独立曲线面板。
 */
static void Telemetry_Emit(uint8_t *frame, uint16_t frameLength)
{
    if (frame == NULL)
    {
        return;
    }
#if CAR_IS_SLAVE
    frame[0] = 0xABU;
    (void)Serial2_SendArray(frame, frameLength);
#else
    Serial1_SendArray(frame, frameLength);
#endif
}

static uint8_t Telemetry_CountChannels(uint16_t mask)
{
    uint8_t count = 0U;
    uint32_t index;

    for (index = 0U; index < TELEMETRY_CHANNEL_COUNT; index++)
    {
        if ((mask & s_channels[index].bit) != 0U)
        {
            count++;
        }
    }
    return count;
}

/* SAMPLE 帧字节数 = 帧开销 + payload(ms:4 + 通道数×4)。 */
static uint16_t Telemetry_SampleFrameBytes(uint16_t mask)
{
    uint16_t payload = (uint16_t)(4U + (uint16_t)Telemetry_CountChannels(mask) * 4U);
    return (uint16_t)(TELEM_FRAME_OVERHEAD + payload);
}

/*
 * 安全频率上限：帧字节数 × 频率 不得超过可用带宽。
 * DMA 不阻塞主循环，约束改为"不撑爆 TX 缓冲、不超串口物理带宽"。
 * 纯整数运算。
 */
uint8_t Telemetry_GetMaxRateHz(void)
{
    uint32_t frameBytes = (uint32_t)Telemetry_SampleFrameBytes(s_fieldMask);
    uint32_t maxRate;

    maxRate = (TELEMETRY_UART_BYTES_PER_SECOND * TELEMETRY_BANDWIDTH_PERCENT)
              / (frameBytes * 100U);

    if (maxRate < 1U)
    {
        maxRate = 1U;
    }
    if (maxRate > (uint32_t)TELEMETRY_RATE_HARD_LIMIT_HZ)
    {
        maxRate = (uint32_t)TELEMETRY_RATE_HARD_LIMIT_HZ;
    }
    return (uint8_t)maxRate;
}

uint8_t Telemetry_SampleChannels(uint16_t mask, float *out)
{
    uint8_t count = 0U;
    uint32_t index;

    if (out == NULL)
    {
        return 0U;
    }
    for (index = 0U; index < TELEMETRY_CHANNEL_COUNT; index++)
    {
        if ((mask & s_channels[index].bit) != 0U)
        {
            out[count] = s_channels[index].read();
            count++;
        }
    }
    return count;
}

void Telemetry_SendSchema(uint16_t mask, uint8_t frameType)
{
    uint8_t payload[TELEM_FRAME_MAX_PAYLOAD];
    uint8_t frame[TELEM_FRAME_MAX_BYTES];
    uint16_t offset = 0U;
    uint16_t frameLength;
    uint32_t index;

    /* payload: channelMask(u16) + 每通道 { nameLen(u8), name[], unit(u8) }。 */
    offset += TelemFrame_PackU16(&payload[offset], mask);
    for (index = 0U; index < TELEMETRY_CHANNEL_COUNT; index++)
    {
        const Telemetry_Channel_t *channel = &s_channels[index];
        uint8_t nameLen = 0U;

        if ((mask & channel->bit) == 0U)
        {
            continue;
        }
        while (channel->name[nameLen] != '\0')
        {
            nameLen++;
        }
        /* 通道名很短（<=4），payload 上限 255，不会溢出；仍保守判一次。 */
        if ((offset + 2U + nameLen) > TELEM_FRAME_MAX_PAYLOAD)
        {
            break;
        }
        payload[offset++] = nameLen;
        for (uint8_t i = 0U; i < nameLen; i++)
        {
            payload[offset++] = (uint8_t)channel->name[i];
        }
        payload[offset++] = channel->unit;
    }

    frameLength = TelemFrame_Build(frame, frameType, s_sequence++,
                                   payload, (uint8_t)offset);
    Telemetry_Emit(frame, frameLength);
}

static void Telemetry_SendSample(void)
{
    uint8_t payload[TELEM_FRAME_MAX_PAYLOAD];
    uint8_t frame[TELEM_FRAME_MAX_BYTES];
    float values[TELEMETRY_CHANNEL_COUNT];
    uint16_t offset = 0U;
    uint8_t count;
    uint16_t frameLength;

    offset += TelemFrame_PackU32(&payload[offset], s_elapsedMs);
    count = Telemetry_SampleChannels(s_fieldMask, values);
    offset += TelemFrame_PackFloats(&payload[offset], values, count);

    frameLength = TelemFrame_Build(frame, TELEM_FRAME_TYPE_SAMPLE,
                                   s_sequence++, payload, (uint8_t)offset);
    Telemetry_Emit(frame, frameLength);
}

void Telemetry_Init(void)
{
    uint8_t maxRate;

    /* 默认掩码取轮速调参子集（TL/LV/PL/TR/RV/PR），比全字段更常用且更快。 */
    s_fieldMask = TELEMETRY_CH_TL | TELEMETRY_CH_LV | TELEMETRY_CH_PL |
                  TELEMETRY_CH_TR | TELEMETRY_CH_RV | TELEMETRY_CH_PR;
    s_rateHz = TELEMETRY_DEFAULT_RATE_HZ;

    maxRate = Telemetry_GetMaxRateHz();
    if (s_rateHz > maxRate)
    {
        s_rateHz = maxRate;
    }

#if CAR_IS_SLAVE
    /* 从车遥测默认关闭：它要经主车 UART1 转发给电脑，与主车自身遥测共用带宽。
     * 默认关掉，网页需要时发 @G<rate> 开启，避免一上电就吃掉主车上行带宽。 */
    s_rateHz = 0U;
#endif

    s_tickAccumulator = 0U;
    s_schemaPending = 1U;
    s_elapsedMs = 0U;
    s_sequence = 0U;
}

void Telemetry_Update(uint8_t elapsedTicks, uint8_t pressedKeys)
{
    uint8_t interval;

    (void)pressedKeys;   /* 按键改由独立通道方案处理，暂不进遥测帧。 */
    s_elapsedMs += (uint32_t)elapsedTicks * 10U;

    if (s_rateHz == 0U)
    {
        return;
    }

    interval = (uint8_t)(TELEMETRY_TICK_HZ / s_rateHz);
    s_tickAccumulator = (uint8_t)(s_tickAccumulator + elapsedTicks);
    if (s_tickAccumulator < interval)
    {
        return;
    }
    s_tickAccumulator = 0U;

    if (s_schemaPending != 0U)
    {
        Telemetry_SendSchema(s_fieldMask, TELEM_FRAME_TYPE_SCHEMA);
        s_schemaPending = 0U;
    }
    Telemetry_SendSample();
}

uint8_t Telemetry_SetRateHz(uint8_t rateHz)
{
    if ((rateHz != 0U) && (rateHz > Telemetry_GetMaxRateHz()))
    {
        return 0U;
    }
    s_rateHz = rateHz;
    s_tickAccumulator = 0U;
    return 1U;
}

uint8_t Telemetry_SetFieldMask(uint16_t mask)
{
    uint8_t maxRate;

    if ((mask == 0U) || (mask > TELEMETRY_CH_ALL))
    {
        return 0U;
    }
    s_fieldMask = mask;
    s_schemaPending = 1U;   /* 掩码变化后必须重发 schema。 */

    maxRate = Telemetry_GetMaxRateHz();
    if (s_rateHz > maxRate)
    {
        s_rateHz = maxRate;
    }
    return 1U;
}

uint8_t Telemetry_GetRateHz(void)
{
    return s_rateHz;
}

uint16_t Telemetry_GetFieldMask(void)
{
    return s_fieldMask;
}
