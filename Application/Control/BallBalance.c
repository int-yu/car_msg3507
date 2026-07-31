#include "Application/Control/BallBalance.h"
#include "Application/Control/BallSensor.h"
#include "Application/Control/BeamActuator.h"
#include "Application/Control/MotionLine.h"
#include <math.h>

float BallBalance_TunePositionKpPerS = BALL_BALANCE_POSITION_KP_PER_S;
float BallBalance_TuneVelocityKpDegPerMMps =
    BALL_BALANCE_VELOCITY_KP_DEG_PER_MMPS;
float BallBalance_TuneVelocityKiDegPerMM =
    BALL_BALANCE_VELOCITY_KI_DEG_PER_MM;
float BallBalance_TuneMaxVelocityMMps = BALL_BALANCE_MAX_VELOCITY_MMPS;
float BallBalance_TuneFeedforwardDegPerMMps2 =
    BALL_BALANCE_FEEDFORWARD_DEG_PER_MMPS2;

typedef struct
{
    BallBalance_State_t state;
    BallBalance_Error_t error;
    float targetMM;
    float velocityTargetMMps;
    float velocityIntegralMM;
    float tiltCommandDeg;
    uint16_t settleTicks;
    uint16_t visionLostTicks;
} BallBalance_Context_t;

static BallBalance_Context_t s_context;

static float BallBalance_Clamp(float value, float limit)
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

static uint8_t BallBalance_TargetIsValid(float targetMM)
{
    return (isfinite(targetMM) &&
            (fabsf(targetMM) <= BALL_BALANCE_TARGET_LIMIT_MM)) ? 1U : 0U;
}

static void BallBalance_ResetRuntime(void)
{
    s_context.targetMM = 0.0f;
    s_context.velocityTargetMMps = 0.0f;
    s_context.velocityIntegralMM = 0.0f;
    s_context.tiltCommandDeg = 0.0f;
    s_context.settleTicks = 0U;
    s_context.visionLostTicks = 0U;
}

static void BallBalance_UpdateSettle(float positionMM)
{
    if (fabsf(positionMM - s_context.targetMM) <=
        BALL_BALANCE_SETTLE_TOLERANCE_MM)
    {
        if (s_context.settleTicks < UINT16_MAX)
        {
            s_context.settleTicks++;
        }
    }
    else
    {
        s_context.settleTicks = 0U;
    }
}

void BallBalance_Init(void)
{
    s_context.state = BALL_BALANCE_STATE_IDLE;
    s_context.error = BALL_BALANCE_ERROR_NONE;
    BallBalance_ResetRuntime();
    BallBalance_TunePositionKpPerS = BALL_BALANCE_POSITION_KP_PER_S;
    BallBalance_TuneVelocityKpDegPerMMps =
        BALL_BALANCE_VELOCITY_KP_DEG_PER_MMPS;
    BallBalance_TuneVelocityKiDegPerMM =
        BALL_BALANCE_VELOCITY_KI_DEG_PER_MM;
    BallBalance_TuneMaxVelocityMMps = BALL_BALANCE_MAX_VELOCITY_MMPS;
    BallBalance_TuneFeedforwardDegPerMMps2 =
        BALL_BALANCE_FEEDFORWARD_DEG_PER_MMPS2;
}

BallBalance_Result_t BallBalance_Start(float targetMM)
{
    if ((BallSensor_IsFresh() == 0U) ||
        (BallBalance_TargetIsValid(targetMM) == 0U))
    {
        return BALL_BALANCE_RESULT_INVALID_ARGUMENT;
    }

    BallBalance_ResetRuntime();
    s_context.targetMM = targetMM;
    s_context.error = BALL_BALANCE_ERROR_NONE;
    s_context.state = BALL_BALANCE_STATE_RUNNING;
    return BALL_BALANCE_RESULT_OK;
}

BallBalance_Result_t BallBalance_SetTarget(float targetMM)
{
    if (BallBalance_TargetIsValid(targetMM) == 0U)
    {
        return BALL_BALANCE_RESULT_INVALID_ARGUMENT;
    }
    if (s_context.state != BALL_BALANCE_STATE_RUNNING)
    {
        return BALL_BALANCE_RESULT_NOT_RUNNING;
    }

    s_context.targetMM = targetMM;
    s_context.velocityTargetMMps = 0.0f;
    s_context.velocityIntegralMM = 0.0f;
    s_context.settleTicks = 0U;
    return BALL_BALANCE_RESULT_OK;
}

