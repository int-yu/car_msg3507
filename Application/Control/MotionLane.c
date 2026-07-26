#include "Application/Control/MotionLane.h"
#include "Application/Control/MotionWheel.h"
#include "Application/Comms/K230Link.h"
#include "Application/State/Heading.h"
#include <math.h>

typedef struct
{
    MotionLane_State_t state;
    MotionLane_Error_t error;
    float cruiseSpeedMMps;
    float laneError;
    float adjustMMps;
    float lastLeftSpeedMMps;
    float lastRightSpeedMMps;
    uint16_t lostTicks;
    uint8_t laneBand;   /* 上一次成功读取时选中的带号，供遥测/排查使用。 */
    uint8_t configured;
} MotionLane_Context_t;

float MotionLane_TuneKp = MOTION_LANE_KP;
float MotionLane_TuneKdYaw = MOTION_LANE_KD_YAW;
float MotionLane_TuneMaxAdjustRatio = MOTION_LANE_MAX_ADJUST_RATIO;

static MotionLane_Context_t s_context = {
    .state = MOTION_LANE_STATE_IDLE,
    .error = MOTION_LANE_ERROR_NONE,
};

static uint8_t MotionLane_ParametersAreValid(void)
{
    if ((!isfinite(MOTION_LANE_MAX_SPEED_MMPS)) ||
        (!isfinite(MOTION_LANE_KP)) ||
        (!isfinite(MOTION_LANE_KD_YAW)) ||
        (!isfinite(MOTION_LANE_MAX_ADJUST_RATIO)))
    {
        return 0U;
    }

    if ((MOTION_LANE_MAX_SPEED_MMPS <= 0.0f) ||
        (MOTION_LANE_KP < 0.0f) ||
        (MOTION_LANE_KD_YAW < 0.0f) ||
        (MOTION_LANE_MAX_ADJUST_RATIO <= 0.0f) ||
        (MOTION_LANE_MAX_ADJUST_RATIO > 1.0f) ||
        (MOTION_LANE_LOST_HOLD_TICKS == 0U) ||
        (MOTION_LANE_MIN_CONFIDENCE > 100U))
    {
        return 0U;
    }

    return 1U;
}

static void MotionLane_ResetControl(void)
{
    s_context.cruiseSpeedMMps = 0.0f;
    s_context.laneError = 0.0f;
    s_context.adjustMMps = 0.0f;
    s_context.lastLeftSpeedMMps = 0.0f;
    s_context.lastRightSpeedMMps = 0.0f;
    s_context.lostTicks = 0U;
    s_context.laneBand = 0U;
}

static void MotionLane_SetError(MotionLane_Error_t error)
{
    MotionWheel_Stop();
    MotionLane_ResetControl();
    s_context.error = error;
    s_context.state = MOTION_LANE_STATE_ERROR;
}

/*
 * 判定本帧视觉是否可用。以下条件缺一不可：
 *   1. 链路新鲜（K230Link 自己的 ageTicks 未超过链路层 300 ms 超时）；
 *   2. 视觉数据不陈旧（ageTicks 未超过控制层自己更紧的
 *      MOTION_LANE_MAX_LANE_AGE_TICKS——链路层超时是给"链路还活着但
 *      偶尔丢帧"用的，控制律拿着更旧的数据打方向没有意义）；
 *   3. K230 自己认为这帧有效；
 *   4. 置信度达标；
 *   5. 五个带里至少有一个不是哨兵——取其中最近（编号最小）的一个。
 *
 * 第 5 条不能硬要 b0：最底那条采样行贴着中心线覆盖范围的边界，车头、
 * 阴影或赛道边沿遮住画面底部一点点就会让它变成哨兵，此时其余各带往往
 * 仍然有效，没有理由把整帧丢掉。代价是前视距离会随着退带而变远，同样
 * 的 Kp 反应会略钝——这远好过停车。选中的带号存进 s_context 供遥测/
 * 排查使用。
 */
