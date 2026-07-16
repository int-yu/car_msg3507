#include "Application/Control/MotionStraight.h"
#include "Application/Control/MotionStraightConfig.h"
#include "Application/Control/PID.h"
#include "Application/Control/MotionWheel.h"
#include "Application/State/Heading.h"
#include "Application/State/Odometry.h"
#include "Hardware/Motor/Motor.h"
#include <math.h>
#include <stddef.h>

static MotionStraight_Config_t s_config;
static PID_t s_headingPID;

static MotionStraight_State_t s_state = MOTION_STRAIGHT_STATE_IDLE;
static MotionStraight_Error_t s_error = MOTION_STRAIGHT_ERROR_NONE;
static uint8_t s_configured;

static float s_startDistanceLMM;
static float s_startDistanceRMM;
static float s_targetDistanceMM;
static float s_totalDistanceMM;
static float s_remainingDistanceMM;
static float s_cruiseSpeedMMps;
static float s_endSpeedMMps;
static float s_profileSpeedMMps;
static float s_decelerationStartDistanceMM;
static float s_effectiveDecelerationMMps2;
static float s_targetHeadingDeg;
static float s_direction;
static uint8_t s_decelerationActive;
static uint8_t s_targetReached;

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

static float MotionStraight_Approach(float current, float target, float maximumStep)
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