void BallBalance_Update(float dt)
{
    float positionMM;
    float speedMMps;
    float errorMM;
    float velocityErrorMMps;
    float tiltDeg;
    float chassisAccelMMps2;
    float feedforwardDeg;

    if (s_context.state != BALL_BALANCE_STATE_RUNNING)
    {
        return;
    }
    if ((!isfinite(dt)) || (dt <= 0.0f))
    {
        s_context.error = BALL_BALANCE_ERROR_UPDATE_PERIOD_INVALID;
        s_context.state = BALL_BALANCE_STATE_ERROR;
        s_context.tiltCommandDeg = 0.0f;
        BeamActuator_SetTiltDeg(0.0f);
        return;
    }

    if (BallSensor_IsFresh() == 0U)
    {
        if (s_context.visionLostTicks < UINT16_MAX)
        {
            s_context.visionLostTicks++;
        }
        if (s_context.visionLostTicks >= BALL_BALANCE_VISION_LOST_TICKS)
        {
            s_context.error = BALL_BALANCE_ERROR_VISION_LOST;
            s_context.state = BALL_BALANCE_STATE_ERROR;
            s_context.tiltCommandDeg = 0.0f;
            BeamActuator_SetTiltDeg(0.0f);
        }
        return;
    }
    s_context.visionLostTicks = 0U;

    positionMM = BallSensor_GetPositionMM();
    speedMMps = BallSensor_GetSpeedMMps();
    errorMM = s_context.targetMM - positionMM;

    s_context.velocityTargetMMps = BallBalance_Clamp(
        BallBalance_TunePositionKpPerS * errorMM,
        BallBalance_TuneMaxVelocityMMps);
    velocityErrorMMps = s_context.velocityTargetMMps - speedMMps;
    s_context.velocityIntegralMM += velocityErrorMMps * dt;
    s_context.velocityIntegralMM = BallBalance_Clamp(
        s_context.velocityIntegralMM,
        BALL_BALANCE_VELOCITY_INTEGRAL_LIMIT_MM);

    /* PID control */
    tiltDeg =
        (BallBalance_TuneVelocityKpDegPerMMps * velocityErrorMMps) +
        (BallBalance_TuneVelocityKiDegPerMM *
         s_context.velocityIntegralMM);

    /* Chassis acceleration feedforward compensation.
     * Only apply when vehicle has significant velocity to avoid forcing
     * the ball to move before the chassis actually accelerates. */
    chassisAccelMMps2 = MotionLine_GetProfileAccelerationMMps2();
    if (MotionLine_GetProfileSpeedMMps() > 50.0f)
    {
        feedforwardDeg = chassisAccelMMps2 * BallBalance_TuneFeedforwardDegPerMMps2;
        tiltDeg += feedforwardDeg;
    }

    s_context.tiltCommandDeg = tiltDeg;
    BeamActuator_SetTiltDeg(tiltDeg);
    BallBalance_UpdateSettle(positionMM);
}

void BallBalance_Stop(void)
{
    s_context.state = BALL_BALANCE_STATE_IDLE;
    s_context.error = BALL_BALANCE_ERROR_NONE;
    BallBalance_ResetRuntime();
    BeamActuator_SetTiltDeg(0.0f);
}

BallBalance_State_t BallBalance_GetState(void)
{
    return s_context.state;
}

BallBalance_Error_t BallBalance_GetError(void)
{
    return s_context.error;
}

uint8_t BallBalance_IsStable(void)
{
    return ((s_context.state == BALL_BALANCE_STATE_RUNNING) &&
            (s_context.settleTicks >= BALL_BALANCE_SETTLE_CONFIRM_TICKS)) ?
        1U : 0U;
}

float BallBalance_GetPositionMM(void)
{
    return BallSensor_GetPositionMM();
}

float BallBalance_GetTargetMM(void)
{
    return s_context.targetMM;
}

float BallBalance_GetProfilePositionMM(void)
{
    return s_context.targetMM;
}

float BallBalance_GetVelocityTargetMMps(void)
{
    return s_context.velocityTargetMMps;
}

float BallBalance_GetTiltCommandDeg(void)
{
    return s_context.tiltCommandDeg;
}

float BallBalance_GetIntegralMMs(void)
{
    return s_context.velocityIntegralMM;
}
