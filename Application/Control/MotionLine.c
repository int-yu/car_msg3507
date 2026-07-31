#include "Application/Control/MotionLine.h"
#include "Application/Control/MotionWheel.h"
#include "Application/State/Odometry.h"
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
    float maxAdjustRatio;
    float weightKd;
    float curveSpeedMMps;
    float curveHoldDistanceMM;
    float lineError;
    float filteredWeight;
    float previousWeight;
    float speedAdjustMMps;
    float curveEntryDistanceMM;
    float lastLeftSpeedMMps;
    float lastRightSpeedMMps;
    uint16_t lostTicks;
    uint8_t curveEntryTicks;
    MotionLine_PathState_t pathState;
    uint8_t configured;
} MotionLine_Context_t;

/* Param 写入的是下一次启动值；Start 时校验并快照到 s_context。 */
float MotionLine_TuneMaxAdjustRatio = MOTION_LINE_MAX_ADJUST_RATIO;
float MotionLine_TuneWeightKd = 1.0f;
float MotionLine_TuneCurveSpeedMMps = MOTION_LINE_CURVE_SPEED_MMPS;
float MotionLine_TuneCurveHoldDistanceMM =
    MOTION_LINE_CURVE_HOLD_DISTANCE_MM;
float MotionLine_TuneAccelerationMMps2 = MOTION_LINE_ACCELERATION_MMPS2;
float MotionLine_TuneDecelerationMMps2 = MOTION_LINE_DECELERATION_MMPS2;

static MotionLine_Context_t s_context = {
    .state = MOTION_LINE_STATE_IDLE,
    .error = MOTION_LINE_ERROR_NONE,
};

static uint8_t MotionLine_ParametersAreValid(void)
{
    if ((!isfinite(MOTION_LINE_MAX_SPEED_MMPS)) ||
        (!isfinite(MOTION_LINE_CURVE_MIN_SPEED_RATIO)) ||
        (!isfinite(MOTION_LINE_WEIGHT_FILTER_ALPHA)) ||
        (!isfinite(MOTION_LINE_MAX_ADJUST_RATE_MMPS2)))
    {
        return 0U;
    }

    if ((MOTION_LINE_MAX_SPEED_MMPS <= 0.0f) ||
        (MOTION_LINE_CURVE_MIN_SPEED_RATIO <= 0.0f) ||
        (MOTION_LINE_CURVE_MIN_SPEED_RATIO > 1.0f) ||
        (MOTION_LINE_WEIGHT_FILTER_ALPHA <= 0.0f) ||
        (MOTION_LINE_WEIGHT_FILTER_ALPHA > 1.0f) ||
        (MOTION_LINE_MAX_ADJUST_RATE_MMPS2 <= 0.0f) ||
        (MOTION_LINE_CURVE_TRIGGER_MASK == 0U) ||
        (MOTION_LINE_CURVE_ENTRY_CONFIRM_TICKS == 0U) ||
        (MOTION_LINE_OUTER_WEIGHT <= 0) ||
        (MOTION_LINE_INNER_WEIGHT <= 0) ||
        (MOTION_LINE_INNER_WEIGHT >= MOTION_LINE_OUTER_WEIGHT) ||
        (MOTION_LINE_LOST_CONFIRM_TICKS == 0U))
    {
        return 0U;
    }

    return 1U;
}

