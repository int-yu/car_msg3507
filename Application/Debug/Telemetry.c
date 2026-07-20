#include "Application/Debug/Telemetry.h"
#include "Application/Comms/K230Link.h"
#include "Application/Control/MotionLine.h"
#include "Application/Control/MotionManager.h"
#include "Application/Control/MotionWheel.h"
#include "Application/Control/Nav.h"
#include "Application/State/Heading.h"
#include "Application/State/Odometry.h"
#include "Hardware/Comms/Serial.h"
#include "Hardware/Sensors/Graydetect.h"
#include "ti_msp_dl_config.h"

/* 100 Hz 主循环下，每 (100 / rateHz) 拍输出一行。 */
#define TELEMETRY_TICK_HZ 100U

/* 每秒可发送的字节数。8N1 每字节含起始位和停止位共 10 个位时，故除以 10。
 * 直接取 SysConfig 生成的波特率，不写死数值——否则改了 UART1 波特率而漏改
 * 这里，限流就会按错误的带宽计算：波特率调低时会严重超发，阻塞发送会把
 * 100 Hz 主循环连同运动控制一起拖垮，且这种失配没有任何编译期提示。 */
#define TELEMETRY_UART_BYTES_PER_SECOND  ((uint32_t)BLUETOOTH_UART_BAUD_RATE / 10U)

/* 允许遥测阻塞占用的主循环时间上限百分比。 */
#define TELEMETRY_MAX_BLOCKING_PERCENT   20U

static uint8_t s_rateHz;
static uint16_t s_fieldMask;
static uint8_t s_tickAccumulator;
static uint8_t s_headerPending;
static uint32_t s_elapsedMs;

/* 估算当前掩码下数据行的最坏情况字节数（含行尾 \r\n）。
 * 各字段宽度为 115200 8N1 格式下实际可能输出的最大字符数，含前导逗号。
 * 全字段约 160 字节，必须用 uint16_t 累加。 */
static uint16_t Telemetry_EstimateRowBytes(void)
{
    uint16_t bytes = 14U;  /* 行基座：'D,' + 最长 10 位 ms + '\r\n' */

    if ((s_fieldMask & TELEMETRY_FIELD_YAW) != 0U)
    {
        bytes = (uint16_t)(bytes + 11U);  /* ',-999999.99' */
    }
    if ((s_fieldMask & TELEMETRY_FIELD_SENSOR) != 0U)
    {
        bytes = (uint16_t)(bytes + 8U);   /* ',FF,255' 实为 7 字节，多留 1 字节余量 */
    }
    if ((s_fieldMask & TELEMETRY_FIELD_DISTANCE) != 0U)
    {
        bytes = (uint16_t)(bytes + 20U);  /* ',-999999.9,-999999.9' */
    }
    if ((s_fieldMask & TELEMETRY_FIELD_SPEED) != 0U)
    {
        bytes = (uint16_t)(bytes + 20U);  /* ',-999999.9,-999999.9' */
    }
    if ((s_fieldMask & TELEMETRY_FIELD_MODE) != 0U)
    {
        bytes = (uint16_t)(bytes + 9U);   /* ',STRAIGHT' */
    }
    if ((s_fieldMask & TELEMETRY_FIELD_K230) != 0U)
    {
        bytes = (uint16_t)(bytes + 16U);  /* ',1:-32768:-32768' */
    }
    if ((s_fieldMask & TELEMETRY_FIELD_TARGET) != 0U)
    {
        bytes = (uint16_t)(bytes + 20U);  /* ',-999999.9,-999999.9' */
    }
    if ((s_fieldMask & TELEMETRY_FIELD_PWM) != 0U)
    {
        bytes = (uint16_t)(bytes + 12U);  /* ',-1000,-1000' */
    }
    if ((s_fieldMask & TELEMETRY_FIELD_NAV) != 0U)
    {
        bytes = (uint16_t)(bytes + 22U);  /* ',-999999.99,-999999.99' */
    }
    if ((s_fieldMask & TELEMETRY_FIELD_LINE) != 0U)
    {
        bytes = (uint16_t)(bytes + 6U);   /* ',-6.0' 加 1 字节余量 */
    }
    /* 单侧字段各占一列，宽度与成对字段的一半相同。 */
    if ((s_fieldMask & TELEMETRY_FIELD_SPEED_L) != 0U)
    {
        bytes = (uint16_t)(bytes + 10U);  /* ',-999999.9' */
    }
    if ((s_fieldMask & TELEMETRY_FIELD_SPEED_R) != 0U)
    {
        bytes = (uint16_t)(bytes + 10U);
    }
    if ((s_fieldMask & TELEMETRY_FIELD_TARGET_L) != 0U)
    {
        bytes = (uint16_t)(bytes + 10U);
    }
    if ((s_fieldMask & TELEMETRY_FIELD_TARGET_R) != 0U)
    {
        bytes = (uint16_t)(bytes + 10U);
    }
    if ((s_fieldMask & TELEMETRY_FIELD_PWM_L) != 0U)
    {
        bytes = (uint16_t)(bytes + 6U);   /* ',-1000' */
    }
    if ((s_fieldMask & TELEMETRY_FIELD_PWM_R) != 0U)
    {
        bytes = (uint16_t)(bytes + 6U);
    }
    return bytes;
}

