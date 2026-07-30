#include "Application/Control/MotionLine.h"
#include "Application/Control/MotionWheel.h"
#include "Hardware/Sensors/Graydetect.h"
#include <math.h>

typedef struct
{
    MotionLine_State_t state;
    MotionLine_Error_t error;
    float requestedSpeedMMps;
    float profileSpeedMMps;
    float profileAccelerationMMps2;
    float lineError;
    float filteredWeight;
    float previousWeight;
    float speedAdjustMMps;
    float lastLeftSpeedMMps;
    float lastRightSpeedMMps;
    uint16_t lostTicks;
    uint8_t configured;
} MotionLine_Context_t;

/* 运行时可调参数，默认值取头文件 #define；范围校验由 Param 模块负责。 */
float MotionLine_TuneMaxAdjustRatio = MOTION_LINE_MAX_ADJUST_RATIO;
float MotionLine_TuneWeightKd = 0.0f;

static MotionLine_Context_t s_context = {
    .state = MOTION_LINE_STATE_IDLE,
    .error = MOTION_LINE_ERROR_NONE,
};

static uint8_t MotionLine_ParametersAreValid(void)
{
    if ((!isfinite(MOTION_LINE_MAX_SPEED_MMPS)) ||
        (!isfinite(MOTION_LINE_MAX_ADJUST_RATIO)) ||
        (!isfinite(MOTION_LINE_ACCELERATION_MMPS2)) ||
        (!isfinite(MOTION_LINE_DECELERATION_MMPS2)) ||
        (!isfinite(MOTION_LINE_CURVE_MIN_SPEED_RATIO)) ||
        (!isfinite(MOTION_LINE_WEIGHT_FILTER_ALPHA)) ||
        (!isfinite(MOTION_LINE_MAX_ADJUST_RATE_MMPS2)))
    {
        return 0U;
    }

    if ((MOTION_LINE_MAX_SPEED_MMPS <= 0.0f) ||
        (MOTION_LINE_MAX_ADJUST_RATIO <= 0.0f) ||
        (MOTION_LINE_MAX_ADJUST_RATIO > 1.0f) ||
        (MOTION_LINE_ACCELERATION_MMPS2 <= 0.0f) ||
        (MOTION_LINE_DECELERATION_MMPS2 <= 0.0f) ||
        (MOTION_LINE_CURVE_MIN_SPEED_RATIO <= 0.0f) ||
        (MOTION_LINE_CURVE_MIN_SPEED_RATIO > 1.0f) ||
        (MOTION_LINE_WEIGHT_FILTER_ALPHA <= 0.0f) ||
        (MOTION_LINE_WEIGHT_FILTER_ALPHA > 1.0f) ||
        (MOTION_LINE_MAX_ADJUST_RATE_MMPS2 <= 0.0f) ||
        (MOTION_LINE_OUTER_WEIGHT <= 0) ||
        (MOTION_LINE_INNER_WEIGHT <= 0) ||
        (MOTION_LINE_INNER_WEIGHT >= MOTION_LINE_OUTER_WEIGHT) ||
        (MOTION_LINE_LOST_CONFIRM_TICKS == 0U))
    {
        return 0U;
    }

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
    s_context.filteredWeight = 0.0f;
    s_context.previousWeight = 0.0f;
    s_context.speedAdjustMMps = 0.0f;
    s_context.lastLeftSpeedMMps = 0.0f;
    s_context.lastRightSpeedMMps = 0.0f;
    s_context.lostTicks = 0U;
}

static void MotionLine_SetError(MotionLine_Error_t error)
{
    MotionWheel_Stop();
    MotionLine_ResetControl();
    s_context.error = error;
    s_context.state = MOTION_LINE_STATE_ERROR;
}

/*
 * 教程协议为 bit0=CH1 ... bit5=CH6。俯视传感器（探头朝前）时 CH1 在
 * 右侧、CH6 在左侧，因此 bit0 的正权重表示车应向右修正；安装翻面时只需
 * 修改 Graydetect.h 的 GRAYDETECT_CHANNEL1_IS_RIGHT。
 */
#if GRAYDETECT_CHANNEL1_IS_RIGHT
static const int8_t s_grayWeight[GRAY_CHANNEL_COUNT] = {
     MOTION_LINE_OUTER_WEIGHT, 4, MOTION_LINE_INNER_WEIGHT,
    -MOTION_LINE_INNER_WEIGHT, -4, -MOTION_LINE_OUTER_WEIGHT
};
#else
static const int8_t s_grayWeight[GRAY_CHANNEL_COUNT] = {
    -MOTION_LINE_OUTER_WEIGHT, -4, -MOTION_LINE_INNER_WEIGHT,
     MOTION_LINE_INNER_WEIGHT, 4, MOTION_LINE_OUTER_WEIGHT
};
#endif

static int8_t MotionLine_GetWeight(uint8_t grayState)
{
    int16_t weight = 0;
    uint8_t index;

    for (index = 0U; index < GRAY_CHANNEL_COUNT; index++)
    {
        if ((grayState & (uint8_t)(1U << index)) != 0U)
        {
            weight += s_grayWeight[index];
        }
    }

    /* 多个同侧探头同时压线时，不允许超过最外侧的修正力度。 */
    if (weight > MOTION_LINE_OUTER_WEIGHT)
    {
        weight = MOTION_LINE_OUTER_WEIGHT;
    }
    else if (weight < -MOTION_LINE_OUTER_WEIGHT)
    {
        weight = -MOTION_LINE_OUTER_WEIGHT;
    }

    return (int8_t)weight;
}

