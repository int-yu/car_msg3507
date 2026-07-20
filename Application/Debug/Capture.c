#include "Application/Debug/Capture.h"
#include "Application/Control/MotionLine.h"
#include "Application/Control/MotionWheel.h"
#include "Application/Control/Nav.h"
#include "Application/State/Heading.h"
#include "Application/State/Odometry.h"
#include "Hardware/Comms/Serial.h"

/* 每样本 = 时间戳 + 各通道 float32。 */
#define CAPTURE_SAMPLE_BYTES(channels) \
    (sizeof(uint32_t) + ((uint32_t)(channels) * sizeof(float)))

/* 一次 DumpNext 输出的行数。dump 时车已停稳，阻塞发送不影响控制，
 * 但仍分批进行，避免单次占用主循环几百毫秒导致看门狗或串口溢出。 */
#define CAPTURE_DUMP_ROWS_PER_TICK 4U

typedef struct
{
    uint16_t bit;
    const char *name;
} Capture_ChannelInfo_t;

/* 顺序必须与 CAPTURE_CH_* 的位序一致：写入和 dump 都按这张表遍历。 */
static const Capture_ChannelInfo_t s_channels[] = {
    { CAPTURE_CH_TL,   "TL"   },
    { CAPTURE_CH_LV,   "LV"   },
    { CAPTURE_CH_PL,   "PL"   },
    { CAPTURE_CH_TR,   "TR"   },
    { CAPTURE_CH_RV,   "RV"   },
    { CAPTURE_CH_PR,   "PR"   },
    { CAPTURE_CH_YAW,  "yaw"  },
    { CAPTURE_CH_NAVE, "navE" },
    { CAPTURE_CH_LERR, "lerr" },
};

#define CAPTURE_CHANNEL_COUNT \
    (sizeof(s_channels) / sizeof(s_channels[0]))

static uint8_t s_buffer[CAPTURE_BUFFER_BYTES];
static Capture_State_t s_state;
static uint16_t s_channelMask;
static uint8_t s_channelCount;
static uint16_t s_sampleCount;
static uint16_t s_capacity;
static uint16_t s_dumpIndex;
static uint32_t s_elapsedMs;

static float Capture_ReadChannel(uint16_t bit)
{
    switch (bit)
    {
        case CAPTURE_CH_TL:   return MotionWheel_GetTargetSpeedL();
        case CAPTURE_CH_LV:   return Odometry_GetSpeedL();
        case CAPTURE_CH_PL:   return MotionWheel_GetLeftCommandPWM();
        case CAPTURE_CH_TR:   return MotionWheel_GetTargetSpeedR();
        case CAPTURE_CH_RV:   return Odometry_GetSpeedR();
        case CAPTURE_CH_PR:   return MotionWheel_GetRightCommandPWM();
        case CAPTURE_CH_YAW:  return Heading_GetYaw();
        case CAPTURE_CH_NAVE: return Nav_GetAngleErrorDeg();
        case CAPTURE_CH_LERR: return MotionLine_GetLineError();
        default:              return 0.0f;
    }
}

static uint8_t Capture_CountChannels(uint16_t mask)
{
    uint8_t count = 0U;
    uint32_t index;

    for (index = 0U; index < CAPTURE_CHANNEL_COUNT; index++)
    {
        if ((mask & s_channels[index].bit) != 0U)
        {
            count++;
        }
    }
    return count;
}

/* 逐字节写入，不做 4 字节对齐假设：样本大小随通道数变化，
 * 直接按 float* 解引用会在奇数偏移上触发 Cortex-M0+ 的非对齐访问异常。 */
static void Capture_WriteBytes(uint32_t offset, const void *data,
                               uint32_t length)
{
    const uint8_t *source = (const uint8_t *)data;
    uint32_t index;

    for (index = 0U; index < length; index++)
    {
        s_buffer[offset + index] = source[index];
    }
}

static void Capture_ReadBytes(uint32_t offset, void *data, uint32_t length)
{
    uint8_t *target = (uint8_t *)data;
    uint32_t index;

    for (index = 0U; index < length; index++)
    {
        target[index] = s_buffer[offset + index];
    }
}

void Capture_Init(void)
{
    s_state = CAPTURE_STATE_IDLE;
    s_channelMask = 0U;
    s_channelCount = 0U;
    s_sampleCount = 0U;
    s_capacity = 0U;
    s_dumpIndex = 0U;
    s_elapsedMs = 0U;
}

