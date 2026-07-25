#include "Application/Control/MotionManager.h"
#include "Application/Control/MotionLine.h"
#include "Application/Control/MotionStraight.h"
#include "Application/Control/MotionWheel.h"
#include "Application/Control/Nav.h"
#include "Hardware/Motor/Motor.h"
#include <math.h>

typedef struct
{
    MotionManager_Mode_t mode;
    MotionManager_Error_t error;
    uint8_t configured;
    float brakeElapsedSeconds;
    uint8_t brakeFinished;
    float speedTargetMMps;
    float driveLeftMMps;
    float driveRightMMps;
} MotionManager_Context_t;

static MotionManager_Context_t s_context;

/* 刹车参数固定在头文件，初始化时统一检查，防止运行期间出现无效时长。 */
static uint8_t MotionManager_BrakeParametersAreValid(void)
{
    if ((!isfinite(MOTION_MANAGER_BRAKE_RELEASE_SECONDS)) ||
        (!isfinite(MOTION_MANAGER_BRAKE_HOLD_SECONDS)))
    {
        return 0U;
    }

    return ((MOTION_MANAGER_BRAKE_RELEASE_SECONDS >= 0.0f) &&
            (MOTION_MANAGER_BRAKE_HOLD_SECONDS > 0.0f)) ? 1U : 0U;
}

/* 完整刹车过程只占用 MotionManager，不进入 MotionWheel 速度闭环。 */
static void MotionManager_UpdateBrake(float dt)
{
    float brakeStartSeconds;
    float brakeEndSeconds;

    if ((!isfinite(dt)) || (dt <= 0.0f))
    {
        Motor_StopAll();
        s_context.error = MOTION_MANAGER_ERROR_BRAKE;
        return;
    }

    s_context.brakeElapsedSeconds += dt;
    brakeStartSeconds = MOTION_MANAGER_BRAKE_RELEASE_SECONDS;
    brakeEndSeconds = brakeStartSeconds + MOTION_MANAGER_BRAKE_HOLD_SECONDS;

    if (s_context.brakeElapsedSeconds < brakeStartSeconds)
    {
        /* 直线速度已平滑降至零；这里继续释放 PWM，避免突变制动。 */
        Motor_StopAll();
        return;
    }

    if (s_context.brakeElapsedSeconds < brakeEndSeconds)
    {
        Motor_Brake();
        return;
    }

    /* 制动脉冲结束必须释放方向脚，防止电机持续发热。 */
    Motor_StopAll();
    s_context.brakeFinished = 1U;
}

static MotionManager_Result_t MotionManager_MapStraightResult(
    MotionStraight_Result_t result)
{
    if (result == MOTION_STRAIGHT_RESULT_OK)
    {
        return MOTION_MANAGER_RESULT_OK;
    }
    if (result == MOTION_STRAIGHT_RESULT_INVALID_ARGUMENT)
    {
        return MOTION_MANAGER_RESULT_INVALID_ARGUMENT;
    }
    if (result == MOTION_STRAIGHT_RESULT_NOT_CONFIGURED)
    {
        return MOTION_MANAGER_RESULT_NOT_CONFIGURED;
    }
    if (result == MOTION_STRAIGHT_RESULT_SENSOR_NOT_READY)
    {
        return MOTION_MANAGER_RESULT_SENSOR_NOT_READY;
    }
    return MOTION_MANAGER_RESULT_START_FAILED;
}

static MotionManager_Result_t MotionManager_MapLineResult(
    MotionLine_Result_t result)
{
    if (result == MOTION_LINE_RESULT_OK)
    {
        return MOTION_MANAGER_RESULT_OK;
    }
    if (result == MOTION_LINE_RESULT_INVALID_ARGUMENT)
    {
        return MOTION_MANAGER_RESULT_INVALID_ARGUMENT;
    }
    if (result == MOTION_LINE_RESULT_NOT_CONFIGURED)
    {
        return MOTION_MANAGER_RESULT_NOT_CONFIGURED;
    }
    return MOTION_MANAGER_RESULT_START_FAILED;
}

