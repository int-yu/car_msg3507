#include "Application/Control/MotionStraight.h"
#include "Application/Control/MotionWheel.h"
#include "Application/Control/PID.h"
#include "Application/State/Heading.h"
#include "Application/State/Odometry.h"
#include "Hardware/Motor/Motor.h"
#include <math.h>

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
    float profileEndDistanceMM;

    float cruiseSpeedMMps;
    float endSpeedMMps;
    float profileSpeedMMps;
    float decelerationStartDistanceMM;
    float effectiveDecelerationMMps2;

    float targetHeadingDeg;
    float direction;
    float brakeElapsedSeconds;
    uint8_t decelerationActive;
    uint8_t targetReached;
} MotionStraight_Context_t;

/* 运行时可调参数，默认值取头文件 #define；范围校验由 Param 模块负责。 */
float MotionStraight_TuneHeadingKp = MOTION_STRAIGHT_HEADING_KP;
float MotionStraight_TuneHeadingKd = MOTION_STRAIGHT_HEADING_KD;
float MotionStraight_TuneAccelerationMMps2 =
    MOTION_STRAIGHT_ACCELERATION_MMPS2;

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
static uint8_t MotionStraight_ParametersAreValid(void)
{
    if ((!isfinite(MOTION_STRAIGHT_MAX_SPEED_MMPS)) ||
        (!isfinite(MOTION_STRAIGHT_ACCELERATION_MMPS2)) ||
        (!isfinite(MOTION_STRAIGHT_DECELERATION_START_RATIO)) ||
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
        (MOTION_STRAIGHT_DECELERATION_START_RATIO <= 0.0f) ||
        (MOTION_STRAIGHT_DECELERATION_START_RATIO >= 1.0f) ||
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
}

static void MotionStraight_SetError(MotionStraight_Error_t error)
{
    MotionWheel_Stop();
    s_context.error = error;
    s_context.state = MOTION_STRAIGHT_STATE_ERROR;
}

/* 根据全程比例确定减速起点；终点速度由减速段的距离曲线保证。 */
static void MotionStraight_PrepareProfile(
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

    /*
     * 容差是允许的终点误差，因此速度曲线在 total - tolerance 处达到
     * endSpeed。这样在允许误差内已经是目标速度，而不会到点后突然降速。
     */
    s_context.profileEndDistanceMM = s_context.totalDistanceMM -
        MOTION_STRAIGHT_DISTANCE_TOLERANCE_MM;
    if (s_context.profileEndDistanceMM < 0.0f)
    {
        s_context.profileEndDistanceMM = 0.0f;
    }

    s_context.decelerationStartDistanceMM =
        s_context.totalDistanceMM *
        MOTION_STRAIGHT_DECELERATION_START_RATIO;
    if (s_context.decelerationStartDistanceMM >
        s_context.profileEndDistanceMM)
    {
        /* 极短距离没有完整的后 1/6，直接在速度规划终点前开始减速。 */
        s_context.decelerationStartDistanceMM =
            s_context.profileEndDistanceMM;
    }

    s_context.effectiveDecelerationMMps2 = 0.0f;
    s_context.decelerationActive = 0U;
    s_context.targetReached = 0U;
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

/* 在固定减速点记录当前速度，并计算剩余距离内所需的恒定减速度。 */
static void MotionStraight_StartDecelerationIfNeeded(
    float travelledDistanceMM)
{
    float currentSpeedMMps;
    float profileRemainingDistanceMM;

    if ((s_context.decelerationActive != 0U) ||
        (travelledDistanceMM < s_context.decelerationStartDistanceMM))
    {
        return;
    }

    s_context.decelerationActive = 1U;
    currentSpeedMMps = fabsf(s_context.profileSpeedMMps);
    profileRemainingDistanceMM = s_context.profileEndDistanceMM -
        travelledDistanceMM;
    if ((currentSpeedMMps > s_context.endSpeedMMps) &&
        (profileRemainingDistanceMM > 0.001f))
    {
        /* v_end^2 = v_start^2 - 2 * a * s。a 在本次减速段内保持不变。 */
        s_context.effectiveDecelerationMMps2 =
            (currentSpeedMMps * currentSpeedMMps -
             s_context.endSpeedMMps * s_context.endSpeedMMps) /
            (2.0f * profileRemainingDistanceMM);
    }
}

/* 距离规划层：返回本周期应跟踪的正速度大小。 */
static float MotionStraight_CalculateTargetSpeedMagnitude(void)
{
    float travelledDistanceMM;
    float remainingDistanceMM;
    float profileRemainingDistanceMM;
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

    MotionStraight_StartDecelerationIfNeeded(travelledDistanceMM);

    profileRemainingDistanceMM = s_context.profileEndDistanceMM -
        travelledDistanceMM;
    if (profileRemainingDistanceMM <= 0.0f)
    {
        /* 到达距离容差范围时，速度已按规划下降至结束速度。 */
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
        profileRemainingDistanceMM);
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
        speedStepMMps = MotionStraight_TuneAccelerationMMps2 * dt;
    }
    else
    {
        speedStepMMps = s_context.effectiveDecelerationMMps2 * dt;
    }
    s_context.profileSpeedMMps = MotionStraight_Approach(
        s_context.profileSpeedMMps, targetSpeedMMps, speedStepMMps);
    return s_context.profileSpeedMMps;
}

/*
 * 到点后进入独立刹车段。相比"继续下发 0 速度闭环保持"，这里直接短接绕组：
 * 低速时编码器读数跳变会让 0 速闭环的 PID 来回纠正，正是"到点前后哆嗦"的
 * 根因；而 Kp 的制动量又低于静摩擦阈值，压不住滑行、车会冲过设定距离一小段
 *（"弹射/冲过头"）。前面的梯形减速已经把速度平滑降到≈0，此处再短接刹车
 * 吸收残余惯性，既不抖也不冲，停点稳落在距离容差内。
 */
static void MotionStraight_StartBraking(void)
{
    MotionWheel_Reset();
    Motor_Brake();
    s_context.remainingDistanceMM = 0.0f;
    s_context.brakeElapsedSeconds = 0.0f;
    s_context.state = MOTION_STRAIGHT_STATE_BRAKING;
}

/* 刹车段独占运动：只短接保持固定时长，不经速度规划与轮速闭环。 */
static void MotionStraight_UpdateBraking(float dt)
{
    s_context.brakeElapsedSeconds += dt;
    if (s_context.brakeElapsedSeconds < MOTION_STRAIGHT_BRAKE_HOLD_SECONDS)
    {
        Motor_Brake();
        return;
    }

    /* 刹车保持结束后释放 PWM，避免电机持续发热。 */
    Motor_StopAll();
    s_context.state = MOTION_STRAIGHT_STATE_COMPLETED;
}

/*
 * 到达距离容差且规划速度已归零时决定去向：
 *   endSpeed=0 → 进入刹车段止住惯性；endSpeed>0 → 转 CONTINUING 保持结束速度。
 * 返回 1 表示本动作已交给刹车段接管，调用方无需再跑速度规划。
 */
static uint8_t MotionStraight_TransitionAtTarget(float targetSpeedMMps)
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
        MotionStraight_StartBraking();
        return 1U;
    }

    s_context.state = MOTION_STRAIGHT_STATE_CONTINUING;
    return 0U;
}

