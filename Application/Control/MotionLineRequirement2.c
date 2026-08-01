#include "Application/Control/MotionLineRequirement2.h"
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
    float rightTurnRightAdjustRatio;
    float lineError;
    float filteredError;
    uint16_t lostTicks;
    uint8_t configured;
    PID_t pid;
} MotionLineRequirement2_Context_t;

float MotionLineRequirement2_TuneKpMMpsPerWeight =
    MOTION_LINE_REQUIREMENT2_KP_MMPS_PER_WEIGHT;
float MotionLineRequirement2_TuneKiMMpsPerWeight =
    MOTION_LINE_REQUIREMENT2_KI_MMPS_PER_WEIGHT;
float MotionLineRequirement2_TuneKdMMpsPerWeight =
    MOTION_LINE_REQUIREMENT2_KD_MMPS_PER_WEIGHT;
float MotionLineRequirement2_TuneAccelerationMMps2 =
    MOTION_LINE_REQUIREMENT2_ACCELERATION_MMPS2;
float MotionLineRequirement2_TuneDecelerationMMps2 =
    MOTION_LINE_REQUIREMENT2_DECELERATION_MMPS2;
float MotionLineRequirement2_TuneRightTurnRightAdjustRatio =
    MOTION_LINE_REQUIREMENT2_RIGHT_TURN_RIGHT_ADJUST_RATIO;

static MotionLineRequirement2_Context_t s_context = {
    .state = MOTION_LINE_STATE_IDLE,
    .error = MOTION_LINE_ERROR_NONE,
};

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

static uint8_t MotionLineRequirement2_TuningsAreValid(void)
{
    return (isfinite(MotionLineRequirement2_TuneKpMMpsPerWeight) &&
            isfinite(MotionLineRequirement2_TuneKiMMpsPerWeight) &&
            isfinite(MotionLineRequirement2_TuneKdMMpsPerWeight) &&
            isfinite(MotionLineRequirement2_TuneAccelerationMMps2) &&
            isfinite(MotionLineRequirement2_TuneDecelerationMMps2) &&
            isfinite(MotionLineRequirement2_TuneRightTurnRightAdjustRatio) &&
            (MotionLineRequirement2_TuneKpMMpsPerWeight >= 0.0f) &&
            (MotionLineRequirement2_TuneKiMMpsPerWeight >= 0.0f) &&
            (MotionLineRequirement2_TuneKdMMpsPerWeight >= 0.0f) &&
            ((MotionLineRequirement2_TuneKpMMpsPerWeight > 0.0f) ||
             (MotionLineRequirement2_TuneKiMMpsPerWeight > 0.0f) ||
             (MotionLineRequirement2_TuneKdMMpsPerWeight > 0.0f)) &&
            (MotionLineRequirement2_TuneAccelerationMMps2 > 0.0f) &&
            (MotionLineRequirement2_TuneDecelerationMMps2 > 0.0f) &&
            (MotionLineRequirement2_TuneRightTurnRightAdjustRatio > 0.0f) &&
            (MOTION_LINE_REQUIREMENT2_LOST_CONFIRM_TICKS > 0U)) ? 1U : 0U;
}