static uint8_t MotionLane_ReadLane(K230Link_Lane_t *lane)
{
    uint8_t band;

    if (K230Link_GetLane(lane) == 0U)
    {
        return 0U;
    }
    if (lane->ageTicks > MOTION_LANE_MAX_LANE_AGE_TICKS)
    {
        return 0U;
    }
    if (lane->valid == 0U)
    {
        return 0U;
    }
    if (lane->confidence < MOTION_LANE_MIN_CONFIDENCE)
    {
        return 0U;
    }

    for (band = 0U; band < K230_LINK_LANE_BAND_COUNT; band++)
    {
        if ((lane->bandValid & (uint8_t)(1U << band)) != 0U)
        {
            break;
        }
    }
    if (band >= K230_LINK_LANE_BAND_COUNT)
    {
        return 0U;
    }
    s_context.laneBand = band;
    return 1U;
}

static uint8_t MotionLane_CalculateTargetSpeeds(
    float *leftSpeedMMps, float *rightSpeedMMps)
{
    K230Link_Lane_t lane;
    float adjustMMps;
    float adjustLimitMMps;

    if (MotionLane_ReadLane(&lane) == 0U)
    {
        if (s_context.lostTicks < MOTION_LANE_LOST_HOLD_TICKS)
        {
            s_context.lostTicks++;
        }
        if (s_context.lostTicks >= MOTION_LANE_LOST_HOLD_TICKS)
        {
            return 0U;
        }

        /* 短暂丢失时保持上一拍的左右轮目标速度，与 MotionLine 一致。 */
        *leftSpeedMMps = s_context.lastLeftSpeedMMps;
        *rightSpeedMMps = s_context.lastRightSpeedMMps;
        return 1U;
    }

    s_context.lostTicks = 0U;

    /*
     * K230 发的是「画面中心 - 车道中心」，车道偏右时为负。这里翻成
     * 「车道偏右为正」，与下面 adjust 正=右转 的约定对齐。
     */
    s_context.laneError = -(float)lane.offsetPermille[s_context.laneBand];

    /*
     * 比例项来自视觉（约 25 Hz，两帧之间是零阶保持），微分阻尼来自陀螺仪
     * （100 Hz）。不差分视觉误差：差分零阶保持信号会产生周期性尖峰。
     */
    adjustMMps = MotionLane_TuneKp * s_context.laneError -
                 MotionLane_TuneKdYaw * Heading_GetYawRate();

    /* 限幅防止单轮反向猛转，与 MotionLine 的 MAX_ADJUST_RATIO 同义。 */
    adjustLimitMMps = s_context.cruiseSpeedMMps *
                      MotionLane_TuneMaxAdjustRatio;
    if (adjustMMps > adjustLimitMMps)
    {
        adjustMMps = adjustLimitMMps;
    }
    else if (adjustMMps < -adjustLimitMMps)
    {
        adjustMMps = -adjustLimitMMps;
    }
    s_context.adjustMMps = adjustMMps;

    /* 车道中心偏右：左轮加速、右轮减速，车向右修正。 */
    *leftSpeedMMps = s_context.cruiseSpeedMMps + adjustMMps;
    *rightSpeedMMps = s_context.cruiseSpeedMMps - adjustMMps;
    s_context.lastLeftSpeedMMps = *leftSpeedMMps;
    s_context.lastRightSpeedMMps = *rightSpeedMMps;
    return 1U;
}

static MotionWheel_Result_t MotionLane_ApplyWheelCommand(
    float leftSpeedMMps, float rightSpeedMMps, float dt)
{
    MotionWheel_Command_t command;

    command.targetSpeedLMMps = leftSpeedMMps;
    command.targetSpeedRMMps = rightSpeedMMps;
    command.trimLPWM = 0.0f;
    command.trimRPWM = 0.0f;
    return MotionWheel_Update(&command, dt);
}

