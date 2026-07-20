#include "Application/Control/MotionWheel.h"
#include "Application/Control/PID.h"
#include "Application/State/Odometry.h"
#include "Hardware/Motor/Motor.h"
#include "Hardware/Motor/PWM.h"
#include <math.h>
#include <stddef.h>

/* 运行时可调参数：左右电机、减速箱和轮胎不完全一致，必须分开标定。 */
float MotionWheel_TuneLeftKp = MOTION_WHEEL_LEFT_KP;
float MotionWheel_TuneLeftKi = MOTION_WHEEL_LEFT_KI;
float MotionWheel_TuneLeftIntegralLimit = MOTION_WHEEL_LEFT_INTEGRAL_LIMIT;
float MotionWheel_TuneLeftFeedforwardPWMPerMMps =
    MOTION_WHEEL_LEFT_FEEDFORWARD_PWM_PER_MMPS;
float MotionWheel_TuneLeftStaticFrictionPWM =
    MOTION_WHEEL_LEFT_STATIC_FRICTION_PWM;
float MotionWheel_TuneRightKp = MOTION_WHEEL_RIGHT_KP;
float MotionWheel_TuneRightKi = MOTION_WHEEL_RIGHT_KI;
float MotionWheel_TuneRightIntegralLimit = MOTION_WHEEL_RIGHT_INTEGRAL_LIMIT;
float MotionWheel_TuneRightFeedforwardPWMPerMMps =
    MOTION_WHEEL_RIGHT_FEEDFORWARD_PWM_PER_MMPS;
float MotionWheel_TuneRightStaticFrictionPWM =
    MOTION_WHEEL_RIGHT_STATIC_FRICTION_PWM;

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

static uint8_t MotionWheel_SideParametersAreValid(
    float kp, float ki, float integralLimit,
    float feedforwardPWMPerMMps, float staticFrictionPWM)
{
    if ((!isfinite(kp)) || (!isfinite(ki)) ||
        (!isfinite(integralLimit)) || (!isfinite(feedforwardPWMPerMMps)) ||
        (!isfinite(staticFrictionPWM)))
    {
        return 0U;
    }
    if ((kp < 0.0f) || (ki < 0.0f) || (integralLimit < 0.0f) ||
        ((ki > 0.0f) && (integralLimit <= 0.0f)) ||
        (feedforwardPWMPerMMps < 0.0f) || (staticFrictionPWM < 0.0f) ||
        (staticFrictionPWM > MOTION_WHEEL_MAX_COMMAND_PWM))
    {
        return 0U;
    }
    if ((kp == 0.0f) && (ki == 0.0f) &&
        (feedforwardPWMPerMMps == 0.0f))
    {
        return 0U;
    }
    return 1U;
}

static uint8_t MotionWheel_ParametersAreValid(void)
{
    if ((!isfinite(MOTION_WHEEL_MAX_COMMAND_PWM)) ||
        (MOTION_WHEEL_MAX_COMMAND_PWM <= 0.0f) ||
        (MOTION_WHEEL_MAX_COMMAND_PWM > (float)PWM_MAX_DUTY))
    {
        return 0U;
    }
    if (MotionWheel_SideParametersAreValid(
            MotionWheel_TuneLeftKp, MotionWheel_TuneLeftKi,
            MotionWheel_TuneLeftIntegralLimit,
            MotionWheel_TuneLeftFeedforwardPWMPerMMps,
            MotionWheel_TuneLeftStaticFrictionPWM) == 0U)
    {
        return 0U;
    }
    return MotionWheel_SideParametersAreValid(
        MotionWheel_TuneRightKp, MotionWheel_TuneRightKi,
        MotionWheel_TuneRightIntegralLimit,
        MotionWheel_TuneRightFeedforwardPWMPerMMps,
        MotionWheel_TuneRightStaticFrictionPWM);
}

static float MotionWheel_GetFeedforward(
    float targetSpeedMMps, float feedforwardPWMPerMMps,
    float staticFrictionPWM)
{
    float output;

    if (fabsf(targetSpeedMMps) < 0.001f)
    {
        return 0.0f;
    }

    output = targetSpeedMMps * feedforwardPWMPerMMps;
    output += (targetSpeedMMps > 0.0f) ?
                  staticFrictionPWM : -staticFrictionPWM;
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
    PID_Init(&s_leftPID, MotionWheel_TuneLeftKp, MotionWheel_TuneLeftKi, 0.0f,
             MOTION_WHEEL_MAX_COMMAND_PWM,
             MotionWheel_TuneLeftIntegralLimit);
    PID_Init(&s_rightPID, MotionWheel_TuneRightKp, MotionWheel_TuneRightKi, 0.0f,
             MOTION_WHEEL_MAX_COMMAND_PWM,
             MotionWheel_TuneRightIntegralLimit);
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
        command->targetSpeedLMMps,
        MotionWheel_TuneLeftFeedforwardPWMPerMMps,
        MotionWheel_TuneLeftStaticFrictionPWM) +
        leftFeedbackPWM + command->trimLPWM;
    s_rightCommandPWM = MotionWheel_GetFeedforward(
        command->targetSpeedRMMps,
        MotionWheel_TuneRightFeedforwardPWMPerMMps,
        MotionWheel_TuneRightStaticFrictionPWM) +
        rightFeedbackPWM + command->trimRPWM;
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
    PID_SetTunings(&s_leftPID, MotionWheel_TuneLeftKp,
                   MotionWheel_TuneLeftKi, 0.0f);
    PID_SetTunings(&s_rightPID, MotionWheel_TuneRightKp,
                   MotionWheel_TuneRightKi, 0.0f);
    s_leftPID.integralMax = MotionWheel_TuneLeftIntegralLimit;
    s_rightPID.integralMax = MotionWheel_TuneRightIntegralLimit;
}

void MotionWheel_Stop(void)
{
    Motor_Brake();
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
