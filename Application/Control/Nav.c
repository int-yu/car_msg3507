#include "Application/Control/Nav.h"
#include "Application/Control/MotionWheel.h"
#include "Application/State/Heading.h"
#include <math.h>

typedef struct
{
    Nav_State_t state;
    Nav_Error_t error;
    float targetYawDeg;
    float angleErrorDeg;
    float requestedSpeedMMps;
    float profileTurnSpeedMMps;
    uint8_t settleCount;
    uint8_t configured;
} Nav_Context_t;

static Nav_Context_t s_context = {
    .state = NAV_STATE_IDLE,
    .error = NAV_ERROR_NONE,
};

static float Nav_Clamp(float value, float minimum, float maximum)
{
    if (value < minimum)
    {
        return minimum;
    }
    if (value > maximum)
    {
        return maximum;
    }
    return value;
}

static float Nav_Approach(float current, float target, float maximumStep)
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

static uint8_t Nav_ParametersAreValid(void)
{
    if ((!isfinite(NAV_MAX_TURN_SPEED_MMPS)) ||
        (!isfinite(NAV_MIN_TURN_SPEED_MMPS)) ||
        (!isfinite(NAV_SLOWDOWN_ANGLE_DEG)) ||
        (!isfinite(NAV_ACCELERATION_MMPS2)) ||
        (!isfinite(NAV_DECELERATION_MMPS2)) ||
        (!isfinite(NAV_ANGLE_TOLERANCE_DEG)))
    {
        return 0U;
    }
    if ((NAV_MAX_TURN_SPEED_MMPS <= 0.0f) ||
        (NAV_MIN_TURN_SPEED_MMPS <= 0.0f) ||
        (NAV_MIN_TURN_SPEED_MMPS > NAV_MAX_TURN_SPEED_MMPS) ||
        (NAV_SLOWDOWN_ANGLE_DEG <= NAV_ANGLE_TOLERANCE_DEG) ||
        (NAV_ACCELERATION_MMPS2 <= 0.0f) ||
        (NAV_DECELERATION_MMPS2 <= 0.0f) ||
        (NAV_ANGLE_TOLERANCE_DEG <= 0.0f) ||
        (NAV_SETTLE_TICKS == 0U) ||
        ((NAV_ROTATION_COMMAND_SIGN != 1) &&
         (NAV_ROTATION_COMMAND_SIGN != -1)))
    {
        return 0U;
    }
    return 1U;
}

static void Nav_ResetRuntime(void)
{
    s_context.targetYawDeg = 0.0f;
    s_context.angleErrorDeg = 0.0f;
    s_context.requestedSpeedMMps = 0.0f;
    s_context.profileTurnSpeedMMps = 0.0f;
    s_context.settleCount = 0U;
}

static void Nav_SetError(Nav_Error_t error)
{
    MotionWheel_Stop();
    s_context.profileTurnSpeedMMps = 0.0f;
    s_context.error = error;
    s_context.state = NAV_STATE_ERROR;
}

static Nav_Result_t Nav_Start(float targetYawDeg, float speedMMps)
{
    if (s_context.configured == 0U)
    {
        return NAV_RESULT_NOT_CONFIGURED;
    }
    if (Nav_IsBusy() != 0U)
    {
        return NAV_RESULT_BUSY;
    }
    if ((!isfinite(targetYawDeg)) || (!isfinite(speedMMps)) ||
        (speedMMps <= 0.0f))
    {
        return NAV_RESULT_INVALID_ARGUMENT;
    }
    if (Heading_IsReady() == 0U)
    {
        return NAV_RESULT_SENSOR_NOT_READY;
    }

    MotionWheel_Stop();
    Nav_ResetRuntime();
    s_context.targetYawDeg = targetYawDeg;
    s_context.angleErrorDeg = targetYawDeg - Heading_GetYaw();
    s_context.requestedSpeedMMps = Nav_Clamp(
        speedMMps, 0.0f, NAV_MAX_TURN_SPEED_MMPS);
    s_context.error = NAV_ERROR_NONE;
    s_context.state = NAV_STATE_RUNNING;
    return NAV_RESULT_OK;
}

/* 根据剩余角度生成带低速区的有符号转向轮速。 */
static float Nav_CalculateTargetTurnSpeed(void)
{
    float absoluteErrorDeg = fabsf(s_context.angleErrorDeg);
    float minimumSpeedMMps;
    float slowdownRatio;
    float speedMagnitudeMMps;
    float direction;

    if (absoluteErrorDeg <= NAV_ANGLE_TOLERANCE_DEG)
    {
        return 0.0f;
    }

    minimumSpeedMMps = Nav_Clamp(
        NAV_MIN_TURN_SPEED_MMPS,
        0.0f, s_context.requestedSpeedMMps);
    slowdownRatio = Nav_Clamp(
        absoluteErrorDeg / NAV_SLOWDOWN_ANGLE_DEG,
        0.0f, 1.0f);
    speedMagnitudeMMps = minimumSpeedMMps +
        (s_context.requestedSpeedMMps - minimumSpeedMMps) *
        slowdownRatio;

    direction = (s_context.angleErrorDeg >= 0.0f) ? 1.0f : -1.0f;
    return direction * (float)NAV_ROTATION_COMMAND_SIGN *
           speedMagnitudeMMps;
}

