#include "Application/Control/MotionStraight.h"
#include "Application/Control/MotionStraightConfig.h"
#include "Application/Control/MotionWheel.h"
#include "Application/Control/PID.h"
#include "Application/State/Heading.h"
#include "Application/State/Odometry.h"
#include "Hardware/Motor/Motor.h"
#include <math.h>
#include <stddef.h>

/* 直线任务的全部运行状态集中保存，便于检查状态转换和速度规划。 */
typedef struct
{
    MotionStraight_State_t state;
    MotionStraight_Error_t error;
    uint8_t configured;

    float startDistanceLMM;
    float startDistanceRMM;
    float totalDistanceMM;
    float remainingDistanceMM;

    float cruiseSpeedMMps;
    float endSpeedMMps;
    float profileSpeedMMps;
    float decelerationStartDistanceMM;
    float effectiveDecelerationMMps2;

    float targetHeadingDeg;
    float direction;
    uint8_t decelerationActive;
    uint8_t targetReached;
} MotionStraight_Context_t;

static MotionStraight_Config_t s_config;
static PID_t s_headingPID;
static MotionStraight_Context_t s_context = {
    .state = MOTION_STRAIGHT_STATE_IDLE,
    .error = MOTION_STRAIGHT_ERROR_NONE,
};

static float MotionStraight_Clamp(float value, float minimum, float maximum)
{
    if (value < minimum)
    {
        return minimum;
    }
    if (value > maximum)
    {
        return maximum;
    }
    return value;
}