static float MotionLineRequirement2_Approach(
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

static void MotionLineRequirement2_ResetControl(void)
{
    s_context.requestedSpeedMMps = 0.0f;
    s_context.profileSpeedMMps = 0.0f;
    s_context.profileAccelerationMMps2 = 0.0f;
    s_context.lineError = 0.0f;
    s_context.filteredError = 0.0f;
    s_context.lostTicks = 0U;
    PID_Reset(&s_context.pid);
}

static float MotionLineRequirement2_GetWeightedError(uint8_t grayState)
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

static uint8_t MotionLineRequirement2_CalculateTargetSpeeds(
    float *leftSpeedMMps, float *rightSpeedMMps, float dt)
{
    uint8_t grayState = Graydetect_GetState();
    float rawError;
    float maximumStep;
    float previousSpeedMMps = s_context.profileSpeedMMps;
    float speedAdjustMMps;

    if (grayState == 0U)
    {
        if (s_context.lostTicks < MOTION_LINE_REQUIREMENT2_LOST_CONFIRM_TICKS)
        {
            s_context.lostTicks++;
        }
        if (s_context.lostTicks >=
            MOTION_LINE_REQUIREMENT2_LOST_CONFIRM_TICKS)
        {
            return 0U;
        }
        rawError = s_context.filteredError;
    }
    else
    {
        s_context.lostTicks = 0U;
        rawError = MotionLineRequirement2_GetWeightedError(grayState);
    }

    s_context.filteredError += MOTION_LINE_WEIGHT_FILTER_ALPHA *
        (rawError - s_context.filteredError);
    s_context.lineError = s_context.filteredError;
    maximumStep = ((s_context.requestedSpeedMMps >
                    s_context.profileSpeedMMps) ?
        s_context.accelerationMMps2 : s_context.decelerationMMps2) * dt;
    s_context.profileSpeedMMps = MotionLineRequirement2_Approach(
        s_context.profileSpeedMMps, s_context.requestedSpeedMMps,
        maximumStep);
    s_context.profileAccelerationMMps2 =
        (s_context.profileSpeedMMps - previousSpeedMMps) / dt;
    speedAdjustMMps = PID_Update(
        &s_context.pid, s_context.lineError, 0.0f, dt);
    *leftSpeedMMps = s_context.profileSpeedMMps + speedAdjustMMps;
    *rightSpeedMMps = s_context.profileSpeedMMps -
        ((speedAdjustMMps > 0.0f) ?
         (speedAdjustMMps * s_context.rightTurnRightAdjustRatio) :
         speedAdjustMMps);
    return 1U;
}

MotionLine_Result_t MotionLineRequirement2_Init(void)
{
    s_context.configured = 0U;
    s_context.state = MOTION_LINE_STATE_IDLE;
    s_context.error = MOTION_LINE_ERROR_NONE;
    MotionLineRequirement2_ResetControl();
    if ((MotionWheel_Init() != MOTION_WHEEL_RESULT_OK) ||
        (MotionLineRequirement2_TuningsAreValid() == 0U))
    {
        return MOTION_LINE_RESULT_INVALID_ARGUMENT;
    }
    s_context.configured = 1U;
    return MOTION_LINE_RESULT_OK;
}

MotionLine_Result_t MotionLineRequirement2_Start(float speedMMps)
{
    if (s_context.configured == 0U)
    {
        return MOTION_LINE_RESULT_NOT_CONFIGURED;
    }
    if (MotionLineRequirement2_IsBusy() != 0U)
    {
        return MOTION_LINE_RESULT_BUSY;
    }
    if ((!isfinite(speedMMps)) || (speedMMps <= 0.0f) ||
        (MotionLineRequirement2_TuningsAreValid() == 0U))
    {
        return MOTION_LINE_RESULT_INVALID_ARGUMENT;
    }

    MotionWheel_Stop();
    MotionLineRequirement2_ResetControl();
    s_context.accelerationMMps2 =
        MotionLineRequirement2_TuneAccelerationMMps2;
    s_context.decelerationMMps2 =
        MotionLineRequirement2_TuneDecelerationMMps2;
    s_context.rightTurnRightAdjustRatio =
        MotionLineRequirement2_TuneRightTurnRightAdjustRatio;
    PID_Init(&s_context.pid,
             MotionLineRequirement2_TuneKpMMpsPerWeight,
             MotionLineRequirement2_TuneKiMMpsPerWeight,
             MotionLineRequirement2_TuneKdMMpsPerWeight, 0.0f, 0.0f);
    s_context.requestedSpeedMMps = speedMMps;
    s_context.error = MOTION_LINE_ERROR_NONE;
    s_context.state = MOTION_LINE_STATE_RUNNING;
    return MOTION_LINE_RESULT_OK;
}

MotionLine_Result_t MotionLineRequirement2_SetSpeed(float speedMMps)
{
    if (s_context.configured == 0U)
    {
        return MOTION_LINE_RESULT_NOT_CONFIGURED;
    }
    if (MotionLineRequirement2_IsBusy() == 0U)
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

MotionLine_Result_t MotionLineRequirement2_RequestStop(void)
{
    return MotionLineRequirement2_SetSpeed(0.0f);
}

void MotionLineRequirement2_Update(float dt)
{
    float leftSpeedMMps;
    float rightSpeedMMps;
    MotionWheel_Command_t command = {0};

    if (s_context.state != MOTION_LINE_STATE_RUNNING)
    {
        return;
    }
    if ((!isfinite(dt)) || (dt <= 0.0f))
    {
        MotionWheel_Stop();
        s_context.error = MOTION_LINE_ERROR_UPDATE_PERIOD_INVALID;
        s_context.state = MOTION_LINE_STATE_ERROR;
        return;
    }
    if (MotionLineRequirement2_CalculateTargetSpeeds(
            &leftSpeedMMps, &rightSpeedMMps, dt) == 0U)
    {
        MotionWheel_Stop();
        s_context.state = MOTION_LINE_STATE_FINISHED;
        return;
    }
    if ((s_context.requestedSpeedMMps <= 0.001f) &&
        (s_context.profileSpeedMMps <= 0.001f))
    {
        MotionWheel_Stop();
        s_context.state = MOTION_LINE_STATE_FINISHED;
        return;
    }
    command.targetSpeedLMMps = leftSpeedMMps;
    command.targetSpeedRMMps = rightSpeedMMps;
    if (MotionWheel_Update(&command, dt) != MOTION_WHEEL_RESULT_OK)
    {
        MotionWheel_Stop();
        s_context.error = MOTION_LINE_ERROR_WHEEL;
        s_context.state = MOTION_LINE_STATE_ERROR;
    }
}

void MotionLineRequirement2_Stop(void)
{
    MotionWheel_Stop();
    MotionLineRequirement2_ResetControl();
    s_context.error = MOTION_LINE_ERROR_NONE;
    s_context.state = MOTION_LINE_STATE_IDLE;
}

uint8_t MotionLineRequirement2_IsBusy(void)
{
    return (s_context.state == MOTION_LINE_STATE_RUNNING) ? 1U : 0U;
}

uint8_t MotionLineRequirement2_IsFinished(void)
{
    return (s_context.state == MOTION_LINE_STATE_FINISHED) ? 1U : 0U;
}

MotionLine_State_t MotionLineRequirement2_GetState(void)
{
    return s_context.state;
}

float MotionLineRequirement2_GetLineError(void)
{
    return s_context.lineError;
}

float MotionLineRequirement2_GetProfileAccelerationMMps2(void)
{
    return s_context.profileAccelerationMMps2;
}
