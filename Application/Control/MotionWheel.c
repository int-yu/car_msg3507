#include "Application/Control/MotionWheel.h"
#include "Application/Control/PID.h"
#include "Application/State/Odometry.h"
#include "Hardware/Motor/Motor.h"
#include "Hardware/Motor/PWM.h"
#include <math.h>
#include <stddef.h>

/* 运行时可调参数，默认值取头文件 #define；范围校验由 Param 模块负责。 */
float MotionWheel_TuneKp = MOTION_WHEEL_KP;
float MotionWheel_TuneKi = MOTION_WHEEL_KI;
float MotionWheel_TuneIntegralLimit = MOTION_WHEEL_INTEGRAL_LIMIT;
float MotionWheel_TuneFeedforwardPWMPerMMps =
    MOTION_WHEEL_FEEDFORWARD_PWM_PER_MMPS;
float MotionWheel_TuneStaticFrictionPWM = MOTION_WHEEL_STATIC_FRICTION_PWM;

static PID_t s_leftPID;
static PID_t s_rightPID;
static float s_leftCommandPWM;
static float s_rightCommandPWM;
static float s_targetSpeedLMMps;
static float s_targetSpeedRMMps;
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

    output = targetSpeedMMps * MotionWheel_TuneFeedforwardPWMPerMMps;
    output += (targetSpeedMMps > 0.0f) ?
                  MotionWheel_TuneStaticFrictionPWM :
                  -MotionWheel_TuneStaticFrictionPWM;
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

    /* 用运行时参数初始化：同一次上电内重复 Init 不会丢掉已调好的增益。 */
    PID_Init(&s_leftPID, MotionWheel_TuneKp, MotionWheel_TuneKi, 0.0f,
             MOTION_WHEEL_MAX_COMMAND_PWM,
             MotionWheel_TuneIntegralLimit);
    PID_Init(&s_rightPID, MotionWheel_TuneKp, MotionWheel_TuneKi, 0.0f,
             MOTION_WHEEL_MAX_COMMAND_PWM,
             MotionWheel_TuneIntegralLimit);
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

    s_targetSpeedLMMps = command->targetSpeedLMMps;
    s_targetSpeedRMMps = command->targetSpeedRMMps;

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
    s_targetSpeedLMMps = 0.0f;
    s_targetSpeedRMMps = 0.0f;
}

void MotionWheel_ApplyPidTunings(void)
{
    if (s_configured == 0U)
    {
        return;
    }
    PID_SetTunings(&s_leftPID, MotionWheel_TuneKp,
                   MotionWheel_TuneKi, 0.0f);
    PID_SetTunings(&s_rightPID, MotionWheel_TuneKp,
                   MotionWheel_TuneKi, 0.0f);
    s_leftPID.integralMax = MotionWheel_TuneIntegralLimit;
    s_rightPID.integralMax = MotionWheel_TuneIntegralLimit;
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

float MotionWheel_GetTargetSpeedL(void)
{
    return s_targetSpeedLMMps;
}

float MotionWheel_GetTargetSpeedR(void)
{
    return s_targetSpeedRMMps;
}
