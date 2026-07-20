#include "Application/Control/MotionLine.h"
#include "Application/Control/MotionWheel.h"
#include "Hardware/Sensors/Graydetect.h"
#include <math.h>

typedef struct
{
    MotionLine_State_t state;
    MotionLine_Error_t error;
    float cruiseSpeedMMps;
    float lineError;
    float previousWeight;
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
        (!isfinite(MOTION_LINE_MAX_ADJUST_RATIO)))
    {
        return 0U;
    }

    if ((MOTION_LINE_MAX_SPEED_MMPS <= 0.0f) ||
        (MOTION_LINE_MAX_ADJUST_RATIO <= 0.0f) ||
        (MOTION_LINE_MAX_ADJUST_RATIO > 1.0f) ||
        (MOTION_LINE_OUTER_WEIGHT <= 0) ||
        (MOTION_LINE_INNER_WEIGHT <= 0) ||
        (MOTION_LINE_INNER_WEIGHT >= MOTION_LINE_OUTER_WEIGHT) ||
        (MOTION_LINE_LOST_CONFIRM_TICKS == 0U))
    {
        return 0U;
    }

    return 1U;
}

static void MotionLine_ResetControl(void)
{
    s_context.cruiseSpeedMMps = 0.0f;
    s_context.lineError = 0.0f;
    s_context.previousWeight = 0.0f;
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
 * 位图从左到右为 bit0~bit4，灰度返回 1 表示检测到黑线。
 * 最外侧权重为正负 6，内侧权重为正负 3，中心权重为 0。
 */
static int8_t MotionLine_GetWeight(uint8_t grayState)
{
    int16_t weight = 0;

    if ((grayState & 0x01U) != 0U)
    {
        weight -= MOTION_LINE_OUTER_WEIGHT;
    }
    if ((grayState & 0x02U) != 0U)
    {
        weight -= MOTION_LINE_INNER_WEIGHT;
    }
    if ((grayState & 0x08U) != 0U)
    {
        weight += MOTION_LINE_INNER_WEIGHT;
    }
    if ((grayState & 0x10U) != 0U)
    {
        weight += MOTION_LINE_OUTER_WEIGHT;
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

static uint8_t MotionLine_CalculateTargetSpeeds(
    float *leftSpeedMMps, float *rightSpeedMMps, float dt)
{
    uint8_t grayState = Graydetect_GetState();
    float weight;
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

        /* 短暂丢线时保持上一拍的左右轮目标速度。 */
        *leftSpeedMMps = s_context.lastLeftSpeedMMps;
        *rightSpeedMMps = s_context.lastRightSpeedMMps;
        return 1U;
    }

    s_context.lostTicks = 0U;
    weight = (float)MotionLine_GetWeight(grayState);
    s_context.lineError = weight;

    /* 权重达到正负 6 时，速度增减比例等于最大调整比例；
     * 微分项对权重跳变（压线切换瞬间）施加一次性阻尼，默认 0 不生效。 */
    speedAdjustMMps = s_context.cruiseSpeedMMps *
                      MotionLine_TuneMaxAdjustRatio *
                      (weight / (float)MOTION_LINE_OUTER_WEIGHT);
    speedAdjustMMps += MotionLine_TuneWeightKd *
                       ((weight - s_context.previousWeight) / dt);
    s_context.previousWeight = weight;

    /* 阻尼过强时不允许反向超过巡航速度，防止单轮猛烈倒转。 */
    if (speedAdjustMMps > s_context.cruiseSpeedMMps)
    {
        speedAdjustMMps = s_context.cruiseSpeedMMps;
    }
    else if (speedAdjustMMps < -s_context.cruiseSpeedMMps)
    {
        speedAdjustMMps = -s_context.cruiseSpeedMMps;
    }

    /* 左侧压线：左轮减速、右轮加速；右侧压线时相反。 */
    *leftSpeedMMps = s_context.cruiseSpeedMMps + speedAdjustMMps;
    *rightSpeedMMps = s_context.cruiseSpeedMMps - speedAdjustMMps;
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
    s_context.cruiseSpeedMMps =
        (speedMMps > MOTION_LINE_MAX_SPEED_MMPS) ?
            MOTION_LINE_MAX_SPEED_MMPS : speedMMps;
    s_context.error = MOTION_LINE_ERROR_NONE;
    s_context.state = MOTION_LINE_STATE_RUNNING;
    return MOTION_LINE_RESULT_OK;
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
