#include "Application/Control/MotionWheel.h"
#include "Application/Control/PID.h"
#include "Application/State/Odometry.h"
#include "Hardware/Motor/Motor.h"
#include "Hardware/Motor/PWM.h"
#include <math.h>
#include <stddef.h>

static PID_t s_leftPID;
static PID_t s_rightPID;
static float s_leftCommandPWM;
static float s_rightCommandPWM;
static uint8_t s_configured;

static float MotionWheel_Clamp(float value, float limit)
{
    if (value > limit)
    {
        return limit;
    }
    if (value < -limit)
    {
        return -limit;
    }
    return value;
}

static uint8_t MotionWheel_ParametersAreValid(void)
{
    if ((!isfinite(MOTION_WHEEL_KP)) ||
        (!isfinite(MOTION_WHEEL_KI)) ||
        (!isfinite(MOTION_WHEEL_INTEGRAL_LIMIT)) ||
        (!isfinite(MOTION_WHEEL_FEEDFORWARD_PWM_PER_MMPS)) ||
        (!isfinite(MOTION_WHEEL_STATIC_FRICTION_PWM)) ||
        (!isfinite(MOTION_WHEEL_MAX_COMMAND_PWM)))
    {
        return 0U;
    }
    if ((MOTION_WHEEL_KP < 0.0f) || (MOTION_WHEEL_KI < 0.0f) ||
        (MOTION_WHEEL_INTEGRAL_LIMIT < 0.0f) ||
        ((MOTION_WHEEL_KI > 0.0f) &&
         (MOTION_WHEEL_INTEGRAL_LIMIT <= 0.0f)) ||
        (MOTION_WHEEL_FEEDFORWARD_PWM_PER_MMPS < 0.0f) ||
        (MOTION_WHEEL_STATIC_FRICTION_PWM < 0.0f) ||
        (MOTION_WHEEL_MAX_COMMAND_PWM <= 0.0f) ||
        (MOTION_WHEEL_MAX_COMMAND_PWM > (float)PWM_MAX_DUTY) ||
        (MOTION_WHEEL_STATIC_FRICTION_PWM >
         MOTION_WHEEL_MAX_COMMAND_PWM))
    {
        return 0U;
    }
    if ((MOTION_WHEEL_KP == 0.0f) &&
        (MOTION_WHEEL_KI == 0.0f) &&
        (MOTION_WHEEL_FEEDFORWARD_PWM_PER_MMPS == 0.0f))
    {
        return 0U;
    }
    return 1U;
}

static float MotionWheel_GetFeedforward(float targetSpeedMMps)
{
    float output;

    if (fabsf(targetSpeedMMps) < 0.001f)
    {
        return 0.0f;
    }

    output = targetSpeedMMps * MOTION_WHEEL_FEEDFORWARD_PWM_PER_MMPS;
    output += (targetSpeedMMps > 0.0f) ?
                  MOTION_WHEEL_STATIC_FRICTION_PWM :
                  -MOTION_WHEEL_STATIC_FRICTION_PWM;
    return output;
}

static int16_t MotionWheel_ToMotorCommand(float value)
{
    value = MotionWheel_Clamp(value, MOTION_WHEEL_MAX_COMMAND_PWM);
    value += (value >= 0.0f) ? 0.5f : -0.5f;
    return (int16_t)value;
}

MotionWheel_Result_t MotionWheel_Init(void)
{
    Motor_StopAll();
    s_configured = 0U;

    if (MotionWheel_ParametersAreValid() == 0U)
    {
        return MOTION_WHEEL_RESULT_INVALID_ARGUMENT;
    }

    PID_Init(&s_leftPID, MOTION_WHEEL_KP, MOTION_WHEEL_KI, 0.0f,
             MOTION_WHEEL_MAX_COMMAND_PWM,
             MOTION_WHEEL_INTEGRAL_LIMIT);
    PID_Init(&s_rightPID, MOTION_WHEEL_KP, MOTION_WHEEL_KI, 0.0f,
             MOTION_WHEEL_MAX_COMMAND_PWM,
             MOTION_WHEEL_INTEGRAL_LIMIT);
    s_configured = 1U;
    MotionWheel_Reset();
    return MOTION_WHEEL_RESULT_OK;
}

MotionWheel_Result_t MotionWheel_Update(
    const MotionWheel_Command_t *command, float dt)
{
    float leftFeedbackPWM;
    float rightFeedbackPWM;

    if (s_configured == 0U)
    {
        return MOTION_WHEEL_RESULT_NOT_CONFIGURED;
    }
    if ((command == NULL) || (!isfinite(dt)) || (dt <= 0.0f) ||
        (!isfinite(command->targetSpeedLMMps)) ||
        (!isfinite(command->targetSpeedRMMps)) ||
        (!isfinite(command->trimLPWM)) || (!isfinite(command->trimRPWM)))
    {
        MotionWheel_Stop();
        return MOTION_WHEEL_RESULT_INVALID_ARGUMENT;
    }
    if (Odometry_CountsPerMM <= 0.001f)
    {
        MotionWheel_Stop();
        return MOTION_WHEEL_RESULT_ODOMETRY_INVALID;
    }

    leftFeedbackPWM = PID_Update(
        &s_leftPID, command->targetSpeedLMMps, Odometry_GetSpeedL(), dt);
    rightFeedbackPWM = PID_Update(
        &s_rightPID, command->targetSpeedRMMps, Odometry_GetSpeedR(), dt);

    s_leftCommandPWM = MotionWheel_GetFeedforward(
        command->targetSpeedLMMps) + leftFeedbackPWM + command->trimLPWM;
    s_rightCommandPWM = MotionWheel_GetFeedforward(
        command->targetSpeedRMMps) + rightFeedbackPWM + command->trimRPWM;
    s_leftCommandPWM = MotionWheel_Clamp(
        s_leftCommandPWM, MOTION_WHEEL_MAX_COMMAND_PWM);
    s_rightCommandPWM = MotionWheel_Clamp(
        s_rightCommandPWM, MOTION_WHEEL_MAX_COMMAND_PWM);

    Motor_SetPWM(MotionWheel_ToMotorCommand(s_leftCommandPWM),
                 MotionWheel_ToMotorCommand(s_rightCommandPWM));
    return MOTION_WHEEL_RESULT_OK;
}

void MotionWheel_Reset(void)
{
    if (s_configured != 0U)
    {
        PID_Reset(&s_leftPID);
        PID_Reset(&s_rightPID);
    }
    s_leftCommandPWM = 0.0f;
    s_rightCommandPWM = 0.0f;
}

void MotionWheel_Stop(void)
{
    Motor_StopAll();
    MotionWheel_Reset();
}

uint8_t MotionWheel_IsConfigured(void)
{
    return s_configured;
}

float MotionWheel_GetMaximumCommandPWM(void)
{
    return (s_configured != 0U) ? MOTION_WHEEL_MAX_COMMAND_PWM : 0.0f;
}

float MotionWheel_GetLeftCommandPWM(void)
{
    return s_leftCommandPWM;
}

float MotionWheel_GetRightCommandPWM(void)
{
    return s_rightCommandPWM;
}
