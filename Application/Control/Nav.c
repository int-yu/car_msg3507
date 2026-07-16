#include "Application/Control/Nav.h"
#include "Application/Control/NavConfig.h"
#include "Application/Control/MotionWheel.h"
#include "Application/State/Heading.h"
#include <math.h>
#include <stddef.h>

typedef struct
{
    Nav_State_t state;
    Nav_Error_t error;
    Nav_Mode_t mode;
    float targetYawDeg;
    float angleErrorDeg;
    float requestedSpeedMMps;
    float profileTurnSpeedMMps;
    uint8_t settleCount;
    uint8_t configured;
} Nav_Context_t;

static Nav_Config_t s_config;
static Nav_Context_t s_context = {
    .state = NAV_STATE_IDLE,
    .error = NAV_ERROR_NONE,
    .mode = NAV_MODE_PIVOT,
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

static uint8_t Nav_ConfigIsValid(const Nav_Config_t *config)
{
    if (config == NULL)
    {
        return 0U;
    }
    if ((!isfinite(config->maximumTurnSpeedMMps)) ||
        (!isfinite(config->minimumTurnSpeedMMps)) ||
        (!isfinite(config->slowdownAngleDeg)) ||
        (!isfinite(config->accelerationMMps2)) ||
        (!isfinite(config->decelerationMMps2)) ||
        (!isfinite(config->angleToleranceDeg)))
    {
        return 0U;
    }
    if ((config->maximumTurnSpeedMMps <= 0.0f) ||
        (config->minimumTurnSpeedMMps <= 0.0f) ||
        (config->minimumTurnSpeedMMps > config->maximumTurnSpeedMMps) ||
        (config->slowdownAngleDeg <= config->angleToleranceDeg) ||
        (config->accelerationMMps2 <= 0.0f) ||
        (config->decelerationMMps2 <= 0.0f) ||
        (config->angleToleranceDeg <= 0.0f) ||
        (config->settleTicks == 0U) ||
        ((config->rotationCommandSign != 1) &&
         (config->rotationCommandSign != -1)))
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

static Nav_Result_t Nav_Start(
    float targetYawDeg, float speedMMps, Nav_Mode_t mode)
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
        (speedMMps <= 0.0f) ||
        ((mode != NAV_MODE_PIVOT) && (mode != NAV_MODE_SPIN)))
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
        speedMMps, 0.0f, s_config.maximumTurnSpeedMMps);
    s_context.mode = mode;
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

    if (absoluteErrorDeg <= s_config.angleToleranceDeg)
    {
        return 0.0f;
    }

    minimumSpeedMMps = Nav_Clamp(
        s_config.minimumTurnSpeedMMps,
        0.0f, s_context.requestedSpeedMMps);
    slowdownRatio = Nav_Clamp(
        absoluteErrorDeg / s_config.slowdownAngleDeg,
        0.0f, 1.0f);
    speedMagnitudeMMps = minimumSpeedMMps +
        (s_context.requestedSpeedMMps - minimumSpeedMMps) *
        slowdownRatio;

    direction = (s_context.angleErrorDeg >= 0.0f) ? 1.0f : -1.0f;
    return direction * (float)s_config.rotationCommandSign *
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
        speedStepMMps = s_config.decelerationMMps2 * dt;
    }
    else
    {
        speedStepMMps = s_config.accelerationMMps2 * dt;
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

    if (s_context.mode == NAV_MODE_SPIN)
    {
        command.targetSpeedLMMps = turnSpeedMMps;
        command.targetSpeedRMMps = -turnSpeedMMps;
    }
    else if (turnSpeedMMps >= 0.0f)
    {
        command.targetSpeedLMMps = turnSpeedMMps;
        command.targetSpeedRMMps = 0.0f;
    }
    else
    {
        command.targetSpeedLMMps = 0.0f;
        command.targetSpeedRMMps = -turnSpeedMMps;
    }

    return MotionWheel_Update(&command, dt);
}

Nav_Result_t Nav_Init(const Nav_Config_t *config)
{
    MotionWheel_Result_t wheelResult;

    MotionWheel_Stop();
    s_context.configured = 0U;
    s_context.state = NAV_STATE_IDLE;
    s_context.error = NAV_ERROR_NONE;
    Nav_ResetRuntime();

    wheelResult = MotionWheel_InitDefault();
    if ((wheelResult != MOTION_WHEEL_RESULT_OK) ||
        (Nav_ConfigIsValid(config) == 0U))
    {
        return NAV_RESULT_INVALID_ARGUMENT;
    }

    s_config = *config;
    s_context.configured = 1U;
    return NAV_RESULT_OK;
}

Nav_Result_t Nav_InitDefault(void)
{
    return Nav_Init(&g_navConfig);
}

Nav_Result_t Nav_StartPivotTo(float targetYawDeg, float speedMMps)
{
    return Nav_Start(targetYawDeg, speedMMps, NAV_MODE_PIVOT);
}

Nav_Result_t Nav_StartSpinTo(float targetYawDeg, float speedMMps)
{
    return Nav_Start(targetYawDeg, speedMMps, NAV_MODE_SPIN);
}

Nav_Result_t Nav_StartPivotBy(float deltaYawDeg, float speedMMps)
{
    if (!isfinite(deltaYawDeg))
    {
        return NAV_RESULT_INVALID_ARGUMENT;
    }
    if (Heading_IsReady() == 0U)
    {
        return NAV_RESULT_SENSOR_NOT_READY;
    }
    return Nav_StartPivotTo(Heading_GetYaw() + deltaYawDeg, speedMMps);
}

Nav_Result_t Nav_StartSpinBy(float deltaYawDeg, float speedMMps)
{
    if (!isfinite(deltaYawDeg))
    {
        return NAV_RESULT_INVALID_ARGUMENT;
    }
    if (Heading_IsReady() == 0U)
    {
        return NAV_RESULT_SENSOR_NOT_READY;
    }
    return Nav_StartSpinTo(Heading_GetYaw() + deltaYawDeg, speedMMps);
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

    if (fabsf(s_context.angleErrorDeg) <= s_config.angleToleranceDeg)
    {
        if (fabsf(s_context.profileTurnSpeedMMps) <= 0.001f)
        {
            MotionWheel_Stop();
            s_context.settleCount++;
            if (s_context.settleCount >= s_config.settleTicks)
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

Nav_Mode_t Nav_GetMode(void)
{
    return s_context.mode;
}

float Nav_GetTargetYawDeg(void)
{
    return s_context.targetYawDeg;
}

float Nav_GetAngleErrorDeg(void)
{
    return s_context.angleErrorDeg;
}
