#include "Application/Control/BallBalance.h"
#include "Application/Control/BallSensor.h"
#include "Application/Control/BeamActuator.h"
#include <math.h>

float BallBalance_TuneKp = BALL_BALANCE_KP_DEG_PER_MM;
float BallBalance_TuneKd = BALL_BALANCE_KD_DEG_PER_MMPS;
float BallBalance_TuneGravityCoupling =
    BALL_BALANCE_GRAVITY_COUPLING_MMPS2_PER_DEG;

typedef struct
{
    BallBalance_State_t state;
    BallBalance_Error_t error;
    float targetMM;
    /* 轨迹发生器的参考量：PD 跟踪的是它，不是最终目标。 */
    float profilePositionMM;
    float profileSpeedMMps;
    float carAccelerationMMps2;
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

static void BallBalance_ResetRuntime(void)
{
    s_context.targetMM = 0.0f;
    s_context.profilePositionMM = 0.0f;
    s_context.profileSpeedMMps = 0.0f;
    s_context.carAccelerationMMps2 = 0.0f;
    s_context.tiltCommandDeg = 0.0f;
    s_context.settleTicks = 0U;
    s_context.visionLostTicks = 0U;
}

/*
 * 梯形速度轨迹。每拍按当前剩余距离决定加速还是减速：以最大减速度恰好
 * 停在目标所需的速度是 sqrt(2*a*|剩余|)，取它和 v_max 的较小者。
 */
static void BallBalance_UpdateProfile(float dt)
{
    float remainingMM = s_context.targetMM - s_context.profilePositionMM;
    float distanceMM = fabsf(remainingMM);
    float directionSign = (remainingMM >= 0.0f) ? 1.0f : -1.0f;
    float stoppingSpeedMMps =
        sqrtf(2.0f * BALL_BALANCE_ACCELERATION_MMPS2 * distanceMM);
    float targetSpeedMMps =
        (stoppingSpeedMMps < BALL_BALANCE_MAX_SPEED_MMPS) ?
            stoppingSpeedMMps : BALL_BALANCE_MAX_SPEED_MMPS;
    float maximumStep = BALL_BALANCE_ACCELERATION_MMPS2 * dt;
    float signedTargetSpeedMMps = directionSign * targetSpeedMMps;
    float speedErrorMMps = signedTargetSpeedMMps - s_context.profileSpeedMMps;

    if (speedErrorMMps > maximumStep)
    {
        speedErrorMMps = maximumStep;
    }
    else if (speedErrorMMps < -maximumStep)
    {
        speedErrorMMps = -maximumStep;
    }
    s_context.profileSpeedMMps += speedErrorMMps;

    s_context.profilePositionMM += s_context.profileSpeedMMps * dt;

    /* 收尾时直接吸附到目标，避免浮点残差让参考位置永远差一点点。 */
    if ((fabsf(s_context.targetMM - s_context.profilePositionMM) < 0.5f) &&
        (fabsf(s_context.profileSpeedMMps) < 1.0f))
    {
        s_context.profilePositionMM = s_context.targetMM;
        s_context.profileSpeedMMps = 0.0f;
    }
}

static void BallBalance_UpdateSettle(float positionMM)
{
    if (fabsf(positionMM - s_context.targetMM) <=
        BALL_BALANCE_SETTLE_TOLERANCE_MM)
    {
        /* 饱和而不是回绕：回绕会让一个刚满足容差的计数突然变成 0，
         * 任务层就会在钢球其实已经稳住时反复推迟折返。 */
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
    BallBalance_TuneKp = BALL_BALANCE_KP_DEG_PER_MM;
    BallBalance_TuneKd = BALL_BALANCE_KD_DEG_PER_MMPS;
    BallBalance_TuneGravityCoupling =
        BALL_BALANCE_GRAVITY_COUPLING_MMPS2_PER_DEG;
}

BallBalance_Result_t BallBalance_Start(void)
{
    if (BallSensor_IsFresh() == 0U)
    {
        return BALL_BALANCE_RESULT_INVALID_ARGUMENT;
    }

    BallBalance_ResetRuntime();
    /* 从钢球当前位置起步，否则第一拍就会有一个大阶跃。 */
    s_context.profilePositionMM = BallSensor_GetPositionMM();
    s_context.targetMM = s_context.profilePositionMM;
    s_context.error = BALL_BALANCE_ERROR_NONE;
    s_context.state = BALL_BALANCE_STATE_RUNNING;
    return BALL_BALANCE_RESULT_OK;
}

BallBalance_Result_t BallBalance_SetTarget(float targetMM)
{
    if (!isfinite(targetMM))
    {
        return BALL_BALANCE_RESULT_INVALID_ARGUMENT;
    }
    if (s_context.state != BALL_BALANCE_STATE_RUNNING)
    {
        return BALL_BALANCE_RESULT_NOT_RUNNING;
    }

    s_context.targetMM = targetMM;
    s_context.settleTicks = 0U;
    return BALL_BALANCE_RESULT_OK;
}

void BallBalance_SetCarAcceleration(float accelerationMMps2)
{
    s_context.carAccelerationMMps2 =
        isfinite(accelerationMMps2) ? accelerationMMps2 : 0.0f;
}

void BallBalance_Update(float dt)
{
    float positionMM;
    float speedMMps;
    float feedforwardDeg;
    float tiltDeg;

    if (s_context.state != BALL_BALANCE_STATE_RUNNING)
    {
        return;
    }
    if ((!isfinite(dt)) || (dt <= 0.0f))
    {
        s_context.error = BALL_BALANCE_ERROR_UPDATE_PERIOD_INVALID;
        s_context.state = BALL_BALANCE_STATE_ERROR;
        BeamActuator_SetTiltDeg(0.0f);
        return;
    }

    /*
     * 视觉失效不能继续闭环：钢球是双积分对象，拿冻结的位置算 PD 会让
     * 倾角锁死，球一路加速滚到挡片。短暂丢帧允许维持上一拍命令，超过
     * 阈值就回中报错。
     */
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

    BallBalance_UpdateProfile(dt);

    /*
     * 前馈抵消车体惯性力：钢球在加速的小车上受大小为 a_car 的惯性力，
     * 需要 theta_ff = a_car / k 的倾角补偿。轨迹自身的加速度不做前馈，
     * 因为它已经足够慢，PD 跟得上。
     */
    feedforwardDeg = (BallBalance_TuneGravityCoupling > 0.0f) ?
        (s_context.carAccelerationMMps2 / BallBalance_TuneGravityCoupling) :
        0.0f;

    tiltDeg = feedforwardDeg +
              BallBalance_TuneKp *
                  (s_context.profilePositionMM - positionMM) +
              BallBalance_TuneKd *
                  (s_context.profileSpeedMMps - speedMMps);
    tiltDeg = BallBalance_Clamp(tiltDeg, BALL_BALANCE_MAX_TILT_DEG);

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
    return s_context.profilePositionMM;
}

float BallBalance_GetTiltCommandDeg(void)
{
    return s_context.tiltCommandDeg;
}
