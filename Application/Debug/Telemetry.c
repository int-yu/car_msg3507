#include "Application/Debug/Telemetry.h"
#include "Application/Comms/K230Link.h"
#include "Application/Control/MotionManager.h"
#include "Application/State/Heading.h"
#include "Application/State/Odometry.h"
#include "Hardware/Comms/Serial.h"
#include "Hardware/Sensors/Graydetect.h"

/* 100 Hz 主循环下，每 (100 / rateHz) 拍输出一行。 */
#define TELEMETRY_TICK_HZ 100U

static uint8_t s_rateHz;
static uint8_t s_fieldMask;
static uint8_t s_tickAccumulator;
static uint8_t s_headerPending;
static uint32_t s_elapsedMs;

static const char *Telemetry_ModeText(void)
{
    switch (MotionManager_GetMode())
    {
        case MOTION_MANAGER_MODE_STRAIGHT: return "STRAIGHT";
        case MOTION_MANAGER_MODE_LINE:     return "LINE";
        case MOTION_MANAGER_MODE_TURN:     return "TURN";
        case MOTION_MANAGER_MODE_BRAKE:    return "BRAKE";
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
    Serial1_SendString("\r\n");
}

void Telemetry_Init(void)
{
    s_rateHz = TELEMETRY_DEFAULT_RATE_HZ;
    s_fieldMask = TELEMETRY_FIELD_ALL;
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

    interval = (uint8_t)(TELEMETRY_TICK_HZ / s_rateHz);
    if (interval == 0U)
    {
        interval = 1U;
    }

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
    if (rateHz > TELEMETRY_MAX_RATE_HZ)
    {
        return 0U;
    }
    s_rateHz = rateHz;
    s_tickAccumulator = 0U;
    return 1U;
}

uint8_t Telemetry_SetFieldMask(uint8_t mask)
{
    if ((mask == 0U) || (mask > TELEMETRY_FIELD_ALL))
    {
        return 0U;
    }
    s_fieldMask = mask;
    s_headerPending = 1U;  /* 掩码变化后必须重发表头。 */
    return 1U;
}

uint8_t Telemetry_GetRateHz(void)
{
    return s_rateHz;
}

uint8_t Telemetry_GetFieldMask(void)
{
    return s_fieldMask;
}