static MotionWheel_Result_t MotionStraight_ApplyWheelCommand(float dt)
{
    MotionWheel_Command_t command;
    float headingCorrectionPWM;

    headingCorrectionPWM = 0.0f;
    if (fabsf(s_context.profileSpeedMMps) > 0.001f)
    {
        /* 连续累计角不做 ±180° 归一化，因此可直接完成多圈航向保持。 */
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

    /* 用运行时参数初始化：同一次上电内重复 Init 不会丢掉已调好的增益。 */
    PID_Init(&s_headingPID,
             MotionStraight_TuneHeadingKp, 0.0f,
             MotionStraight_TuneHeadingKd,
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
    MotionStraight_PrepareProfile(distanceMM, speedMMps, endSpeedMMps);

    if (fabsf(distanceMM) <= MOTION_STRAIGHT_DISTANCE_TOLERANCE_MM)
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
        (s_context.state != MOTION_STRAIGHT_STATE_CONTINUING) &&
        (s_context.state != MOTION_STRAIGHT_STATE_BRAKING))
    {
        return;
    }
    if ((!isfinite(dt)) || (dt <= 0.0f))
    {
        MotionStraight_SetError(
            MOTION_STRAIGHT_ERROR_UPDATE_PERIOD_INVALID);
        return;
    }

    /* 刹车段独立于速度规划与航向闭环，只推进短接刹车定时。 */
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

    targetSpeedMagnitudeMMps =
        MotionStraight_CalculateTargetSpeedMagnitude();
    targetSpeedMMps = s_context.direction * targetSpeedMagnitudeMMps;
    MotionStraight_UpdateProfileSpeed(targetSpeedMMps, dt);

    if (MotionStraight_ApplyWheelCommand(dt) != MOTION_WHEEL_RESULT_OK)
    {
        MotionStraight_SetError(MOTION_STRAIGHT_ERROR_WHEEL);
        return;
    }
    (void)MotionStraight_TransitionAtTarget(targetSpeedMMps);
}

void MotionStraight_ApplyHeadingTunings(void)
{
    if (s_context.configured == 0U)
    {
        return;
    }
    PID_SetTunings(&s_headingPID, MotionStraight_TuneHeadingKp,
                   0.0f, MotionStraight_TuneHeadingKd);
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
            (s_context.state == MOTION_STRAIGHT_STATE_CONTINUING) ||
            (s_context.state == MOTION_STRAIGHT_STATE_BRAKING)) ?
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