static uint8_t MotionStraight_ConfigIsValid(const MotionStraight_Config_t *config)
{
    if (config == NULL)
    {
        return 0U;
    }
    if ((!isfinite(config->maximumSpeedMMps)) ||
        (!isfinite(config->accelerationMMps2)) ||
        (!isfinite(config->decelerationMMps2)) ||
        (!isfinite(config->decelerationStartRatio)) ||
        (!isfinite(config->distanceToleranceMM)))
    {
        return 0U;
    }
    if ((config->maximumSpeedMMps <= 0.0f) ||
        (config->accelerationMMps2 <= 0.0f) ||
        (config->decelerationMMps2 <= 0.0f) ||
        (config->decelerationStartRatio <= 0.0f) ||
        (config->decelerationStartRatio >= 1.0f) ||
        (config->distanceToleranceMM < 0.0f))
    {
        return 0U;
    }
    if ((!isfinite(config->heading.kp)) ||
        (!isfinite(config->heading.kd)) ||
        (!isfinite(config->heading.correctionLimitPWM)) ||
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
    s_profileSpeedMMps = 0.0f;
}

static void MotionStraight_SetError(MotionStraight_Error_t error)
{
    MotionWheel_Stop();
    s_error = error;
    s_state = MOTION_STRAIGHT_STATE_ERROR;
}

MotionStraight_Result_t MotionStraight_Init(const MotionStraight_Config_t *config)
{
    MotionWheel_Result_t wheelResult;

    Motor_StopAll();
    s_configured = 0U;
    s_state = MOTION_STRAIGHT_STATE_IDLE;
    s_error = MOTION_STRAIGHT_ERROR_NONE;

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
    s_remainingDistanceMM = 0.0f;
    s_configured = 1U;
    return MOTION_STRAIGHT_RESULT_OK;
}

MotionStraight_Result_t MotionStraight_InitDefault(void)
{
    return MotionStraight_Init(&g_motionStraightConfig);
}

MotionStraight_Result_t MotionStraight_Start(
    float distanceMM, float speedMMps, float endSpeedMMps)
{
    float preferredDecelerationDistanceMM;
    float requiredDecelerationDistanceMM;
    float speedSquaredDifference;

    if (s_configured == 0U)
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
    s_error = MOTION_STRAIGHT_ERROR_NONE;
    s_startDistanceLMM = Odometry_GetDistanceLMM();
    s_startDistanceRMM = Odometry_GetDistanceRMM();
    s_targetDistanceMM = distanceMM;
    s_totalDistanceMM = fabsf(distanceMM);
    s_remainingDistanceMM = distanceMM;
    s_cruiseSpeedMMps = MotionStraight_Clamp(speedMMps,
                                     0.0f,
                                     s_config.maximumSpeedMMps);
    s_endSpeedMMps = MotionStraight_Clamp(endSpeedMMps,
                                  0.0f,
                                  s_cruiseSpeedMMps);
    s_targetHeadingDeg = Heading_GetYaw();
    s_direction = (distanceMM >= 0.0f) ? 1.0f : -1.0f;
    s_decelerationActive = 0U;
    s_targetReached = 0U;

    /*
     * 优先在全程 5/6 处开始降速。若配置的最大减速度不足以在末段
     * 达到终点速度，则按运动学所需距离自动提前，避免终点速度突变。
     */
    preferredDecelerationDistanceMM = s_totalDistanceMM *
        (1.0f - s_config.decelerationStartRatio);
    speedSquaredDifference = s_cruiseSpeedMMps * s_cruiseSpeedMMps -
                             s_endSpeedMMps * s_endSpeedMMps;
    requiredDecelerationDistanceMM = speedSquaredDifference /
        (2.0f * s_config.decelerationMMps2);
    if (requiredDecelerationDistanceMM > preferredDecelerationDistanceMM)
    {
        preferredDecelerationDistanceMM = requiredDecelerationDistanceMM;
    }
    preferredDecelerationDistanceMM = MotionStraight_Clamp(
        preferredDecelerationDistanceMM, 0.0f, s_totalDistanceMM);
    s_decelerationStartDistanceMM =
        s_totalDistanceMM - preferredDecelerationDistanceMM;
    s_effectiveDecelerationMMps2 = s_config.decelerationMMps2;

    if (fabsf(distanceMM) <= s_config.distanceToleranceMM)
    {
        s_remainingDistanceMM = 0.0f;
        if (s_endSpeedMMps <= 0.001f)
        {
            s_state = MOTION_STRAIGHT_STATE_COMPLETED;
        }
        else
        {
            /* 极短距离任务也通过加速度斜坡进入终点持续速度。 */
            s_decelerationActive = 1U;
            s_targetReached = 1U;
            s_state = MOTION_STRAIGHT_STATE_RUNNING;
        }
        return MOTION_STRAIGHT_RESULT_OK;
    }

    s_state = MOTION_STRAIGHT_STATE_RUNNING;
    return MOTION_STRAIGHT_RESULT_OK;
}

MotionStraight_Result_t MotionStraight_StartForward(
    uint32_t distanceMM, float speedMMps, float endSpeedMMps)
{
    return MotionStraight_Start((float)distanceMM, speedMMps, endSpeedMMps);
}

MotionStraight_Result_t MotionStraight_StartBackward(
    uint32_t distanceMM, float speedMMps, float endSpeedMMps)
{
    return MotionStraight_Start(-(float)distanceMM, speedMMps, endSpeedMMps);
}

void MotionStraight_Update(float dt)
{
    MotionWheel_Command_t wheelCommand;
    MotionWheel_Result_t wheelResult;
    float distanceLMM;
    float distanceRMM;
    float averageDistanceMM;
    float directedTravelledMM;
    float directedRemainingMM;
    float brakingSpeedMMps;
    float targetSpeedMagnitudeMMps;
    float targetSpeedMMps;
    float profileStepMMps;
    float headingCorrectionPWM;

    if ((s_state != MOTION_STRAIGHT_STATE_RUNNING) &&
        (s_state != MOTION_STRAIGHT_STATE_CONTINUING))
    {
        return;
    }
    if ((!isfinite(dt)) || (dt <= 0.0f))
    {
        MotionStraight_SetError(MOTION_STRAIGHT_ERROR_UPDATE_PERIOD_INVALID);
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

    distanceLMM = Odometry_GetDistanceLMM() - s_startDistanceLMM;
    distanceRMM = Odometry_GetDistanceRMM() - s_startDistanceRMM;
    averageDistanceMM = (distanceLMM + distanceRMM) * 0.5f;
    directedTravelledMM = s_direction * averageDistanceMM;

    if (s_state == MOTION_STRAIGHT_STATE_CONTINUING)
    {
        s_remainingDistanceMM = 0.0f;
        targetSpeedMagnitudeMMps = s_endSpeedMMps;
    }
    else
    {
        s_remainingDistanceMM = s_targetDistanceMM - averageDistanceMM;
        directedRemainingMM = s_direction * s_remainingDistanceMM;

        if ((s_decelerationActive == 0U) &&
            (directedTravelledMM >= s_decelerationStartDistanceMM))
        {
            float currentSpeedMagnitudeMMps = fabsf(s_profileSpeedMMps);

            s_decelerationActive = 1U;
            if ((currentSpeedMagnitudeMMps > s_endSpeedMMps) &&
                (directedRemainingMM > 0.001f))
            {
                s_effectiveDecelerationMMps2 =
                    (currentSpeedMagnitudeMMps * currentSpeedMagnitudeMMps -
                     s_endSpeedMMps * s_endSpeedMMps) /
                    (2.0f * directedRemainingMM);
                s_effectiveDecelerationMMps2 = MotionStraight_Clamp(
                    s_effectiveDecelerationMMps2,
                    0.001f, s_config.decelerationMMps2);
            }
        }

        if (directedRemainingMM <= s_config.distanceToleranceMM)
        {
            /* 到点后只继续完成速度斜坡，不再调用满占空比主动制动。 */
            s_targetReached = 1U;
            targetSpeedMagnitudeMMps = s_endSpeedMMps;
        }
        else if (s_decelerationActive != 0U)
        {
            brakingSpeedMMps = sqrtf(
                s_endSpeedMMps * s_endSpeedMMps +
                2.0f * s_effectiveDecelerationMMps2 *
                directedRemainingMM);
            targetSpeedMagnitudeMMps = MotionStraight_Clamp(
                brakingSpeedMMps, s_endSpeedMMps, s_cruiseSpeedMMps);
        }
        else
        {
            targetSpeedMagnitudeMMps = s_cruiseSpeedMMps;
        }
    }
    targetSpeedMMps = s_direction * targetSpeedMagnitudeMMps;

    if (fabsf(targetSpeedMMps) > fabsf(s_profileSpeedMMps))
    {
        profileStepMMps = s_config.accelerationMMps2 * dt;
    }
    else
    {
        profileStepMMps = s_config.decelerationMMps2 * dt;
    }
    s_profileSpeedMMps = MotionStraight_Approach(s_profileSpeedMMps,
                                         targetSpeedMMps,
                                         profileStepMMps);

    if ((s_state == MOTION_STRAIGHT_STATE_RUNNING) &&
        (s_targetReached != 0U) &&
        (fabsf(s_profileSpeedMMps - targetSpeedMMps) <= 0.001f))
    {
        s_remainingDistanceMM = 0.0f;
        if (s_endSpeedMMps <= 0.001f)
        {
            /* PWM 归零并释放方向脚，依靠滑行停车，避免最后猛刹。 */
            MotionWheel_Stop();
            s_state = MOTION_STRAIGHT_STATE_COMPLETED;
            return;
        }
        s_state = MOTION_STRAIGHT_STATE_CONTINUING;
    }

    /* 航向层：直接使用连续累计角度，不对多圈角度做 ±180° 归一化。 */
    headingCorrectionPWM = PID_Update(
        &s_headingPID, s_targetHeadingDeg, Heading_GetYaw(), dt);
    headingCorrectionPWM *= (float)s_config.heading.correctionSign;

    /* 公共轮速层负责双轮速度 PI、前馈和最终 PWM 限幅。 */
    wheelCommand.targetSpeedLMMps = s_profileSpeedMMps;
    wheelCommand.targetSpeedRMMps = s_profileSpeedMMps;
    wheelCommand.trimLPWM = -headingCorrectionPWM;
    wheelCommand.trimRPWM = headingCorrectionPWM;
    wheelResult = MotionWheel_Update(&wheelCommand, dt);
    if (wheelResult != MOTION_WHEEL_RESULT_OK)
    {
        MotionStraight_SetError(MOTION_STRAIGHT_ERROR_WHEEL);
    }
}

void MotionStraight_Stop(void)
{
    MotionWheel_Stop();
    MotionStraight_ResetControllers();
    s_remainingDistanceMM = 0.0f;
    s_error = MOTION_STRAIGHT_ERROR_NONE;
    s_state = MOTION_STRAIGHT_STATE_IDLE;
}

uint8_t MotionStraight_IsConfigured(void)
{
    return s_configured;
}

uint8_t MotionStraight_IsBusy(void)
{
    return ((s_state == MOTION_STRAIGHT_STATE_RUNNING) ||
            (s_state == MOTION_STRAIGHT_STATE_CONTINUING)) ? 1U : 0U;
}

uint8_t MotionStraight_IsFinished(void)
{
    return ((s_state == MOTION_STRAIGHT_STATE_COMPLETED) ||
            (s_state == MOTION_STRAIGHT_STATE_CONTINUING)) ? 1U : 0U;
}

MotionStraight_State_t MotionStraight_GetState(void)
{
    return s_state;
}

MotionStraight_Error_t MotionStraight_GetError(void)
{
    return s_error;
}

float MotionStraight_GetRemainingDistanceMM(void)
{
    return s_remainingDistanceMM;
}
