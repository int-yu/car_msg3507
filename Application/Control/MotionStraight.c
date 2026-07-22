#include "Application/Control/MotionStraight.h"
#include "Application/Control/MotionWheel.h"
#include "Application/Control/PID.h"
#include "Application/State/Heading.h"
#include "Application/State/Odometry.h"
#include "Hardware/Motor/Motor.h"
#include <math.h>

/* 直线任务只负责定距、航向保持和到点短接刹车。 */
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

    float targetHeadingDeg;
    float direction;
    float brakeElapsedSeconds;
} MotionStraight_Context_t;

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

/* 检查公共参数，防止无效值进入运动控制。 */
static uint8_t MotionStraight_ParametersAreValid(void)
{
    if ((!isfinite(MOTION_STRAIGHT_MAX_SPEED_MMPS)) ||
        (!isfinite(MOTION_STRAIGHT_ACCELERATION_MMPS2)) ||
        (!isfinite(MOTION_STRAIGHT_DISTANCE_TOLERANCE_MM)) ||
        (!isfinite(MOTION_STRAIGHT_BRAKE_HOLD_SECONDS)) ||
        (!isfinite(MOTION_STRAIGHT_HEADING_KP)) ||
        (!isfinite(MOTION_STRAIGHT_HEADING_KD)) ||
        (!isfinite(MOTION_STRAIGHT_HEADING_LIMIT_PWM)))
    {
        return 0U;
    }
    if ((MOTION_STRAIGHT_MAX_SPEED_MMPS <= 0.0f) ||
        (MOTION_STRAIGHT_ACCELERATION_MMPS2 <= 0.0f) ||
        (MOTION_STRAIGHT_DISTANCE_TOLERANCE_MM < 0.0f) ||
        (MOTION_STRAIGHT_BRAKE_HOLD_SECONDS <= 0.0f) ||
        (MOTION_STRAIGHT_HEADING_KP < 0.0f) ||
        (MOTION_STRAIGHT_HEADING_KD < 0.0f) ||
        ((MOTION_STRAIGHT_HEADING_KP == 0.0f) &&
         (MOTION_STRAIGHT_HEADING_KD == 0.0f)) ||
        (MOTION_STRAIGHT_HEADING_LIMIT_PWM <= 0.0f) ||
        (MOTION_STRAIGHT_HEADING_LIMIT_PWM >
         MotionWheel_GetMaximumCommandPWM()) ||
        ((MOTION_STRAIGHT_CORRECTION_SIGN != 1) &&
         (MOTION_STRAIGHT_CORRECTION_SIGN != -1)))
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
    s_context.brakeElapsedSeconds = 0.0f;
}

static void MotionStraight_SetError(MotionStraight_Error_t error)
{
    MotionWheel_Stop();
    s_context.error = error;
    s_context.state = MOTION_STRAIGHT_STATE_ERROR;
}

