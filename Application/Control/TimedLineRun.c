#include "Application/Control/TimedLineRun.h"
#include "Application/Control/MotionLine.h"
#include "Application/Control/MotionManager.h"
#include "Application/Control/TaskTimer.h"
#include "Application/State/Odometry.h"
#include "Hardware/Sensors/Graydetect.h"
#include "System/Tick.h"
#include <limits.h>
#include <math.h>
#include <stddef.h>

float TimedLineRun_TuneAccelerationMMps2 =
    TIMED_LINE_RUN_ACCELERATION_MMPS2;
float TimedLineRun_TuneCruiseSpeedMMps =
    TIMED_LINE_RUN_CRUISE_SPEED_MMPS;
float TimedLineRun_TuneDurationSeconds =
    TIMED_LINE_RUN_DURATION_SECONDS;

static TimedLineRun_State_t s_state;
static TimedLineRun_Error_t s_error;
static uint32_t s_elapsedTicks;
static uint32_t s_runDurationTicks;
static uint16_t s_settleTicks;
static uint16_t s_settleElapsedTicks;

static uint16_t TimedLineRun_AddSaturating16(
    uint16_t current, uint8_t elapsedTicks)
{
    if (((uint32_t)current + (uint32_t)elapsedTicks) > UINT16_MAX)
    {
        return UINT16_MAX;
    }
    return (uint16_t)(current + elapsedTicks);
}

static void TimedLineRun_AddElapsed(uint8_t elapsedTicks)
{
    if ((UINT32_MAX - s_elapsedTicks) < elapsedTicks)
    {
        s_elapsedTicks = UINT32_MAX;
    }
    else
    {
        s_elapsedTicks += elapsedTicks;
    }
}

static void TimedLineRun_Fail(TimedLineRun_Error_t error)
{
    MotionManager_Stop();
    TaskTimer_Stop(TASK_TIMER_OWNER_LINE);
    s_error = error;
    s_state = TIMED_LINE_RUN_STATE_ERROR;
}

static uint8_t TimedLineRun_LineIsHealthy(void)
{
    if (Graydetect_IsOnline() == 0U)
    {
        TimedLineRun_Fail(TIMED_LINE_RUN_ERROR_SENSOR_OFFLINE);
        return 0U;
    }
    if ((MotionManager_GetError() != MOTION_MANAGER_ERROR_NONE) ||
        (MotionManager_GetMode() != MOTION_MANAGER_MODE_LINE) ||
        (MotionManager_IsBusy() == 0U))
    {
        TimedLineRun_Fail(TIMED_LINE_RUN_ERROR_MOTION);
        return 0U;
    }
    return 1U;
}

static void TimedLineRun_BeginSoftStop(void)
{
    TaskTimer_Stop(TASK_TIMER_OWNER_LINE);
    if (MotionManager_RequestLineStop() != MOTION_MANAGER_RESULT_OK)
    {
        TimedLineRun_Fail(TIMED_LINE_RUN_ERROR_MOTION);
        return;
    }

    s_settleTicks = 0U;
    s_settleElapsedTicks = 0U;
    s_state = TIMED_LINE_RUN_STATE_SOFT_STOP;
}

void TimedLineRun_Init(void)
{
    s_state = TIMED_LINE_RUN_STATE_READY;
    s_error = TIMED_LINE_RUN_ERROR_NONE;
    s_elapsedTicks = 0U;
    s_runDurationTicks = 0U;
    s_settleTicks = 0U;
    s_settleElapsedTicks = 0U;
}

uint8_t TimedLineRun_Start(void)
{
    float durationTicks =
        TimedLineRun_TuneDurationSeconds * (float)TICK_HZ;
    float savedAccelerationMMps2;
    MotionManager_Result_t result;

    if ((TimedLineRun_CanStart() == 0U) ||
        (MotionManager_IsBusy() != 0U))
    {
        return 0U;
    }
    if ((!isfinite(TimedLineRun_TuneAccelerationMMps2)) ||
        (!isfinite(TimedLineRun_TuneCruiseSpeedMMps)) ||
        (!isfinite(TimedLineRun_TuneDurationSeconds)) ||
        (!isfinite(durationTicks)) ||
        (TimedLineRun_TuneAccelerationMMps2 <= 0.0f) ||
        (TimedLineRun_TuneCruiseSpeedMMps <= 0.0f) ||
        (TimedLineRun_TuneDurationSeconds <= 0.0f) ||
        (durationTicks < 1.0f) ||
        (durationTicks > (float)UINT32_MAX))
    {
        s_error = TIMED_LINE_RUN_ERROR_START;
        s_state = TIMED_LINE_RUN_STATE_ERROR;
        return 0U;
    }
    if (Graydetect_IsOnline() == 0U)
    {
        s_error = TIMED_LINE_RUN_ERROR_SENSOR_OFFLINE;
        s_state = TIMED_LINE_RUN_STATE_ERROR;
        return 0U;
    }

    /* MotionLine 在 Start 内快照斜坡参数；恢复 lacc 后不会影响 KEY1。 */
    savedAccelerationMMps2 = MotionLine_TuneAccelerationMMps2;
    MotionLine_TuneAccelerationMMps2 =
        TimedLineRun_TuneAccelerationMMps2;
    result = MotionManager_StartLine(TimedLineRun_TuneCruiseSpeedMMps);
    MotionLine_TuneAccelerationMMps2 = savedAccelerationMMps2;
    if (result != MOTION_MANAGER_RESULT_OK)
    {
        MotionManager_Stop();
        s_error = TIMED_LINE_RUN_ERROR_START;
        s_state = TIMED_LINE_RUN_STATE_ERROR;
        return 0U;
    }

    s_runDurationTicks = (uint32_t)(durationTicks + 0.5f);
    s_elapsedTicks = 0U;
    s_settleTicks = 0U;
    s_settleElapsedTicks = 0U;
    s_error = TIMED_LINE_RUN_ERROR_NONE;
    s_state = TIMED_LINE_RUN_STATE_RUNNING;
    TaskTimer_Start(TASK_TIMER_OWNER_LINE);
    return 1U;
}