static MotionManager_Result_t MotionManager_MapNavResult(Nav_Result_t result)
{
    if (result == NAV_RESULT_OK)
    {
        return MOTION_MANAGER_RESULT_OK;
    }
    if (result == NAV_RESULT_INVALID_ARGUMENT)
    {
        return MOTION_MANAGER_RESULT_INVALID_ARGUMENT;
    }
    if (result == NAV_RESULT_NOT_CONFIGURED)
    {
        return MOTION_MANAGER_RESULT_NOT_CONFIGURED;
    }
    if (result == NAV_RESULT_SENSOR_NOT_READY)
    {
        return MOTION_MANAGER_RESULT_SENSOR_NOT_READY;
    }
    return MOTION_MANAGER_RESULT_START_FAILED;
}

static MotionManager_Result_t MotionManager_FinishStart(
    MotionManager_Result_t result, MotionManager_Mode_t mode,
    MotionManager_Error_t error)
{
    if (result == MOTION_MANAGER_RESULT_OK)
    {
        s_context.mode = mode;
        s_context.error = MOTION_MANAGER_ERROR_NONE;
    }
    else
    {
        s_context.mode = MOTION_MANAGER_MODE_IDLE;
        s_context.error = error;
    }
    return result;
}

MotionManager_Result_t MotionManager_Init(void)
{
    s_context.configured = 0U;
    s_context.mode = MOTION_MANAGER_MODE_IDLE;
    s_context.error = MOTION_MANAGER_ERROR_NONE;
    s_context.brakeElapsedSeconds = 0.0f;
    s_context.brakeFinished = 0U;
    s_context.speedTargetMMps = 0.0f;

    if ((MotionManager_BrakeParametersAreValid() == 0U) ||
        (MotionStraight_Init() != MOTION_STRAIGHT_RESULT_OK) ||
        (MotionLine_Init() != MOTION_LINE_RESULT_OK) ||
        (Nav_Init() != NAV_RESULT_OK))
    {
        MotionWheel_Stop();
        s_context.error = MOTION_MANAGER_ERROR_INIT;
        return MOTION_MANAGER_RESULT_NOT_CONFIGURED;
    }

    MotionWheel_Stop();
    s_context.configured = 1U;
    return MOTION_MANAGER_RESULT_OK;
}

MotionManager_Result_t MotionManager_StartForward(
    uint32_t distanceMM, float speedMMps, float endSpeedMMps)
{
    MotionManager_Result_t result;

    if (s_context.configured == 0U)
    {
        return MOTION_MANAGER_RESULT_NOT_CONFIGURED;
    }

    MotionManager_Stop();
    result = MotionManager_MapStraightResult(MotionStraight_StartForward(
        distanceMM, speedMMps, endSpeedMMps));
    return MotionManager_FinishStart(
        result, MOTION_MANAGER_MODE_STRAIGHT,
        MOTION_MANAGER_ERROR_STRAIGHT);
}

MotionManager_Result_t MotionManager_StartBackward(
    uint32_t distanceMM, float speedMMps, float endSpeedMMps)
{
    MotionManager_Result_t result;

    if (s_context.configured == 0U)
    {
        return MOTION_MANAGER_RESULT_NOT_CONFIGURED;
    }

    MotionManager_Stop();
    result = MotionManager_MapStraightResult(MotionStraight_StartBackward(
        distanceMM, speedMMps, endSpeedMMps));
    return MotionManager_FinishStart(
        result, MOTION_MANAGER_MODE_STRAIGHT,
        MOTION_MANAGER_ERROR_STRAIGHT);
}

MotionManager_Result_t MotionManager_StartLine(float speedMMps)
{
    MotionManager_Result_t result;

    if (s_context.configured == 0U)
    {
        return MOTION_MANAGER_RESULT_NOT_CONFIGURED;
    }

    MotionManager_Stop();
    result = MotionManager_MapLineResult(MotionLine_Start(speedMMps));
    return MotionManager_FinishStart(
        result, MOTION_MANAGER_MODE_LINE, MOTION_MANAGER_ERROR_LINE);
}

MotionManager_Result_t MotionManager_TurnTo(
    float targetYawDeg, float speedMMps)
{
    MotionManager_Result_t result;

    if (s_context.configured == 0U)
    {
        return MOTION_MANAGER_RESULT_NOT_CONFIGURED;
    }

    MotionManager_Stop();
    result = MotionManager_MapNavResult(Nav_StartTo(targetYawDeg, speedMMps));
    return MotionManager_FinishStart(
        result, MOTION_MANAGER_MODE_TURN, MOTION_MANAGER_ERROR_TURN);
}