static void MotionStraight_PrepareTask(
    float distanceMM, float speedMMps, float endSpeedMMps)
{
    s_context.startDistanceLMM = Odometry_GetDistanceLMM();
    s_context.startDistanceRMM = Odometry_GetDistanceRMM();
    s_context.totalDistanceMM = fabsf(distanceMM);
    s_context.remainingDistanceMM = distanceMM;
    s_context.direction = (distanceMM >= 0.0f) ? 1.0f : -1.0f;

    s_context.cruiseSpeedMMps = MotionStraight_Clamp(
        speedMMps, 0.0f, MOTION_STRAIGHT_MAX_SPEED_MMPS);
    s_context.endSpeedMMps = MotionStraight_Clamp(
        endSpeedMMps, 0.0f, s_context.cruiseSpeedMMps);
    s_context.brakeElapsedSeconds = 0.0f;
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

static uint8_t MotionStraight_HasReachedTarget(void)
{
    float travelledDistanceMM = MotionStraight_GetDirectedTravelledDistance();
    float remainingDistanceMM = s_context.totalDistanceMM -
                                travelledDistanceMM;

    s_context.remainingDistanceMM =
        s_context.direction * remainingDistanceMM;

    return (remainingDistanceMM <=
            MOTION_STRAIGHT_DISTANCE_TOLERANCE_MM) ? 1U : 0U;
}

static void MotionStraight_StartBraking(void)
{
    /*
     * 到点后直接短接刹车。
     * 这里不再下发 0 速度闭环，避免编码器低速跳变造成前后抖动。
     */
    MotionWheel_Reset();
    Motor_Brake();
    s_context.remainingDistanceMM = 0.0f;
    s_context.brakeElapsedSeconds = 0.0f;
    s_context.state = MOTION_STRAIGHT_STATE_BRAKING;
}

static void MotionStraight_UpdateBraking(float dt)
{
    s_context.brakeElapsedSeconds += dt;
    if (s_context.brakeElapsedSeconds < MOTION_STRAIGHT_BRAKE_HOLD_SECONDS)
    {
        Motor_Brake();
        return;
    }

    Motor_StopAll();
    s_context.state = MOTION_STRAIGHT_STATE_COMPLETED;
}

static MotionWheel_Result_t MotionStraight_ApplyWheelCommand(float dt)
{
    MotionWheel_Command_t command;
    float headingCorrectionPWM = 0.0f;

    if (fabsf(s_context.profileSpeedMMps) > 0.001f)
    {
        /* MPU6050 使用连续累计角，不做 ±180° 限幅，可直接保持多圈航向。 */
        headingCorrectionPWM = PID_Update(
            &s_headingPID,
            s_context.targetHeadingDeg,
            Heading_GetYaw(), dt);
        headingCorrectionPWM *= (float)MOTION_STRAIGHT_CORRECTION_SIGN;
    }

    command.targetSpeedLMMps = s_context.profileSpeedMMps;
    command.targetSpeedRMMps = s_context.profileSpeedMMps;
    command.trimLPWM = -headingCorrectionPWM;
    command.trimRPWM = headingCorrectionPWM;
    return MotionWheel_Update(&command, dt);
}

MotionStraight_Result_t MotionStraight_Init(void)
{
    MotionWheel_Result_t wheelResult;

    Motor_StopAll();
    s_context.configured = 0U;
    s_context.state = MOTION_STRAIGHT_STATE_IDLE;
    s_context.error = MOTION_STRAIGHT_ERROR_NONE;

    wheelResult = MotionWheel_Init();
    if ((wheelResult != MOTION_WHEEL_RESULT_OK) ||
        (MotionStraight_ParametersAreValid() == 0U))
    {
        return MOTION_STRAIGHT_RESULT_INVALID_ARGUMENT;
    }

    PID_Init(&s_headingPID,
             MOTION_STRAIGHT_HEADING_KP, 0.0f,
             MOTION_STRAIGHT_HEADING_KD,
             MOTION_STRAIGHT_HEADING_LIMIT_PWM, 0.0f);
    MotionStraight_ResetControllers();
    s_context.remainingDistanceMM = 0.0f;
    s_context.configured = 1U;
    return MOTION_STRAIGHT_RESULT_OK;
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
    MotionStraight_PrepareTask(distanceMM, speedMMps, endSpeedMMps);

    if (fabsf(distanceMM) <= MOTION_STRAIGHT_DISTANCE_TOLERANCE_MM)
    {
        s_context.remainingDistanceMM = 0.0f;
        s_context.state = (s_context.endSpeedMMps <= 0.001f) ?
            MOTION_STRAIGHT_STATE_COMPLETED :
            MOTION_STRAIGHT_STATE_CONTINUING;
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
    float targetSpeedMMps;
    float maximumStepMMps;

    if ((s_context.state != MOTION_STRAIGHT_STATE_RUNNING) &&
        (s_context.state != MOTION_STRAIGHT_STATE_BRAKING) &&
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
    if (s_context.state == MOTION_STRAIGHT_STATE_BRAKING)
    {
        MotionStraight_UpdateBraking(dt);
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

    if (s_context.state == MOTION_STRAIGHT_STATE_RUNNING)
    {
        if (MotionStraight_HasReachedTarget() != 0U)
        {
            if (s_context.endSpeedMMps <= 0.001f)
            {
                MotionStraight_StartBraking();
                return;
            }
            s_context.remainingDistanceMM = 0.0f;
            s_context.state = MOTION_STRAIGHT_STATE_CONTINUING;
        }
    }

    targetSpeedMMps = s_context.direction *
        ((s_context.state == MOTION_STRAIGHT_STATE_CONTINUING) ?
            s_context.endSpeedMMps :
            s_context.cruiseSpeedMMps);
    maximumStepMMps = MOTION_STRAIGHT_ACCELERATION_MMPS2 * dt;
    s_context.profileSpeedMMps = MotionStraight_Approach(
        s_context.profileSpeedMMps, targetSpeedMMps, maximumStepMMps);

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
            (s_context.state == MOTION_STRAIGHT_STATE_BRAKING) ||
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