MotionLane_Result_t MotionLane_Init(void)
{
    MotionWheel_Result_t wheelResult;

    s_context.configured = 0U;
    s_context.state = MOTION_LANE_STATE_IDLE;
    s_context.error = MOTION_LANE_ERROR_NONE;
    MotionLane_ResetControl();

    wheelResult = MotionWheel_Init();
    if ((wheelResult != MOTION_WHEEL_RESULT_OK) ||
        (MotionLane_ParametersAreValid() == 0U))
    {
        return MOTION_LANE_RESULT_INVALID_ARGUMENT;
    }

    s_context.configured = 1U;
    return MOTION_LANE_RESULT_OK;
}

MotionLane_Result_t MotionLane_Start(float speedMMps)
{
    if (s_context.configured == 0U)
    {
        return MOTION_LANE_RESULT_NOT_CONFIGURED;
    }
    if (MotionLane_IsBusy() != 0U)
    {
        return MOTION_LANE_RESULT_BUSY;
    }
    if ((!isfinite(speedMMps)) || (speedMMps <= 0.0f))
    {
        return MOTION_LANE_RESULT_INVALID_ARGUMENT;
    }

    MotionWheel_Stop();
    MotionLane_ResetControl();
    s_context.cruiseSpeedMMps =
        (speedMMps > MOTION_LANE_MAX_SPEED_MMPS) ?
            MOTION_LANE_MAX_SPEED_MMPS : speedMMps;
    s_context.error = MOTION_LANE_ERROR_NONE;
    s_context.state = MOTION_LANE_STATE_RUNNING;
    return MOTION_LANE_RESULT_OK;
}

void MotionLane_Update(float dt)
{
    float leftSpeedMMps;
    float rightSpeedMMps;

    if (s_context.state != MOTION_LANE_STATE_RUNNING)
    {
        return;
    }
    if ((!isfinite(dt)) || (dt <= 0.0f))
    {
        MotionLane_SetError(MOTION_LANE_ERROR_UPDATE_PERIOD_INVALID);
        return;
    }
    if (MotionLane_CalculateTargetSpeeds(
            &leftSpeedMMps, &rightSpeedMMps) == 0U)
    {
        /* 确认丢道是巡道任务的正常结束条件，与 MotionLine 一致。
         * 同时要清掉遥测量：不复位的话 vx/vad 会冻结在最后一次有效值，
         * 标定时容易被误读成"车已经停了但视觉还在报误差"。 */
        MotionWheel_Stop();
        MotionLane_ResetControl();
        s_context.error = MOTION_LANE_ERROR_NONE;
        s_context.state = MOTION_LANE_STATE_FINISHED;
        return;
    }
    if (MotionLane_ApplyWheelCommand(
            leftSpeedMMps, rightSpeedMMps, dt) != MOTION_WHEEL_RESULT_OK)
    {
        MotionLane_SetError(MOTION_LANE_ERROR_WHEEL);
    }
}

void MotionLane_Stop(void)
{
    MotionWheel_Stop();
    MotionLane_ResetControl();
    s_context.error = MOTION_LANE_ERROR_NONE;
    s_context.state = MOTION_LANE_STATE_IDLE;
}

uint8_t MotionLane_IsConfigured(void)
{
    return s_context.configured;
}

uint8_t MotionLane_IsBusy(void)
{
    return (s_context.state == MOTION_LANE_STATE_RUNNING) ? 1U : 0U;
}

uint8_t MotionLane_IsFinished(void)
{
    return (s_context.state == MOTION_LANE_STATE_FINISHED) ? 1U : 0U;
}

MotionLane_State_t MotionLane_GetState(void)
{
    return s_context.state;
}

MotionLane_Error_t MotionLane_GetError(void)
{
    return s_context.error;
}

float MotionLane_GetLaneError(void)
{
    return s_context.laneError;
}

float MotionLane_GetAdjustMMps(void)
{
    return s_context.adjustMMps;
}