/* 根据当前字段掩码计算安全频率上限。
 * 纯整数运算，避免在 MCU 上引入浮点。
 * 公式：maxRate = (BYTES_PER_SEC * MAX_PERCENT) / (rowBytes * 100)
 * 结果夹紧到 1..TELEMETRY_RATE_HARD_LIMIT_HZ。 */
uint8_t Telemetry_GetMaxRateHz(void)
{
    uint32_t rowBytes = (uint32_t)Telemetry_EstimateRowBytes();
    uint32_t maxRate;

    maxRate = (TELEMETRY_UART_BYTES_PER_SECOND * TELEMETRY_MAX_BLOCKING_PERCENT)
              / (rowBytes * 100U);

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

static const char *Telemetry_ModeText(void)
{
    MotionManager_Mode_t mode = MotionManager_GetMode();

    /* 模式在动作完成后仍保留（例如 F 走完 mode 停在 STRAIGHT），
     * 上位机需要一个明确的"动作已结束"信号来自动收尾一次调参试验。 */
    if ((mode != MOTION_MANAGER_MODE_IDLE) &&
        (MotionManager_IsBusy() == 0U))
    {
        return "DONE";
    }

    switch (mode)
    {
        case MOTION_MANAGER_MODE_STRAIGHT: return "STRAIGHT";
        case MOTION_MANAGER_MODE_LINE:     return "LINE";
        case MOTION_MANAGER_MODE_TURN:     return "TURN";
        case MOTION_MANAGER_MODE_BRAKE:    return "BRAKE";
        case MOTION_MANAGER_MODE_SPEED:    return "SPEED";
        case MOTION_MANAGER_MODE_IDLE:     return "IDLE";
        default:                           return "ERROR";
    }
}

/* 表头与数据行的字段顺序必须完全一致，网页据表头解析后续数据行。 */
static void Telemetry_SendHeader(void)
{
    Serial1_SendString("H,ms");
    if ((s_fieldMask & TELEMETRY_FIELD_YAW) != 0U)
    {
        Serial1_SendString(",yaw");
    }
    if ((s_fieldMask & TELEMETRY_FIELD_SENSOR) != 0U)
    {
        Serial1_SendString(",gray,keys");
    }
    if ((s_fieldMask & TELEMETRY_FIELD_DISTANCE) != 0U)
    {
        Serial1_SendString(",LD,RD");
    }
    if ((s_fieldMask & TELEMETRY_FIELD_SPEED) != 0U)
    {
        Serial1_SendString(",LV,RV");
    }
    if ((s_fieldMask & TELEMETRY_FIELD_MODE) != 0U)
    {
        Serial1_SendString(",mode");
    }
    if ((s_fieldMask & TELEMETRY_FIELD_K230) != 0U)
    {
        Serial1_SendString(",k230");
    }
    if ((s_fieldMask & TELEMETRY_FIELD_TARGET) != 0U)
    {
        Serial1_SendString(",TL,TR");
    }
    if ((s_fieldMask & TELEMETRY_FIELD_PWM) != 0U)
    {
        Serial1_SendString(",PL,PR");
    }
    if ((s_fieldMask & TELEMETRY_FIELD_NAV) != 0U)
    {
        Serial1_SendString(",navT,navE");
    }
    if ((s_fieldMask & TELEMETRY_FIELD_LINE) != 0U)
    {
        Serial1_SendString(",lerr");
    }
    if ((s_fieldMask & TELEMETRY_FIELD_SPEED_L) != 0U)
    {
        Serial1_SendString(",LV");
    }
    if ((s_fieldMask & TELEMETRY_FIELD_SPEED_R) != 0U)
    {
        Serial1_SendString(",RV");
    }
    if ((s_fieldMask & TELEMETRY_FIELD_TARGET_L) != 0U)
    {
        Serial1_SendString(",TL");
    }
    if ((s_fieldMask & TELEMETRY_FIELD_TARGET_R) != 0U)
    {
        Serial1_SendString(",TR");
    }
    if ((s_fieldMask & TELEMETRY_FIELD_PWM_L) != 0U)
    {
        Serial1_SendString(",PL");
    }
    if ((s_fieldMask & TELEMETRY_FIELD_PWM_R) != 0U)
    {
        Serial1_SendString(",PR");
    }
    Serial1_SendString("\r\n");
}

static void Telemetry_SendRow(uint8_t pressedKeys)
{
    Serial1_Printf("D,%lu", (unsigned long)s_elapsedMs);

    if ((s_fieldMask & TELEMETRY_FIELD_YAW) != 0U)
    {
        Serial1_Printf(",%.2f", (double)Heading_GetYaw());
    }
    if ((s_fieldMask & TELEMETRY_FIELD_SENSOR) != 0U)
    {
        Serial1_Printf(",%02X,%u",
                       (unsigned)Graydetect_GetState(),
                       (unsigned)pressedKeys);
    }
    if ((s_fieldMask & TELEMETRY_FIELD_DISTANCE) != 0U)
    {
        Serial1_Printf(",%.1f,%.1f",
                       (double)Odometry_GetDistanceLMM(),
                       (double)Odometry_GetDistanceRMM());
    }
    if ((s_fieldMask & TELEMETRY_FIELD_SPEED) != 0U)
    {
        Serial1_Printf(",%.1f,%.1f",
                       (double)Odometry_GetSpeedL(),
                       (double)Odometry_GetSpeedR());
    }
    if ((s_fieldMask & TELEMETRY_FIELD_MODE) != 0U)
    {
        Serial1_Printf(",%s", Telemetry_ModeText());
    }
    if ((s_fieldMask & TELEMETRY_FIELD_K230) != 0U)
    {
        K230Link_Target_t target;

        if (K230Link_GetTarget(&target) != 0U)
        {
            Serial1_Printf(",%u:%d:%d",
                           (unsigned)target.valid,
                           (int)target.offsetX,
                           (int)target.offsetY);
        }
        else
        {
            Serial1_SendString(",0:0:0");
        }
    }
    if ((s_fieldMask & TELEMETRY_FIELD_TARGET) != 0U)
    {
        Serial1_Printf(",%.1f,%.1f",
                       (double)MotionWheel_GetTargetSpeedL(),
                       (double)MotionWheel_GetTargetSpeedR());
    }
    if ((s_fieldMask & TELEMETRY_FIELD_PWM) != 0U)
    {
        Serial1_Printf(",%.0f,%.0f",
                       (double)MotionWheel_GetLeftCommandPWM(),
                       (double)MotionWheel_GetRightCommandPWM());
    }
    if ((s_fieldMask & TELEMETRY_FIELD_NAV) != 0U)
    {
        Serial1_Printf(",%.2f,%.2f",
                       (double)Nav_GetTargetYawDeg(),
                       (double)Nav_GetAngleErrorDeg());
    }
    if ((s_fieldMask & TELEMETRY_FIELD_LINE) != 0U)
    {
        Serial1_Printf(",%.1f", (double)MotionLine_GetLineError());
    }
    if ((s_fieldMask & TELEMETRY_FIELD_SPEED_L) != 0U)
    {
        Serial1_Printf(",%.1f", (double)Odometry_GetSpeedL());
    }
    if ((s_fieldMask & TELEMETRY_FIELD_SPEED_R) != 0U)
    {
        Serial1_Printf(",%.1f", (double)Odometry_GetSpeedR());
    }
    if ((s_fieldMask & TELEMETRY_FIELD_TARGET_L) != 0U)
    {
        Serial1_Printf(",%.1f", (double)MotionWheel_GetTargetSpeedL());
    }
    if ((s_fieldMask & TELEMETRY_FIELD_TARGET_R) != 0U)
    {
        Serial1_Printf(",%.1f", (double)MotionWheel_GetTargetSpeedR());
    }
    if ((s_fieldMask & TELEMETRY_FIELD_PWM_L) != 0U)
    {
        Serial1_Printf(",%.0f", (double)MotionWheel_GetLeftCommandPWM());
    }
    if ((s_fieldMask & TELEMETRY_FIELD_PWM_R) != 0U)
    {
        Serial1_Printf(",%.0f", (double)MotionWheel_GetRightCommandPWM());
    }
    Serial1_SendString("\r\n");
}

void Telemetry_Init(void)
{
    uint8_t maxRate;

    s_fieldMask = TELEMETRY_FIELD_ALL;
    s_rateHz = TELEMETRY_DEFAULT_RATE_HZ;

    /* 默认频率也要服从当前掩码的安全上限。默认值现在合规（全字段 23 Hz > 20 Hz），
     * 但默认频率、默认掩码和波特率三者中任意一个被改动都可能让它越界，而 Init
     * 不经 SetRateHz()，越界不会有任何提示。这里显式夹一次，让上限只有一处定义。 */
    maxRate = Telemetry_GetMaxRateHz();
    if (s_rateHz > maxRate)
    {
        s_rateHz = maxRate;
    }

    s_tickAccumulator = 0U;
    s_headerPending = 1U;
    s_elapsedMs = 0U;
}

void Telemetry_Update(uint8_t elapsedTicks, uint8_t pressedKeys)
{
    uint8_t interval;

    s_elapsedMs += (uint32_t)elapsedTicks * 10U;

    if (s_rateHz == 0U)
    {
        return;
    }

    /* s_rateHz 由 SetRateHz 限制在 1..TELEMETRY_RATE_HARD_LIMIT_HZ（== TICK_HZ），
     * TICK_HZ / s_rateHz 最小为 1，interval == 0 分支不可能触发，已删除。 */
    interval = (uint8_t)(TELEMETRY_TICK_HZ / s_rateHz);

    s_tickAccumulator = (uint8_t)(s_tickAccumulator + elapsedTicks);
    if (s_tickAccumulator < interval)
    {
        return;
    }
    s_tickAccumulator = 0U;

    if (s_headerPending != 0U)
    {
        Telemetry_SendHeader();
        s_headerPending = 0U;
    }
    Telemetry_SendRow(pressedKeys);
}

uint8_t Telemetry_SetRateHz(uint8_t rateHz)
{
    /* 0 表示关闭遥测，合法；非零时不得超过当前掩码对应的安全上限。 */
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

    if ((mask == 0U) || (mask > TELEMETRY_FIELD_ALL))
    {
        return 0U;
    }
    s_fieldMask = mask;
    s_headerPending = 1U;  /* 掩码变化后必须重发表头。 */

    /* 掩码变化后行长可能增加，安全上限随之降低。若当前频率已超过新上限，
     * 自动降频而非报错——否则用户先设高频率再新增字段时会静默超限。
     * 先更新 s_fieldMask 再调用 GetMaxRateHz，确保上限基于新掩码计算。 */
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
