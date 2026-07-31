#include "Application/Control/MotionLine.h"
#include "Application/Control/MotionWheel.h"
#include "Application/Control/PID.h"
#include "Hardware/Sensors/Graydetect.h"
#include <math.h>

typedef struct
{
    MotionLine_State_t state;
    MotionLine_Error_t error;
    float requestedSpeedMMps;
    float profileSpeedMMps;
    float profileAccelerationMMps2;
    float accelerationMMps2;
    float decelerationMMps2;
    float lineError;
    float filteredError;
    uint16_t lostTicks;
    uint8_t configured;
    PID_t pid;
} MotionLine_Context_t;

float MotionLine_TuneKpMMpsPerWeight = MOTION_LINE_KP_MMPS_PER_WEIGHT;
float MotionLine_TuneKiMMpsPerWeight = MOTION_LINE_KI_MMPS_PER_WEIGHT;
float MotionLine_TuneKdMMpsPerWeight = MOTION_LINE_KD_MMPS_PER_WEIGHT;
float MotionLine_TuneAccelerationMMps2 = MOTION_LINE_ACCELERATION_MMPS2;
float MotionLine_TuneDecelerationMMps2 = MOTION_LINE_DECELERATION_MMPS2;

static MotionLine_Context_t s_context = {
    .state = MOTION_LINE_STATE_IDLE,
    .error = MOTION_LINE_ERROR_NONE,
};

static uint8_t MotionLine_ParametersAreValid(void)
{
    if ((!isfinite(MOTION_LINE_WEIGHT_FILTER_ALPHA)) ||
        (MOTION_LINE_WEIGHT_FILTER_ALPHA <= 0.0f) ||
        (MOTION_LINE_WEIGHT_FILTER_ALPHA > 1.0f) ||
        (MOTION_LINE_OUTER_WEIGHT <= 0) ||
        (MOTION_LINE_INNER_WEIGHT <= 0.0f) ||
        (MOTION_LINE_INNER_WEIGHT >= (float)MOTION_LINE_OUTER_WEIGHT) ||
        (MOTION_LINE_LOST_CONFIRM_TICKS == 0U))
    {
        return 0U;
    }
    return 1U;
}

static uint8_t MotionLine_TuningsAreValid(void)
{
    if ((!isfinite(MotionLine_TuneKpMMpsPerWeight)) ||
        (!isfinite(MotionLine_TuneKiMMpsPerWeight)) ||
        (!isfinite(MotionLine_TuneKdMMpsPerWeight)) ||
        (!isfinite(MotionLine_TuneAccelerationMMps2)) ||
        (!isfinite(MotionLine_TuneDecelerationMMps2)))
    {
        return 0U;
    }

    if ((MotionLine_TuneKpMMpsPerWeight < 0.0f) ||
        (MotionLine_TuneKiMMpsPerWeight < 0.0f) ||
        (MotionLine_TuneKdMMpsPerWeight < 0.0f) ||
        ((MotionLine_TuneKpMMpsPerWeight == 0.0f) &&
         (MotionLine_TuneKiMMpsPerWeight == 0.0f) &&
         (MotionLine_TuneKdMMpsPerWeight == 0.0f)) ||
        (MotionLine_TuneAccelerationMMps2 <= 0.0f) ||
        (MotionLine_TuneDecelerationMMps2 <= 0.0f))
    {
        return 0U;
    }
    return 1U;
}

static uint8_t MotionLine_SnapshotTunings(void)
{
    if (MotionLine_TuningsAreValid() == 0U)
    {
        return 0U;
    }

    s_context.accelerationMMps2 = MotionLine_TuneAccelerationMMps2;
    s_context.decelerationMMps2 = MotionLine_TuneDecelerationMMps2;
    PID_Init(&s_context.pid,
             MotionLine_TuneKpMMpsPerWeight,
             MotionLine_TuneKiMMpsPerWeight,
             MotionLine_TuneKdMMpsPerWeight,
             0.0f, 0.0f);
    return 1U;
}

static float MotionLine_Approach(
    float current, float target, float maximumStep)
{
    if (current < target)
    {
        current += maximumStep;
        return (current > target) ? target : current;
    }
    if (current > target)
    {
        current -= maximumStep;
        return (current < target) ? target : current;
    }
    return current;
}

static void MotionLine_ResetControl(void)
{
    s_context.requestedSpeedMMps = 0.0f;
    s_context.profileSpeedMMps = 0.0f;
    s_context.profileAccelerationMMps2 = 0.0f;
    s_context.lineError = 0.0f;
    s_context.filteredError = 0.0f;
    s_context.lostTicks = 0U;
    PID_Reset(&s_context.pid);
}