static float MotionLine_GetCurveTargetSpeed(float weight)
{
    float errorCurveRatio;
    float normalizedWeight = fabsf(weight) /
                             (float)MOTION_LINE_OUTER_WEIGHT;

    if (normalizedWeight > 1.0f)
    {
        normalizedWeight = 1.0f;
    }
    errorCurveRatio = 1.0f -
                  (1.0f - MOTION_LINE_CURVE_MIN_SPEED_RATIO) *
                  normalizedWeight;
    return s_context.requestedSpeedMMps * errorCurveRatio;
}

static float MotionLine_UpdateProfileSpeed(float targetSpeedMMps, float dt)
{
    float maximumStep = (targetSpeedMMps > s_context.profileSpeedMMps) ?
        (MOTION_LINE_ACCELERATION_MMPS2 * dt) :
        (MOTION_LINE_DECELERATION_MMPS2 * dt);
    float previousSpeedMMps = s_context.profileSpeedMMps;

    s_context.profileSpeedMMps = MotionLine_Approach(
        s_context.profileSpeedMMps, targetSpeedMMps, maximumStep);
    /* 规划加速度而不是编码器差分：差分噪声乘进摆杆前馈会让钢球抖到
     * 没法看，而这里是规划量——无噪声，且先于实际运动一拍。 */
    s_context.profileAccelerationMMps2 =
        (s_context.profileSpeedMMps - previousSpeedMMps) / dt;
    return s_context.profileSpeedMMps;
}

static uint8_t MotionLine_CalculateTargetSpeeds(
    float *leftSpeedMMps, float *rightSpeedMMps, float dt)
{
    uint8_t grayState = Graydetect_GetState();
    float rawWeight;
    float weight;
    float requestedAdjustMMps;
    float targetSpeedMMps;
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

        /* 短暂丢线时沿用最近一次偏差，但仍按新的速度斜坡减速/提速。 */
        rawWeight = s_context.filteredWeight;
    }
    else
    {
        s_context.lostTicks = 0U;
        rawWeight = (float)MotionLine_GetWeight(grayState);
    }

    s_context.filteredWeight += MOTION_LINE_WEIGHT_FILTER_ALPHA *
        (rawWeight - s_context.filteredWeight);
    weight = s_context.filteredWeight;
    s_context.lineError = weight;

    targetSpeedMMps = MotionLine_GetCurveTargetSpeed(weight);
    (void)MotionLine_UpdateProfileSpeed(targetSpeedMMps, dt);

    /* 权重达到正负 6 时，速度增减比例等于最大调整比例；
     * 微分项对权重跳变（压线切换瞬间）施加一次性阻尼，默认 0 不生效。 */
    requestedAdjustMMps = s_context.profileSpeedMMps *
                           MotionLine_TuneMaxAdjustRatio *
                           (weight /
                            (float)MOTION_LINE_OUTER_WEIGHT);
    requestedAdjustMMps += MotionLine_TuneWeightKd *
                           ((weight - s_context.previousWeight) / dt);
    s_context.previousWeight = weight;

    speedAdjustMMps = MotionLine_Approach(
        s_context.speedAdjustMMps, requestedAdjustMMps,
        MOTION_LINE_MAX_ADJUST_RATE_MMPS2 * dt);

    /* 阻尼过强时不允许反向超过巡航速度，防止单轮猛烈倒转。 */
    if (speedAdjustMMps > s_context.profileSpeedMMps)
    {
        speedAdjustMMps = s_context.profileSpeedMMps;
    }
    else if (speedAdjustMMps < -s_context.profileSpeedMMps)
    {
        speedAdjustMMps = -s_context.profileSpeedMMps;
    }
    s_context.speedAdjustMMps = speedAdjustMMps;

    /* 左侧压线：左轮减速、右轮加速；右侧压线时相反。 */
    *leftSpeedMMps = s_context.profileSpeedMMps + speedAdjustMMps;
    *rightSpeedMMps = s_context.profileSpeedMMps - speedAdjustMMps;
    s_context.lastLeftSpeedMMps = *leftSpeedMMps;
    s_context.lastRightSpeedMMps = *rightSpeedMMps;
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
        (MotionLine_ParametersAreValid() == 0U))
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

    MotionWheel_Stop();
    MotionLine_ResetControl();
    s_context.requestedSpeedMMps =
        (speedMMps > MOTION_LINE_MAX_SPEED_MMPS) ?
            MOTION_LINE_MAX_SPEED_MMPS : speedMMps;
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

    s_context.requestedSpeedMMps =
        (speedMMps > MOTION_LINE_MAX_SPEED_MMPS) ?
            MOTION_LINE_MAX_SPEED_MMPS : speedMMps;
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
        /* 25E 等流程把确认丢线作为巡线任务的正常结束条件。 */
        MotionWheel_Stop();
        s_context.error = MOTION_LINE_ERROR_NONE;
        s_context.state = MOTION_LINE_STATE_FINISHED;
        return;
    }
    if ((s_context.requestedSpeedMMps <= 0.001f) &&
        (s_context.profileSpeedMMps <= 0.001f))
    {
        /* 速度斜坡已平滑归零；上层可在下一拍接管短暂主动刹车。 */
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

float MotionLine_GetRequestedSpeedMMps(void)
{
    return s_context.requestedSpeedMMps;
}

float MotionLine_GetProfileSpeedMMps(void)
{
    return s_context.profileSpeedMMps;
}

float MotionLine_GetProfileAccelerationMMps2(void)
{
    return s_context.profileAccelerationMMps2;
}