static uint8_t MotionLine_TuningsAreValid(void)
{
    if ((!isfinite(MotionLine_TuneMaxAdjustRatio)) ||
        (!isfinite(MotionLine_TuneWeightKd)) ||
        (!isfinite(MotionLine_TuneCurveSpeedMMps)) ||
        (!isfinite(MotionLine_TuneCurveHoldDistanceMM)) ||
        (!isfinite(MotionLine_TuneAccelerationMMps2)) ||
        (!isfinite(MotionLine_TuneDecelerationMMps2)))
    {
        return 0U;
    }

    if ((MotionLine_TuneMaxAdjustRatio <= 0.0f) ||
        (MotionLine_TuneMaxAdjustRatio > 1.0f) ||
        (MotionLine_TuneWeightKd < 0.0f) ||
        (MotionLine_TuneCurveSpeedMMps <= 0.0f) ||
        (MotionLine_TuneCurveSpeedMMps > MOTION_LINE_MAX_SPEED_MMPS) ||
        (MotionLine_TuneCurveHoldDistanceMM <= 0.0f) ||
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
    s_context.maxAdjustRatio = MotionLine_TuneMaxAdjustRatio;
    s_context.weightKd = MotionLine_TuneWeightKd;
    s_context.curveSpeedMMps = MotionLine_TuneCurveSpeedMMps;
    s_context.curveHoldDistanceMM = MotionLine_TuneCurveHoldDistanceMM;
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
    s_context.curveEntryDistanceMM = 0.0f;
    s_context.lastLeftSpeedMMps = 0.0f;
    s_context.lastRightSpeedMMps = 0.0f;
    s_context.lostTicks = 0U;
    s_context.curveEntryTicks = 0U;
    s_context.pathState = MOTION_LINE_PATH_STRAIGHT;
}

static void MotionLine_SetError(MotionLine_Error_t error)
{
    MotionWheel_Stop();
    MotionLine_ResetControl();
    s_context.error = error;
    s_context.state = MOTION_LINE_STATE_ERROR;
}

/*
 * 教程协议为 bit0=CH1 ... bit5=CH6。实车俯视（探头朝前）时 CH1 在左侧、
 * CH6 在右侧，对应 GRAYDETECT_CHANNEL1_IS_RIGHT 为 0 的下面一张表：
 * 左侧通道取负权重，压到左侧时左轮减速、右轮加速，把车拉回线上。
 * 传感器翻面安装时改 Graydetect.h 的那个宏即可，这里两张表都在。
 */
#if GRAYDETECT_CHANNEL1_IS_RIGHT
static const float s_grayWeight[GRAY_CHANNEL_COUNT] = {
     MOTION_LINE_OUTER_WEIGHT, 4, MOTION_LINE_INNER_WEIGHT,
    -MOTION_LINE_INNER_WEIGHT, -4, -MOTION_LINE_OUTER_WEIGHT
};
#else
static const float s_grayWeight[GRAY_CHANNEL_COUNT] = {
    -MOTION_LINE_OUTER_WEIGHT, -4, -MOTION_LINE_INNER_WEIGHT,
     MOTION_LINE_INNER_WEIGHT, 4, MOTION_LINE_OUTER_WEIGHT
};
#endif

static float MotionLine_GetWeight(uint8_t grayState)
{
    float weight = 0.0f;
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

    return weight;
}

static float MotionLine_GetCurveTargetSpeed(float weight)
{
    float errorCurveRatio;
    float normalizedWeight = fabsf(weight) /
                             (float)MOTION_LINE_OUTER_WEIGHT;

    if (s_context.pathState == MOTION_LINE_PATH_CURVE)
    {
        /* 弧线速度由已确认的状态和 lcv 决定，不能因压线回中又提速。 */
        return (s_context.requestedSpeedMMps < s_context.curveSpeedMMps) ?
            s_context.requestedSpeedMMps : s_context.curveSpeedMMps;
    }

    if (normalizedWeight > 1.0f)
    {
        normalizedWeight = 1.0f;
    }
    errorCurveRatio = 1.0f -
                  (1.0f - MOTION_LINE_CURVE_MIN_SPEED_RATIO) *
                  normalizedWeight;
    return s_context.requestedSpeedMMps * errorCurveRatio;
}

static uint8_t MotionLine_AddSaturatingTick(uint8_t value)
{
    return (value < UINT8_MAX) ? (uint8_t)(value + 1U) : value;
}

/* CH2/CH5 连续压线后锁住弧线低速区。退出只由编码器累计距离决定，
 * 避免 MPU 漂移或红外回中导致控制参数/速度在同一弧线内反复切换。 */
static void MotionLine_UpdatePathState(uint8_t grayState)
{
    if (s_context.pathState == MOTION_LINE_PATH_CURVE)
    {
        s_context.curveEntryTicks = 0U;
        if (fabsf(Odometry_GetDistanceMM() -
                  s_context.curveEntryDistanceMM) >=
            s_context.curveHoldDistanceMM)
        {
            s_context.pathState = MOTION_LINE_PATH_STRAIGHT;
        }
        return;
    }

    if ((grayState & MOTION_LINE_CURVE_TRIGGER_MASK) != 0U)
    {
        s_context.curveEntryTicks =
            MotionLine_AddSaturatingTick(s_context.curveEntryTicks);
        if (s_context.curveEntryTicks >=
            MOTION_LINE_CURVE_ENTRY_CONFIRM_TICKS)
        {
            s_context.pathState = MOTION_LINE_PATH_CURVE;
            s_context.curveEntryDistanceMM = Odometry_GetDistanceMM();
            s_context.curveEntryTicks = 0U;
        }
    }
    else
    {
        s_context.curveEntryTicks = 0U;
    }
}

static float MotionLine_UpdateProfileSpeed(float targetSpeedMMps, float dt)
{
    float maximumStep = (targetSpeedMMps > s_context.profileSpeedMMps) ?
        (s_context.accelerationMMps2 * dt) :
        (s_context.decelerationMMps2 * dt);
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

    MotionLine_UpdatePathState(grayState);

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
        rawWeight = MotionLine_GetWeight(grayState);
    }

    s_context.filteredWeight += MOTION_LINE_WEIGHT_FILTER_ALPHA *
        (rawWeight - s_context.filteredWeight);
    weight = s_context.filteredWeight;
    s_context.lineError = weight;

    targetSpeedMMps = MotionLine_GetCurveTargetSpeed(weight);
    (void)MotionLine_UpdateProfileSpeed(targetSpeedMMps, dt);

    /* 全程只使用一套 P/D；弧线状态只锁住低速保持距离，不切控制参数。 */
    requestedAdjustMMps = s_context.profileSpeedMMps *
                           s_context.maxAdjustRatio *
                           (weight /
                            (float)MOTION_LINE_OUTER_WEIGHT);
    requestedAdjustMMps += s_context.weightKd *
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

MotionLine_PathState_t MotionLine_GetPathState(void)
{
    return s_context.pathState;
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