static void Nav_UpdateSpeedProfile(float targetTurnSpeedMMps, float dt)
{
    float speedStepMMps;
    uint8_t reversing =
        ((targetTurnSpeedMMps * s_context.profileTurnSpeedMMps) < 0.0f) ?
            1U : 0U;

    if ((reversing != 0U) ||
        (fabsf(targetTurnSpeedMMps) <
         fabsf(s_context.profileTurnSpeedMMps)))
    {
        speedStepMMps = NAV_DECELERATION_MMPS2 * dt;
    }
    else
    {
        speedStepMMps = NAV_ACCELERATION_MMPS2 * dt;
    }
    s_context.profileTurnSpeedMMps = Nav_Approach(
        s_context.profileTurnSpeedMMps,
        targetTurnSpeedMMps,
        speedStepMMps);
}

static MotionWheel_Result_t Nav_ApplyWheelCommand(float dt)
{
    MotionWheel_Command_t command = {0};
    float turnSpeedMMps = s_context.profileTurnSpeedMMps;

    /* 左右轮使用大小相等、方向相反的目标速度。 */
    command.targetSpeedLMMps = turnSpeedMMps;
    command.targetSpeedRMMps = -turnSpeedMMps;

    return MotionWheel_Update(&command, dt);
}

Nav_Result_t Nav_Init(void)
{
    MotionWheel_Result_t wheelResult;

    MotionWheel_Stop();
    s_context.configured = 0U;
    s_context.state = NAV_STATE_IDLE;
    s_context.error = NAV_ERROR_NONE;
    Nav_ResetRuntime();

    wheelResult = MotionWheel_Init();
    if ((wheelResult != MOTION_WHEEL_RESULT_OK) ||
        (Nav_ParametersAreValid() == 0U))
    {
        return NAV_RESULT_INVALID_ARGUMENT;
    }

    s_context.configured = 1U;
    return NAV_RESULT_OK;
}

Nav_Result_t Nav_StartTo(float targetYawDeg, float speedMMps)
{
    return Nav_Start(targetYawDeg, speedMMps);
}

Nav_Result_t Nav_StartBy(float deltaYawDeg, float speedMMps)
{
    if (!isfinite(deltaYawDeg))
    {
        return NAV_RESULT_INVALID_ARGUMENT;
    }
    if (Heading_IsReady() == 0U)
    {
        return NAV_RESULT_SENSOR_NOT_READY;
    }
    return Nav_StartTo(Heading_GetYaw() + deltaYawDeg, speedMMps);
}

void Nav_Update(float dt)
{
    float targetTurnSpeedMMps;

    if (s_context.state != NAV_STATE_RUNNING)
    {
        return;
    }
    if ((!isfinite(dt)) || (dt <= 0.0f))
    {
        Nav_SetError(NAV_ERROR_UPDATE_PERIOD_INVALID);
        return;
    }
    if (Heading_IsReady() == 0U)
    {
        Nav_SetError(NAV_ERROR_HEADING_OFFLINE);
        return;
    }

    s_context.angleErrorDeg =
        s_context.targetYawDeg - Heading_GetYaw();
    targetTurnSpeedMMps = Nav_CalculateTargetTurnSpeed();
    Nav_UpdateSpeedProfile(targetTurnSpeedMMps, dt);

    if (fabsf(s_context.angleErrorDeg) <= NAV_ANGLE_TOLERANCE_DEG)
    {
        if (fabsf(s_context.profileTurnSpeedMMps) <= 0.001f)
        {
            MotionWheel_Stop();
            s_context.settleCount++;
            if (s_context.settleCount >= NAV_SETTLE_TICKS)
            {
                s_context.state = NAV_STATE_COMPLETED;
            }
            return;
        }
    }
    else
    {
        s_context.settleCount = 0U;
    }

    if (Nav_ApplyWheelCommand(dt) != MOTION_WHEEL_RESULT_OK)
    {
        Nav_SetError(NAV_ERROR_WHEEL);
    }
}

void Nav_Stop(void)
{
    MotionWheel_Stop();
    Nav_ResetRuntime();
    s_context.error = NAV_ERROR_NONE;
    s_context.state = NAV_STATE_IDLE;
}

uint8_t Nav_IsConfigured(void)
{
    return s_context.configured;
}

uint8_t Nav_IsBusy(void)
{
    return (s_context.state == NAV_STATE_RUNNING) ? 1U : 0U;
}

uint8_t Nav_IsFinished(void)
{
    return (s_context.state == NAV_STATE_COMPLETED) ? 1U : 0U;
}

Nav_State_t Nav_GetState(void)
{
    return s_context.state;
}

Nav_Error_t Nav_GetError(void)
{
    return s_context.error;
}

float Nav_GetTargetYawDeg(void)
{
    return s_context.targetYawDeg;
}

float Nav_GetAngleErrorDeg(void)
{
    return s_context.angleErrorDeg;
}