MotionManager_Result_t MotionManager_TurnBy(
    float deltaYawDeg, float speedMMps)
{
    MotionManager_Result_t result;

    if (s_context.configured == 0U)
    {
        return MOTION_MANAGER_RESULT_NOT_CONFIGURED;
    }

    MotionManager_Stop();
    result = MotionManager_MapNavResult(Nav_StartBy(deltaYawDeg, speedMMps));
    return MotionManager_FinishStart(
        result, MOTION_MANAGER_MODE_TURN, MOTION_MANAGER_ERROR_TURN);
}

MotionManager_Result_t MotionManager_StartSpeed(float speedMMps)
{
    if (s_context.configured == 0U)
    {
        return MOTION_MANAGER_RESULT_NOT_CONFIGURED;
    }
    if ((!isfinite(speedMMps)) ||
        (fabsf(speedMMps) > MOTION_MANAGER_SPEED_MAX_MMPS))
    {
        return MOTION_MANAGER_RESULT_INVALID_ARGUMENT;
    }

    /* 已在恒速模式时只改目标：保留双轮 PID 状态，支持链式阶跃。 */
    if (s_context.mode == MOTION_MANAGER_MODE_SPEED)
    {
        s_context.speedTargetMMps = speedMMps;
        return MOTION_MANAGER_RESULT_OK;
    }

    MotionManager_Stop();
    s_context.speedTargetMMps = speedMMps;
    s_context.mode = MOTION_MANAGER_MODE_SPEED;
    s_context.error = MOTION_MANAGER_ERROR_NONE;
    return MOTION_MANAGER_RESULT_OK;
}

MotionManager_Result_t MotionManager_SetDrive(
    float leftMMps, float rightMMps)
{
    if (s_context.configured == 0U)
    {
        return MOTION_MANAGER_RESULT_NOT_CONFIGURED;
    }
    if ((!isfinite(leftMMps)) || (!isfinite(rightMMps)) ||
        (fabsf(leftMMps) > MOTION_MANAGER_SPEED_MAX_MMPS) ||
        (fabsf(rightMMps) > MOTION_MANAGER_SPEED_MAX_MMPS))
    {
        return MOTION_MANAGER_RESULT_INVALID_ARGUMENT;
    }

    /* 已在差速驾驶模式时只改目标：保留双轮 PID 状态，支持高频连续下发。 */
    if (s_context.mode == MOTION_MANAGER_MODE_DRIVE)
    {
        s_context.driveLeftMMps = leftMMps;
        s_context.driveRightMMps = rightMMps;
        return MOTION_MANAGER_RESULT_OK;
    }

    MotionManager_Stop();
    s_context.driveLeftMMps = leftMMps;
    s_context.driveRightMMps = rightMMps;
    s_context.mode = MOTION_MANAGER_MODE_DRIVE;
    s_context.error = MOTION_MANAGER_ERROR_NONE;
    return MOTION_MANAGER_RESULT_OK;
}

MotionManager_Result_t MotionManager_StartBrake(void)
{
    if (s_context.configured == 0U)
    {
        return MOTION_MANAGER_RESULT_NOT_CONFIGURED;
    }
    if (MotionManager_BrakeParametersAreValid() == 0U)
    {
        s_context.error = MOTION_MANAGER_ERROR_BRAKE;
        return MOTION_MANAGER_RESULT_INVALID_ARGUMENT;
    }

    /* 此调用发生在目标状态 onEnter；只清理进入前的旧模式。 */
    MotionManager_Stop();
    s_context.brakeElapsedSeconds = 0.0f;
    s_context.brakeFinished = 0U;
    s_context.mode = MOTION_MANAGER_MODE_BRAKE;
    s_context.error = MOTION_MANAGER_ERROR_NONE;
    return MOTION_MANAGER_RESULT_OK;
}

