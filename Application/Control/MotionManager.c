#include "Application/Control/MotionManager.h"
#include "Application/Control/MotionLine.h"
#include "Application/Control/MotionStraight.h"
#include "Application/Control/MotionWheel.h"
#include "Application/Control/Nav.h"

typedef struct
{
    MotionManager_Mode_t mode;
    MotionManager_Error_t error;
    uint8_t configured;
} MotionManager_Context_t;

static MotionManager_Context_t s_context;

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

    if ((MotionStraight_Init() != MOTION_STRAIGHT_RESULT_OK) ||
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

        case MOTION_MANAGER_MODE_IDLE:
        default:
            MotionWheel_Stop();
            break;
    }

    s_context.mode = MOTION_MANAGER_MODE_IDLE;
    s_context.error = MOTION_MANAGER_ERROR_NONE;
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
