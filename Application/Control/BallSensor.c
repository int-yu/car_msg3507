#include "Application/Control/BallSensor.h"
#include "Application/Comms/K230Link.h"
#include <math.h>

float BallSensor_TuneHalfLengthMM = BALL_SENSOR_HALF_LENGTH_MM;

typedef struct
{
    uint8_t fresh;
    uint8_t hasPrevious;
    float positionMM;
    float previousPositionMM;
    float speedMMps;
    /* 两个新序号之间累计的真实时间。相机约 25 fps、控制环 100 Hz，除以
     * 控制拍 dt 会把速度高估约 4 倍，必须用帧间实际间隔。 */
    float frameIntervalS;
    uint8_t previousSequence;
} BallSensor_Context_t;

static BallSensor_Context_t s_context;

static void BallSensor_MarkStale(void)
{
    /* 位置保留最后一次有效值只为显示和排查；速度必须归零，否则平衡环
     * 的阻尼项会拿一个陈旧速度继续推摆杆。 */
    s_context.fresh = 0U;
    s_context.hasPrevious = 0U;
    s_context.speedMMps = 0.0f;
    s_context.frameIntervalS = 0.0f;
}

void BallSensor_Init(void)
{
    s_context.fresh = 0U;
    s_context.hasPrevious = 0U;
    s_context.positionMM = 0.0f;
    s_context.previousPositionMM = 0.0f;
    s_context.speedMMps = 0.0f;
    s_context.frameIntervalS = 0.0f;
    s_context.previousSequence = 0U;
    BallSensor_TuneHalfLengthMM = BALL_SENSOR_HALF_LENGTH_MM;
}

void BallSensor_Update(float dt)
{
    K230Link_Target_t target;
    float positionMM;

    if ((!isfinite(dt)) || (dt <= 0.0f))
    {
        BallSensor_MarkStale();
        return;
    }

    /* GetTarget() 已经把超时挡掉；返回 0 时不写 target，不能读它。 */
    if (K230Link_GetTarget(&target) == 0U)
    {
        BallSensor_MarkStale();
        return;
    }
    if (target.valid == 0U)
    {
        BallSensor_MarkStale();
        return;
    }

    positionMM = ((float)target.offsetX * BallSensor_TuneHalfLengthMM) /
                 1000.0f;
    if ((!isfinite(positionMM)) ||
        (fabsf(positionMM) > BALL_SENSOR_VALID_LIMIT_MM))
    {
        BallSensor_MarkStale();
        return;
    }

    s_context.positionMM = positionMM;
    s_context.fresh = 1U;
    s_context.frameIntervalS += dt;

    /*
     * 只在序号变化时更新速度。相机约 25 fps 而控制环 100 Hz，同一帧会
     * 被读到约 4 次；对重复帧做差分会得到 0，把估计值一路拉向零并废掉
     * 阻尼项。
     */
    if (target.sequence != s_context.previousSequence)
    {
        if ((s_context.hasPrevious != 0U) &&
            (s_context.frameIntervalS > 0.0f))
        {
            float rawSpeedMMps =
                (positionMM - s_context.previousPositionMM) /
                s_context.frameIntervalS;

            if (isfinite(rawSpeedMMps))
            {
                s_context.speedMMps +=
                    BALL_SENSOR_SPEED_FILTER_ALPHA *
                    (rawSpeedMMps - s_context.speedMMps);
            }
        }
        s_context.previousPositionMM = positionMM;
        s_context.previousSequence = target.sequence;
        s_context.frameIntervalS = 0.0f;
        s_context.hasPrevious = 1U;
    }
}

uint8_t BallSensor_IsFresh(void)
{
    return s_context.fresh;
}

float BallSensor_GetPositionMM(void)
{
    return s_context.positionMM;
}

float BallSensor_GetSpeedMMps(void)
{
    return s_context.speedMMps;
}