void MotionManager_Update(float dt)
{
    if (s_context.configured == 0U)
    {
        return;
    }

    switch (s_context.mode)
    {
        case MOTION_MANAGER_MODE_STRAIGHT:
            MotionStraight_Update(dt);
            if (MotionStraight_GetState() == MOTION_STRAIGHT_STATE_ERROR)
            {
                s_context.error = MOTION_MANAGER_ERROR_STRAIGHT;
            }
            break;

        case MOTION_MANAGER_MODE_LINE:
            MotionLine_Update(dt);
            if (MotionLine_GetState() == MOTION_LINE_STATE_ERROR)
            {
                s_context.error = MOTION_MANAGER_ERROR_LINE;
            }
            break;

        case MOTION_MANAGER_MODE_TURN:
            Nav_Update(dt);
            if (Nav_GetState() == NAV_STATE_ERROR)
            {
                s_context.error = MOTION_MANAGER_ERROR_TURN;
            }
            break;

        case MOTION_MANAGER_MODE_BRAKE:
            MotionManager_UpdateBrake(dt);
            break;

        case MOTION_MANAGER_MODE_SPEED:
        {
            MotionWheel_Command_t command = {0};

            command.targetSpeedLMMps = s_context.speedTargetMMps;
            command.targetSpeedRMMps = s_context.speedTargetMMps;
            if (MotionWheel_Update(&command, dt) != MOTION_WHEEL_RESULT_OK)
            {
                MotionWheel_Stop();
                s_context.mode = MOTION_MANAGER_MODE_IDLE;
                s_context.error = MOTION_MANAGER_ERROR_SPEED;
            }
            break;
        }

        case MOTION_MANAGER_MODE_DRIVE:
        {
            MotionWheel_Command_t command = {0};

            command.targetSpeedLMMps = s_context.driveLeftMMps;
            command.targetSpeedRMMps = s_context.driveRightMMps;
            if (MotionWheel_Update(&command, dt) != MOTION_WHEEL_RESULT_OK)
            {
                MotionWheel_Stop();
                s_context.mode = MOTION_MANAGER_MODE_IDLE;
                s_context.error = MOTION_MANAGER_ERROR_DRIVE;
            }
            break;
        }

        case MOTION_MANAGER_MODE_IDLE:
        default:
            break;
    }
}

void MotionManager_Stop(void)
{
    switch (s_context.mode)
    {
        case MOTION_MANAGER_MODE_STRAIGHT:
            MotionStraight_Stop();
            break;

        case MOTION_MANAGER_MODE_LINE:
            MotionLine_Stop();
            break;

        case MOTION_MANAGER_MODE_TURN:
            Nav_Stop();
            break;

        case MOTION_MANAGER_MODE_BRAKE:
            /* 无论刹车处于哪个阶段，停止时都只能释放电机。 */
            MotionWheel_Stop();
            break;

        case MOTION_MANAGER_MODE_SPEED:
        case MOTION_MANAGER_MODE_IDLE:
        default:
            MotionWheel_Stop();
            break;
    }

    s_context.mode = MOTION_MANAGER_MODE_IDLE;
    s_context.error = MOTION_MANAGER_ERROR_NONE;
    s_context.brakeElapsedSeconds = 0.0f;
    s_context.brakeFinished = 0U;
    s_context.speedTargetMMps = 0.0f;
    s_context.driveLeftMMps = 0.0f;
    s_context.driveRightMMps = 0.0f;
}

uint8_t MotionManager_IsConfigured(void)
{
    return s_context.configured;
}

uint8_t MotionManager_IsBusy(void)
{
    switch (s_context.mode)
    {
        case MOTION_MANAGER_MODE_STRAIGHT:
            return MotionStraight_IsBusy();
        case MOTION_MANAGER_MODE_LINE:
            return MotionLine_IsBusy();
        case MOTION_MANAGER_MODE_TURN:
            return Nav_IsBusy();
        case MOTION_MANAGER_MODE_BRAKE:
            return (s_context.brakeFinished == 0U) ? 1U : 0U;
        case MOTION_MANAGER_MODE_SPEED:
        case MOTION_MANAGER_MODE_DRIVE:
            /* 恒速与差速驾驶都没有完成条件，只能被显式停止。 */
            return 1U;
        case MOTION_MANAGER_MODE_IDLE:
        default:
            return 0U;
    }
}

uint8_t MotionManager_IsFinished(void)
{
    switch (s_context.mode)
    {
        case MOTION_MANAGER_MODE_STRAIGHT:
            return MotionStraight_IsFinished();
        case MOTION_MANAGER_MODE_TURN:
            return Nav_IsFinished();
        case MOTION_MANAGER_MODE_LINE:
            return MotionLine_IsFinished();
        case MOTION_MANAGER_MODE_BRAKE:
            return s_context.brakeFinished;
        case MOTION_MANAGER_MODE_SPEED:
        case MOTION_MANAGER_MODE_DRIVE:
        case MOTION_MANAGER_MODE_IDLE:
        default:
            return 0U;
    }
}

MotionManager_Mode_t MotionManager_GetMode(void)
{
    return s_context.mode;
}

MotionManager_Error_t MotionManager_GetError(void)
{
    return s_context.error;
}
