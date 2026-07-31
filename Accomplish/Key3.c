#include "Key3.h"
#include "Application/Control/LineFollow.h"
#include "Application/Control/Nav.h"
#include "Application/Control/Motion.h"
#include "Application/Control/Pose.h"
#include "Application/Core/App.h"
#include "Application/Sensor/ADC_Gray.h"
#include "Application/Sensor/Encoder.h"
#include <stddef.h>

/* 实时可调参数，启动时从宏初始化 */
float AccomplishKey3_TuneAccelerationMMps2 = ACCOMPLISH_KEY3_ACCELERATION_MMPS2;
float AccomplishKey3_TuneCruiseSpeedMMps = ACCOMPLISH_KEY3_CRUISE_SPEED_MMPS;
uint32_t AccomplishKey3_TuneRunDurationTicks = ACCOMPLISH_KEY3_RUN_DURATION_TICKS;

static AccomplishKey3_State_t s_state = ACCOMPLISH_KEY3_STATE_READY;
static AccomplishKey3_Error_t s_error = ACCOMPLISH_KEY3_ERROR_NONE;
static uint32_t s_tickCounter = 0U;
static uint32_t s_settleCounter = 0U;

/* 启动时记录的初始位姿，用于里程计算 */
static float s_startX = 0.0f;
static float s_startY = 0.0f;

void AccomplishKey3_Init(void)
{
    s_state = ACCOMPLISH_KEY3_STATE_READY;
    s_error = ACCOMPLISH_KEY3_ERROR_NONE;
    s_tickCounter = 0U;
    s_settleCounter = 0U;
    s_startX = 0.0f;
    s_startY = 0.0f;

    /* 恢复默认值 */
    AccomplishKey3_TuneAccelerationMMps2 = ACCOMPLISH_KEY3_ACCELERATION_MMPS2;
    AccomplishKey3_TuneCruiseSpeedMMps = ACCOMPLISH_KEY3_CRUISE_SPEED_MMPS;
    AccomplishKey3_TuneRunDurationTicks = ACCOMPLISH_KEY3_RUN_DURATION_TICKS;
}

void AccomplishKey3_Update(uint8_t keys)
{
    const uint8_t grayBits = ADC_Gray_Read();
    const float speedL = Encoder_GetSpeedLeftMMps();
    const float speedR = Encoder_GetSpeedRightMMps();
    const float avgSpeed = (speedL + speedR) * 0.5f;

    switch (s_state)
    {
    case ACCOMPLISH_KEY3_STATE_READY:
        if ((keys & ACCOMPLISH_KEY3_START_KEY_MASK) != 0U)
        {
            /* 检查灰度传感器 */
            if (grayBits == 0xFFU)
            {
                s_error = ACCOMPLISH_KEY3_ERROR_SENSOR_OFFLINE;
                s_state = ACCOMPLISH_KEY3_STATE_ERROR;
                break;
            }

            /* 记录初始位姿 */
            s_startX = Pose_GetXMM();
            s_startY = Pose_GetYMM();
            s_tickCounter = 0U;

            /* 启动巡线，使用可调参数 */
            LineFollow_Start(grayBits);
            Motion_SetAccelerationMMps2(AccomplishKey3_TuneAccelerationMMps2);
            Motion_SetTargetSpeedMMps(AccomplishKey3_TuneCruiseSpeedMMps);

            s_state = ACCOMPLISH_KEY3_STATE_RUNNING;
        }
        break;

    case ACCOMPLISH_KEY3_STATE_RUNNING:
        /* 应急停止检测 */
        if ((keys & ACCOMPLISH_KEY3_EMERGENCY_STOP_KEY_MASK) == ACCOMPLISH_KEY3_EMERGENCY_STOP_KEY_MASK)
        {
            Motion_SetTargetSpeedMMps(0.0f);
            s_error = ACCOMPLISH_KEY3_ERROR_START;
            s_state = ACCOMPLISH_KEY3_STATE_ERROR;
            break;
        }

        /* 超时检测 */
        if (s_tickCounter >= ACCOMPLISH_KEY3_MAX_RUN_TICKS)
        {
            Motion_SetTargetSpeedMMps(0.0f);
            s_error = ACCOMPLISH_KEY3_ERROR_TIMEOUT;
            s_state = ACCOMPLISH_KEY3_STATE_ERROR;
            break;
        }

        /* 时长到达，开始软停 */
        if (s_tickCounter >= AccomplishKey3_TuneRunDurationTicks)
        {
            Motion_SetTargetSpeedMMps(0.0f);
            s_settleCounter = 0U;
            s_state = ACCOMPLISH_KEY3_STATE_SOFT_STOP;
            break;
        }

        /* 持续巡线 */
        LineFollow_Update(grayBits);
        s_tickCounter++;
        break;

    case ACCOMPLISH_KEY3_STATE_SOFT_STOP:
        /* 等待速度降低 */
        if ((avgSpeed < ACCOMPLISH_KEY3_SETTLE_SPEED_MMPS) &&
            (avgSpeed > -ACCOMPLISH_KEY3_SETTLE_SPEED_MMPS))
        {
            s_settleCounter++;
            if (s_settleCounter >= ACCOMPLISH_KEY3_SETTLE_CONFIRM_TICKS)
            {
                s_state = ACCOMPLISH_KEY3_STATE_SETTLING;
                s_settleCounter = 0U;
            }
        }
        else
        {
            s_settleCounter = 0U;
        }

        /* 软停超时 */
        if (s_tickCounter >= ACCOMPLISH_KEY3_MAX_RUN_TICKS)
        {
            s_error = ACCOMPLISH_KEY3_ERROR_MOTION;
            s_state = ACCOMPLISH_KEY3_STATE_ERROR;
        }

        s_tickCounter++;
        break;

    case ACCOMPLISH_KEY3_STATE_SETTLING:
        /* 保持零速，等待稳定 */
        s_settleCounter++;
        if (s_settleCounter >= ACCOMPLISH_KEY3_SETTLE_TIMEOUT_TICKS)
        {
            s_state = ACCOMPLISH_KEY3_STATE_FINISHED;
        }
        break;

    case ACCOMPLISH_KEY3_STATE_FINISHED:
    case ACCOMPLISH_KEY3_STATE_ERROR:
    default:
        /* 保持状态，等待手动复位 */
        break;
    }
}

AccomplishKey3_State_t AccomplishKey3_GetState(void)
{
    return s_state;
}

AccomplishKey3_Error_t AccomplishKey3_GetError(void)
{
    return s_error;
}

uint32_t AccomplishKey3_GetElapsedTicks(void)
{
    return s_tickCounter;
}