uint8_t Capture_Arm(uint16_t channelMask)
{
    uint8_t count;

    if (s_state == CAPTURE_STATE_DUMPING)
    {
        return 0U;
    }
    if ((channelMask == 0U) || ((channelMask & ~CAPTURE_CH_ALL) != 0U))
    {
        return 0U;
    }

    count = Capture_CountChannels(channelMask);
    if ((count == 0U) || (count > CAPTURE_MAX_CHANNELS))
    {
        return 0U;
    }

    s_channelMask = channelMask;
    s_channelCount = count;
    s_capacity = (uint16_t)((uint32_t)CAPTURE_BUFFER_BYTES /
                            CAPTURE_SAMPLE_BYTES(count));
    s_sampleCount = 0U;
    s_dumpIndex = 0U;
    s_elapsedMs = 0U;
    s_state = CAPTURE_STATE_ARMED;
    return 1U;
}

void Capture_Trigger(void)
{
    if (s_state != CAPTURE_STATE_ARMED)
    {
        return;
    }
    s_sampleCount = 0U;
    s_elapsedMs = 0U;
    s_state = CAPTURE_STATE_RUNNING;
}

void Capture_Update(uint32_t elapsedMs)
{
    uint32_t offset;
    uint32_t index;

    if (s_state != CAPTURE_STATE_RUNNING)
    {
        return;
    }

    s_elapsedMs += elapsedMs;
    if (s_sampleCount >= s_capacity)
    {
        /* 写满即停：继续覆盖会让时间轴失去连续性，分析阶跃时更麻烦。 */
        s_state = CAPTURE_STATE_FULL;
        return;
    }

    offset = (uint32_t)s_sampleCount * CAPTURE_SAMPLE_BYTES(s_channelCount);
    Capture_WriteBytes(offset, &s_elapsedMs, sizeof(s_elapsedMs));
    offset += sizeof(s_elapsedMs);

    for (index = 0U; index < CAPTURE_CHANNEL_COUNT; index++)
    {
        float value;

        if ((s_channelMask & s_channels[index].bit) == 0U)
        {
            continue;
        }
        value = Capture_ReadChannel(s_channels[index].bit);
        Capture_WriteBytes(offset, &value, sizeof(value));
        offset += sizeof(value);
    }
    s_sampleCount++;
}

void Capture_Stop(void)
{
    if ((s_state == CAPTURE_STATE_RUNNING) ||
        (s_state == CAPTURE_STATE_ARMED))
    {
        s_state = (s_sampleCount > 0U) ?
            CAPTURE_STATE_FULL : CAPTURE_STATE_IDLE;
    }
}

uint16_t Capture_StartDump(void)
{
    uint32_t index;

    if ((s_state == CAPTURE_STATE_DUMPING) || (s_sampleCount == 0U))
    {
        return 0U;
    }

    /* 表头列名顺序与写入顺序一致，网页据此解析而不猜列位。 */
    Serial1_SendString("CH,ms");
    for (index = 0U; index < CAPTURE_CHANNEL_COUNT; index++)
    {
        if ((s_channelMask & s_channels[index].bit) != 0U)
        {
            Serial1_SendByte((uint8_t)',');
            Serial1_SendString(s_channels[index].name);
        }
    }
    Serial1_SendString("\r\n");

    s_dumpIndex = 0U;
    s_state = CAPTURE_STATE_DUMPING;
    return s_sampleCount;
}

void Capture_DumpNext(void)
{
    uint8_t row;

    if (s_state != CAPTURE_STATE_DUMPING)
    {
        return;
    }

    for (row = 0U; row < CAPTURE_DUMP_ROWS_PER_TICK; row++)
    {
        uint32_t offset;
        uint32_t index;
        uint32_t timestampMs;

        if (s_dumpIndex >= s_sampleCount)
        {
            Serial1_Printf("OK X END=%u\r\n", (unsigned)s_sampleCount);
            s_state = CAPTURE_STATE_IDLE;
            return;
        }

        offset = (uint32_t)s_dumpIndex *
                 CAPTURE_SAMPLE_BYTES(s_channelCount);
        Capture_ReadBytes(offset, &timestampMs, sizeof(timestampMs));
        offset += sizeof(timestampMs);
        Serial1_Printf("C,%lu", (unsigned long)timestampMs);

        for (index = 0U; index < CAPTURE_CHANNEL_COUNT; index++)
        {
            float value;

            if ((s_channelMask & s_channels[index].bit) == 0U)
            {
                continue;
            }
            Capture_ReadBytes(offset, &value, sizeof(value));
            offset += sizeof(value);
            Serial1_Printf(",%.2f", (double)value);
        }
        Serial1_SendString("\r\n");
        s_dumpIndex++;
    }
}

Capture_State_t Capture_GetState(void)
{
    return s_state;
}

uint16_t Capture_GetSampleCount(void)
{
    return s_sampleCount;
}

uint16_t Capture_GetCapacity(void)
{
    return s_capacity;
}

uint16_t Capture_GetChannelMask(void)
{
    return s_channelMask;
}