static void MotionLine_SetError(MotionLine_Error_t error)
{
    MotionWheel_Stop();
    MotionLine_ResetControl();
    s_context.error = error;
    s_context.state = MOTION_LINE_STATE_ERROR;
}

#if GRAYDETECT_CHANNEL1_IS_RIGHT
static const float s_grayWeight[GRAY_CHANNEL_COUNT] = {
     MOTION_LINE_OUTER_WEIGHT, 4.0f, MOTION_LINE_INNER_WEIGHT,
    -MOTION_LINE_INNER_WEIGHT, -4.0f, -MOTION_LINE_OUTER_WEIGHT
};
#else
static const float s_grayWeight[GRAY_CHANNEL_COUNT] = {
    -MOTION_LINE_OUTER_WEIGHT, -4.0f, -MOTION_LINE_INNER_WEIGHT,
     MOTION_LINE_INNER_WEIGHT, 4.0f, MOTION_LINE_OUTER_WEIGHT
};
#endif

static float MotionLine_GetWeightedError(uint8_t grayState)
{
    float weightedSum = 0.0f;
    uint8_t activeCount = 0U;
    uint8_t index;

    for (index = 0U; index < GRAY_CHANNEL_COUNT; index++)
    {
        if ((grayState & (uint8_t)(1U << index)) != 0U)
        {
            weightedSum += s_grayWeight[index];
            activeCount++;
        }
    }
    return (activeCount == 0U) ? 0.0f : weightedSum / (float)activeCount;
}

static float MotionLine_UpdateProfileSpeed(float targetSpeedMMps, float dt)
{
    float maximumStep = (targetSpeedMMps > s_context.profileSpeedMMps) ?
        (s_context.accelerationMMps2 * dt) :
        (s_context.decelerationMMps2 * dt);
    float previousSpeedMMps = s_context.profileSpeedMMps;

    s_context.profileSpeedMMps = MotionLine_Approach(
        s_context.profileSpeedMMps, targetSpeedMMps, maximumStep);
    s_context.profileAccelerationMMps2 =
        (s_context.profileSpeedMMps - previousSpeedMMps) / dt;
    return s_context.profileSpeedMMps;
}

static uint8_t MotionLine_CalculateTargetSpeeds(
    float *leftSpeedMMps, float *rightSpeedMMps, float dt)
{
    uint8_t grayState = Graydetect_GetState();
    float rawError;
    float speedAdjustMMps;

    if (grayState == 0U)
    {
        if (s_context.lostTicks < MOTION_LINE_LOST_CONFIRM_TICKS)
        {
            s_context.lostTicks++;
        }
        if (s_context.lostTicks >= MOTION_LINE_LOST_CONFIRM_TICKS)
        {
            return 0U;
        }
        rawError = s_context.filteredError;
    }
    else
    {
        s_context.lostTicks = 0U;
        rawError = MotionLine_GetWeightedError(grayState);
    }

    s_context.filteredError += MOTION_LINE_WEIGHT_FILTER_ALPHA *
        (rawError - s_context.filteredError);
    s_context.lineError = s_context.filteredError;

    (void)MotionLine_UpdateProfileSpeed(s_context.requestedSpeedMMps, dt);
    speedAdjustMMps = PID_Update(
        &s_context.pid, s_context.lineError, 0.0f, dt);

    *leftSpeedMMps = s_context.profileSpeedMMps + speedAdjustMMps;
    *rightSpeedMMps = s_context.profileSpeedMMps - speedAdjustMMps;
    return 1U;
}

static MotionWheel_Result_t MotionLine_ApplyWheelCommand(
    float leftSpeedMMps, float rightSpeedMMps, float dt)
{
    MotionWheel_Command_t command;

    command.targetSpeedLMMps = leftSpeedMMps;
    command.targetSpeedRMMps = rightSpeedMMps;
    command.trimLPWM = 0.0f;
    command.trimRPWM = 0.0f;
    return MotionWheel_Update(&command, dt);
}

MotionLine_Result_t MotionLine_Init(void)
{
    MotionWheel_Result_t wheelResult;

    s_context.configured = 0U;
    s_context.state = MOTION_LINE_STATE_IDLE;
    s_context.error = MOTION_LINE_ERROR_NONE;
    MotionLine_ResetControl();

    wheelResult = MotionWheel_Init();
    if ((wheelResult != MOTION_WHEEL_RESULT_OK) ||
        (MotionLine_ParametersAreValid() == 0U) ||
        (MotionLine_TuningsAreValid() == 0U))
    {
        return MOTION_LINE_RESULT_INVALID_ARGUMENT;
    }

    s_context.configured = 1U;
    return MOTION_LINE_RESULT_OK;
}