void TimedLineRun_Update(const App_UpdateContext_t *context)
{
    if ((context == NULL) || (TimedLineRun_IsActive() == 0U))
    {
        return;
    }

    if (((context->pressedKeys & TIMED_LINE_RUN_EMERGENCY_KEY_MASK) ==
         TIMED_LINE_RUN_EMERGENCY_KEY_MASK) ||
        ((context->hasBluetoothSignal != 0U) &&
         (context->bluetoothSignal == 0U)))
    {
        TimedLineRun_Fail(TIMED_LINE_RUN_ERROR_EMERGENCY_STOP);
        return;
    }

    if (s_state == TIMED_LINE_RUN_STATE_RUNNING)
    {
        TimedLineRun_AddElapsed(context->elapsedTicks);
        if (s_elapsedTicks >= s_runDurationTicks)
        {
            TimedLineRun_BeginSoftStop();
            return;
        }
        (void)TimedLineRun_LineIsHealthy();
        return;
    }

    if (s_state == TIMED_LINE_RUN_STATE_SOFT_STOP)
    {
        if (MotionManager_GetError() != MOTION_MANAGER_ERROR_NONE)
        {
            TimedLineRun_Fail(TIMED_LINE_RUN_ERROR_MOTION);
        }
        else if (MotionManager_IsFinished() != 0U)
        {
            s_state = TIMED_LINE_RUN_STATE_SETTLING;
        }
        else if (MotionManager_IsBusy() == 0U)
        {
            TimedLineRun_Fail(TIMED_LINE_RUN_ERROR_MOTION);
        }
        return;
    }

    if (s_state == TIMED_LINE_RUN_STATE_SETTLING)
    {
        float averageSpeedMMps =
            (fabsf(Odometry_GetSpeedL()) + fabsf(Odometry_GetSpeedR())) *
            0.5f;

        if (averageSpeedMMps <= TIMED_LINE_RUN_SETTLE_SPEED_MMPS)
        {
            s_settleTicks = TimedLineRun_AddSaturating16(
                s_settleTicks, context->elapsedTicks);
        }
        else
        {
            s_settleTicks = 0U;
        }
        s_settleElapsedTicks = TimedLineRun_AddSaturating16(
            s_settleElapsedTicks, context->elapsedTicks);

        if (s_settleTicks >= TIMED_LINE_RUN_SETTLE_CONFIRM_TICKS)
        {
            s_state = TIMED_LINE_RUN_STATE_FINISHED;
        }
        else if (s_settleElapsedTicks >=
                 TIMED_LINE_RUN_SETTLE_TIMEOUT_TICKS)
        {
            s_error = TIMED_LINE_RUN_ERROR_SETTLE_TIMEOUT;
            s_state = TIMED_LINE_RUN_STATE_ERROR;
        }
    }
}

uint8_t TimedLineRun_IsActive(void)
{
    return ((s_state == TIMED_LINE_RUN_STATE_RUNNING) ||
            (s_state == TIMED_LINE_RUN_STATE_SOFT_STOP) ||
            (s_state == TIMED_LINE_RUN_STATE_SETTLING)) ? 1U : 0U;
}

uint8_t TimedLineRun_CanStart(void)
{
    return ((s_state == TIMED_LINE_RUN_STATE_READY) ||
            (s_state == TIMED_LINE_RUN_STATE_FINISHED) ||
            (s_state == TIMED_LINE_RUN_STATE_ERROR)) ? 1U : 0U;
}

uint32_t TimedLineRun_GetElapsedTicks(void)
{
    return s_elapsedTicks;
}

TimedLineRun_State_t TimedLineRun_GetState(void)
{
    return s_state;
}

TimedLineRun_Error_t TimedLineRun_GetError(void)
{
    return s_error;
}