static float MotionStraight_Approach(
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

/* 检查所有公共调参，防止非法值进入速度规划和电机控制。 */
static uint8_t MotionStraight_ConfigIsValid(
    const MotionStraight_Config_t *config)
{
    if (config == NULL)
    {
        return 0U;
    }
    if ((!isfinite(config->maximumSpeedMMps)) ||
        (!isfinite(config->accelerationMMps2)) ||
        (!isfinite(config->decelerationMMps2)) ||
        (!isfinite(config->decelerationStartRatio)) ||
        (!isfinite(config->distanceToleranceMM)) ||
        (!isfinite(config->heading.kp)) ||
        (!isfinite(config->heading.kd)) ||
        (!isfinite(config->heading.correctionLimitPWM)))
    {
        return 0U;
    }
    if ((config->maximumSpeedMMps <= 0.0f) ||
        (config->accelerationMMps2 <= 0.0f) ||
        (config->decelerationMMps2 <= 0.0f) ||
        (config->decelerationStartRatio <= 0.0f) ||
        (config->decelerationStartRatio >= 1.0f) ||
        (config->distanceToleranceMM < 0.0f) ||
        (config->heading.kp < 0.0f) ||
        (config->heading.kd < 0.0f) ||
        ((config->heading.kp == 0.0f) &&
         (config->heading.kd == 0.0f)) ||
        (config->heading.correctionLimitPWM <= 0.0f) ||
        (config->heading.correctionLimitPWM >
         MotionWheel_GetMaximumCommandPWM()) ||
        ((config->heading.correctionSign != 1) &&
         (config->heading.correctionSign != -1)))
    {
        return 0U;
    }
    return 1U;
}

static void MotionStraight_ResetControllers(void)
{
    MotionWheel_Reset();
    PID_Reset(&s_headingPID);
    s_context.profileSpeedMMps = 0.0f;
}

static void MotionStraight_SetError(MotionStraight_Error_t error)
{
    MotionWheel_Stop();
    s_context.error = error;
    s_context.state = MOTION_STRAIGHT_STATE_ERROR;
}

/* 根据距离、巡航速度和终点速度计算本次任务的减速起点。 */
static void MotionStraight_PrepareProfile(
    float distanceMM, float speedMMps, float endSpeedMMps)
{
    float preferredDecelerationDistanceMM;
    float requiredDecelerationDistanceMM;
    float speedSquaredDifference;

    s_context.startDistanceLMM = Odometry_GetDistanceLMM();
    s_context.startDistanceRMM = Odometry_GetDistanceRMM();
    s_context.totalDistanceMM = fabsf(distanceMM);
    s_context.remainingDistanceMM = distanceMM;
    s_context.direction = (distanceMM >= 0.0f) ? 1.0f : -1.0f;

    s_context.cruiseSpeedMMps = MotionStraight_Clamp(
        speedMMps, 0.0f, s_config.maximumSpeedMMps);
    s_context.endSpeedMMps = MotionStraight_Clamp(
        endSpeedMMps, 0.0f, s_context.cruiseSpeedMMps);

    preferredDecelerationDistanceMM = s_context.totalDistanceMM *
        (1.0f - s_config.decelerationStartRatio);
    speedSquaredDifference =
        s_context.cruiseSpeedMMps * s_context.cruiseSpeedMMps -
        s_context.endSpeedMMps * s_context.endSpeedMMps;
    requiredDecelerationDistanceMM = speedSquaredDifference /
        (2.0f * s_config.decelerationMMps2);

    if (requiredDecelerationDistanceMM > preferredDecelerationDistanceMM)
    {
        preferredDecelerationDistanceMM = requiredDecelerationDistanceMM;
    }
    preferredDecelerationDistanceMM = MotionStraight_Clamp(
        preferredDecelerationDistanceMM,
        0.0f, s_context.totalDistanceMM);

    s_context.decelerationStartDistanceMM =
        s_context.totalDistanceMM - preferredDecelerationDistanceMM;
    s_context.effectiveDecelerationMMps2 =
        s_config.decelerationMMps2;
    s_context.decelerationActive = 0U;
    s_context.targetReached = 0U;
}

static float MotionStraight_GetDirectedTravelledDistance(void)
{
    float distanceLMM = Odometry_GetDistanceLMM() -
                        s_context.startDistanceLMM;
    float distanceRMM = Odometry_GetDistanceRMM() -
                        s_context.startDistanceRMM;
    float averageDistanceMM = (distanceLMM + distanceRMM) * 0.5f;

    return s_context.direction * averageDistanceMM;
}

/*
 * 在首选减速点启动减速。若最大减速度不足以在最后 1/6 完成降速，
 * PrepareProfile() 已经按运动学距离把减速点提前。
 */
static void MotionStraight_StartDecelerationIfNeeded(
    float travelledDistanceMM, float remainingDistanceMM)
{
    float currentSpeedMMps;

    if ((s_context.decelerationActive != 0U) ||
        (travelledDistanceMM < s_context.decelerationStartDistanceMM))
    {
        return;
    }

    s_context.decelerationActive = 1U;
    currentSpeedMMps = fabsf(s_context.profileSpeedMMps);
    if ((currentSpeedMMps > s_context.endSpeedMMps) &&
        (remainingDistanceMM > 0.001f))
    {
        s_context.effectiveDecelerationMMps2 =
            (currentSpeedMMps * currentSpeedMMps -
             s_context.endSpeedMMps * s_context.endSpeedMMps) /
            (2.0f * remainingDistanceMM);
        s_context.effectiveDecelerationMMps2 = MotionStraight_Clamp(
            s_context.effectiveDecelerationMMps2,
            0.001f, s_config.decelerationMMps2);
    }
}

/* 距离规划层：返回本周期应跟踪的正速度大小。 */
static float MotionStraight_CalculateTargetSpeedMagnitude(void)
{
    float travelledDistanceMM;
    float remainingDistanceMM;
    float brakingSpeedMMps;

    if (s_context.state == MOTION_STRAIGHT_STATE_CONTINUING)
    {
        s_context.remainingDistanceMM = 0.0f;
        return s_context.endSpeedMMps;
    }

    travelledDistanceMM = MotionStraight_GetDirectedTravelledDistance();
    remainingDistanceMM = s_context.totalDistanceMM -
                          travelledDistanceMM;
    s_context.remainingDistanceMM =
        s_context.direction * remainingDistanceMM;

    MotionStraight_StartDecelerationIfNeeded(
        travelledDistanceMM, remainingDistanceMM);

    if (remainingDistanceMM <= s_config.distanceToleranceMM)
    {
        /* 到点后继续完成速度斜坡，但不再调用满占空比主动制动。 */
        s_context.targetReached = 1U;
        return s_context.endSpeedMMps;
    }
    if (s_context.decelerationActive == 0U)
    {
        return s_context.cruiseSpeedMMps;
    }

    brakingSpeedMMps = sqrtf(
        s_context.endSpeedMMps * s_context.endSpeedMMps +
        2.0f * s_context.effectiveDecelerationMMps2 *
        remainingDistanceMM);
    return MotionStraight_Clamp(
        brakingSpeedMMps,
        s_context.endSpeedMMps,
        s_context.cruiseSpeedMMps);
}

static float MotionStraight_UpdateProfileSpeed(
    float targetSpeedMMps, float dt)
{
    float speedStepMMps;

    if (fabsf(targetSpeedMMps) > fabsf(s_context.profileSpeedMMps))
    {
        speedStepMMps = s_config.accelerationMMps2 * dt;
    }
    else
    {
        speedStepMMps = s_config.decelerationMMps2 * dt;
    }
    s_context.profileSpeedMMps = MotionStraight_Approach(
        s_context.profileSpeedMMps, targetSpeedMMps, speedStepMMps);
    return s_context.profileSpeedMMps;
}

/* 返回 1 表示已经完成零速停车，本周期不再向车轮层发送指令。 */
static uint8_t MotionStraight_FinishProfileIfReady(float targetSpeedMMps)
{
    if ((s_context.state != MOTION_STRAIGHT_STATE_RUNNING) ||
        (s_context.targetReached == 0U) ||
        (fabsf(s_context.profileSpeedMMps - targetSpeedMMps) > 0.001f))
    {
        return 0U;
    }

    s_context.remainingDistanceMM = 0.0f;
    if (s_context.endSpeedMMps <= 0.001f)
    {
        /* PWM 归零并释放方向脚，依靠滑行停车，避免最后猛刹。 */
        MotionWheel_Stop();
        s_context.state = MOTION_STRAIGHT_STATE_COMPLETED;
        return 1U;
    }

    s_context.state = MOTION_STRAIGHT_STATE_CONTINUING;
    return 0U;
}

static MotionWheel_Result_t MotionStraight_ApplyWheelCommand(float dt)
{
    MotionWheel_Command_t command;
    float headingCorrectionPWM;

    /* 连续累计角不做 ±180° 归一化，因此可直接完成多圈航向保持。 */
    headingCorrectionPWM = PID_Update(
        &s_headingPID,
        s_context.targetHeadingDeg,
        Heading_GetYaw(), dt);
    headingCorrectionPWM *= (float)s_config.heading.correctionSign;

    command.targetSpeedLMMps = s_context.profileSpeedMMps;
    command.targetSpeedRMMps = s_context.profileSpeedMMps;
    command.trimLPWM = -headingCorrectionPWM;
    command.trimRPWM = headingCorrectionPWM;
    return MotionWheel_Update(&command, dt);
}

MotionStraight_Result_t MotionStraight_Init(
    const MotionStraight_Config_t *config)
{
    MotionWheel_Result_t wheelResult;

    Motor_StopAll();
    s_context.configured = 0U;
    s_context.state = MOTION_STRAIGHT_STATE_IDLE;
    s_context.error = MOTION_STRAIGHT_ERROR_NONE;

    wheelResult = MotionWheel_InitDefault();
    if ((wheelResult != MOTION_WHEEL_RESULT_OK) ||
        (MotionStraight_ConfigIsValid(config) == 0U))
    {
        return MOTION_STRAIGHT_RESULT_INVALID_ARGUMENT;
    }

    s_config = *config;
    PID_Init(&s_headingPID,
             s_config.heading.kp, 0.0f, s_config.heading.kd,
             s_config.heading.correctionLimitPWM, 0.0f);
    MotionStraight_ResetControllers();
    s_context.remainingDistanceMM = 0.0f;
    s_context.configured = 1U;
    return MOTION_STRAIGHT_RESULT_OK;
}

MotionStraight_Result_t MotionStraight_InitDefault(void)
{
    return MotionStraight_Init(&g_motionStraightConfig);
}

MotionStraight_Result_t MotionStraight_Start(
    float distanceMM, float speedMMps, float endSpeedMMps)
{
    if (s_context.configured == 0U)
    {
        return MOTION_STRAIGHT_RESULT_NOT_CONFIGURED;
    }
    if (MotionStraight_IsBusy() != 0U)
    {
        return MOTION_STRAIGHT_RESULT_BUSY;
    }
    if ((!isfinite(distanceMM)) || (!isfinite(speedMMps)) ||
        (!isfinite(endSpeedMMps)) || (speedMMps <= 0.0f) ||
        (endSpeedMMps < 0.0f) || (endSpeedMMps > speedMMps))
    {
        return MOTION_STRAIGHT_RESULT_INVALID_ARGUMENT;
    }
    if ((Heading_IsReady() == 0U) ||
        (Odometry_CountsPerMM <= 0.001f))
    {
        return MOTION_STRAIGHT_RESULT_SENSOR_NOT_READY;
    }

    MotionWheel_Stop();
    MotionStraight_ResetControllers();
    s_context.error = MOTION_STRAIGHT_ERROR_NONE;
    s_context.targetHeadingDeg = Heading_GetYaw();
    MotionStraight_PrepareProfile(distanceMM, speedMMps, endSpeedMMps);

    if (fabsf(distanceMM) <= s_config.distanceToleranceMM)
    {
        s_context.remainingDistanceMM = 0.0f;
        if (s_context.endSpeedMMps <= 0.001f)
        {
            s_context.state = MOTION_STRAIGHT_STATE_COMPLETED;
        }
        else
        {
            /* 极短距离任务也通过加速度斜坡进入终点持续速度。 */
            s_context.decelerationActive = 1U;
            s_context.targetReached = 1U;
            s_context.state = MOTION_STRAIGHT_STATE_RUNNING;
        }
        return MOTION_STRAIGHT_RESULT_OK;
    }

    s_context.state = MOTION_STRAIGHT_STATE_RUNNING;
    return MOTION_STRAIGHT_RESULT_OK;
}

MotionStraight_Result_t MotionStraight_StartForward(
    uint32_t distanceMM, float speedMMps, float endSpeedMMps)
{
    return MotionStraight_Start(
        (float)distanceMM, speedMMps, endSpeedMMps);
}

MotionStraight_Result_t MotionStraight_StartBackward(
    uint32_t distanceMM, float speedMMps, float endSpeedMMps)
{
    return MotionStraight_Start(
        -(float)distanceMM, speedMMps, endSpeedMMps);
}

void MotionStraight_Update(float dt)
{
    float targetSpeedMagnitudeMMps;
    float targetSpeedMMps;

    if ((s_context.state != MOTION_STRAIGHT_STATE_RUNNING) &&
        (s_context.state != MOTION_STRAIGHT_STATE_CONTINUING))
    {
        return;
    }
    if ((!isfinite(dt)) || (dt <= 0.0f))
    {
        MotionStraight_SetError(
            MOTION_STRAIGHT_ERROR_UPDATE_PERIOD_INVALID);
        return;
    }
    if (Heading_IsReady() == 0U)
    {
        MotionStraight_SetError(MOTION_STRAIGHT_ERROR_HEADING_OFFLINE);
        return;
    }
    if (Odometry_CountsPerMM <= 0.001f)
    {
        MotionStraight_SetError(MOTION_STRAIGHT_ERROR_ODOMETRY_INVALID);
        return;
    }

    targetSpeedMagnitudeMMps =
        MotionStraight_CalculateTargetSpeedMagnitude();
    targetSpeedMMps = s_context.direction * targetSpeedMagnitudeMMps;
    MotionStraight_UpdateProfileSpeed(targetSpeedMMps, dt);

    if (MotionStraight_FinishProfileIfReady(targetSpeedMMps) != 0U)
    {
        return;
    }
    if (MotionStraight_ApplyWheelCommand(dt) != MOTION_WHEEL_RESULT_OK)
    {
        MotionStraight_SetError(MOTION_STRAIGHT_ERROR_WHEEL);
    }
}

void MotionStraight_Stop(void)
{
    MotionWheel_Stop();
    MotionStraight_ResetControllers();
    s_context.remainingDistanceMM = 0.0f;
    s_context.error = MOTION_STRAIGHT_ERROR_NONE;
    s_context.state = MOTION_STRAIGHT_STATE_IDLE;
}

uint8_t MotionStraight_IsConfigured(void)
{
    return s_context.configured;
}

uint8_t MotionStraight_IsBusy(void)
{
    return ((s_context.state == MOTION_STRAIGHT_STATE_RUNNING) ||
            (s_context.state == MOTION_STRAIGHT_STATE_CONTINUING)) ?
               1U : 0U;
}

uint8_t MotionStraight_IsFinished(void)
{
    return ((s_context.state == MOTION_STRAIGHT_STATE_COMPLETED) ||
            (s_context.state == MOTION_STRAIGHT_STATE_CONTINUING)) ?
               1U : 0U;
}

MotionStraight_State_t MotionStraight_GetState(void)
{
    return s_context.state;
}

MotionStraight_Error_t MotionStraight_GetError(void)
{
    return s_context.error;
}

float MotionStraight_GetRemainingDistanceMM(void)
{
    return s_context.remainingDistanceMM;
}