MotionLine_Result_t MotionLine_Start(float speedMMps)
{
    if (s_context.configured == 0U)
    {
        return MOTION_LINE_RESULT_NOT_CONFIGURED;
    }
    if (MotionLine_IsBusy() != 0U)
    {
        return MOTION_LINE_RESULT_BUSY;
    }
    if ((!isfinite(speedMMps)) || (speedMMps <= 0.0f))
    {
        return MOTION_LINE_RESULT_INVALID_ARGUMENT;
    }
    if (MotionLine_SnapshotTunings() == 0U)
    {
        return MOTION_LINE_RESULT_INVALID_ARGUMENT;
    }

    MotionWheel_Stop();
    MotionLine_ResetControl();
    s_context.requestedSpeedMMps = speedMMps;
    s_context.error = MOTION_LINE_ERROR_NONE;
    s_context.state = MOTION_LINE_STATE_RUNNING;
    return MOTION_LINE_RESULT_OK;
}

MotionLine_Result_t MotionLine_SetSpeed(float speedMMps)
{
    if (s_context.configured == 0U)
    {
        return MOTION_LINE_RESULT_NOT_CONFIGURED;
    }
    if (MotionLine_IsBusy() == 0U)
    {
        return MOTION_LINE_RESULT_BUSY;
    }
    if ((!isfinite(speedMMps)) || (speedMMps < 0.0f))
    {
        return MOTION_LINE_RESULT_INVALID_ARGUMENT;
    }

    s_context.requestedSpeedMMps = speedMMps;
    return MOTION_LINE_RESULT_OK;
}

MotionLine_Result_t MotionLine_RequestStop(void)
{
    return MotionLine_SetSpeed(0.0f);
}

void MotionLine_Update(float dt)
{
    float leftSpeedMMps;
    float rightSpeedMMps;

    if (s_context.state != MOTION_LINE_STATE_RUNNING)
    {
        return;
    }
    if ((!isfinite(dt)) || (dt <= 0.0f))
    {
        MotionLine_SetError(MOTION_LINE_ERROR_UPDATE_PERIOD_INVALID);
        return;
    }
    if (MotionLine_CalculateTargetSpeeds(
            &leftSpeedMMps, &rightSpeedMMps, dt) == 0U)
    {
        MotionWheel_Stop();
        s_context.error = MOTION_LINE_ERROR_NONE;
        s_context.state = MOTION_LINE_STATE_FINISHED;
        return;
    }
    if ((s_context.requestedSpeedMMps <= 0.001f) &&
        (s_context.profileSpeedMMps <= 0.001f))
    {
        MotionWheel_Stop();
        s_context.error = MOTION_LINE_ERROR_NONE;
        s_context.state = MOTION_LINE_STATE_FINISHED;
        return;
    }
    if (MotionLine_ApplyWheelCommand(
            leftSpeedMMps, rightSpeedMMps, dt) != MOTION_WHEEL_RESULT_OK)
    {
        MotionLine_SetError(MOTION_LINE_ERROR_WHEEL);
    }
}

void MotionLine_Stop(void)
{
    MotionWheel_Stop();
    MotionLine_ResetControl();
    s_context.error = MOTION_LINE_ERROR_NONE;
    s_context.state = MOTION_LINE_STATE_IDLE;
}

uint8_t MotionLine_IsConfigured(void)
{
    return s_context.configured;
}

uint8_t MotionLine_IsBusy(void)
{
    return (s_context.state == MOTION_LINE_STATE_RUNNING) ? 1U : 0U;
}

uint8_t MotionLine_IsFinished(void)
{
    return (s_context.state == MOTION_LINE_STATE_FINISHED) ? 1U : 0U;
}

MotionLine_State_t MotionLine_GetState(void)
{
    return s_context.state;
}

MotionLine_Error_t MotionLine_GetError(void)
{
    return s_context.error;
}

float MotionLine_GetLineError(void)
{
    return s_context.lineError;
}

float MotionLine_GetProfileSpeedMMps(void)
{
    return s_context.profileSpeedMMps;
}

float MotionLine_GetProfileAccelerationMMps2(void)
{
    return s_context.profileAccelerationMMps2;
}
