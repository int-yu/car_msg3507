#include "Application/Control/BallSequence.h"
#include "Application/Control/BallBalance.h"
#include "Application/Control/TaskTimer.h"

typedef struct
{
    BallSequence_State_t state;
    BallSequence_Error_t error;
    uint32_t elapsedTicks;
    float targetMM;
} BallSequence_Context_t;

static BallSequence_Context_t s_context;

static void BallSequence_Fail(BallSequence_Error_t error)
{
    BallBalance_Stop();
    TaskTimer_Stop(TASK_TIMER_OWNER_BALL);
    s_context.error = error;
    s_context.state = BALL_SEQUENCE_STATE_ERROR;
}

void BallSequence_Init(void)
{
    s_context.state = BALL_SEQUENCE_STATE_READY;
    s_context.error = BALL_SEQUENCE_ERROR_NONE;
    s_context.elapsedTicks = 0U;
    s_context.targetMM = BALL_SEQUENCE_DEFAULT_TARGET_MM;
    BallBalance_Init();
}

uint8_t BallSequence_Start(float targetMM)
{
    if (BallBalance_Start(targetMM) != BALL_BALANCE_RESULT_OK)
    {
        s_context.error = BALL_SEQUENCE_ERROR_VISION;
        s_context.state = BALL_SEQUENCE_STATE_ERROR;
        return 0U;
    }

    s_context.elapsedTicks = 0U;
    s_context.targetMM = targetMM;
    s_context.error = BALL_SEQUENCE_ERROR_NONE;
    s_context.state = BALL_SEQUENCE_STATE_HOLDING;
    return 1U;
}

uint8_t BallSequence_StartSweep(void)
{
    if (BallBalance_Start(BALL_SEQUENCE_NEGATIVE_TARGET_MM) !=
        BALL_BALANCE_RESULT_OK)
    {
        s_context.error = BALL_SEQUENCE_ERROR_VISION;
        s_context.state = BALL_SEQUENCE_STATE_ERROR;
        return 0U;
    }

    s_context.elapsedTicks = 0U;
    s_context.targetMM = BALL_SEQUENCE_NEGATIVE_TARGET_MM;
    s_context.error = BALL_SEQUENCE_ERROR_NONE;
    s_context.state = BALL_SEQUENCE_STATE_SWEEP_TO_NEGATIVE;
    return 1U;
}

uint8_t BallSequence_SetTarget(float targetMM)
{
    BallSequence_State_t previousState = s_context.state;

    if ((BallSequence_IsActive() == 0U) ||
        (BallBalance_SetTarget(targetMM) != BALL_BALANCE_RESULT_OK))
    {
        return 0U;
    }

    s_context.targetMM = targetMM;
    s_context.state = BALL_SEQUENCE_STATE_HOLDING;
    if (previousState != BALL_SEQUENCE_STATE_HOLDING)
    {
        TaskTimer_Stop(TASK_TIMER_OWNER_BALL);
    }
    return 1U;
}

void BallSequence_Update(float dt)
{
    if (BallSequence_IsActive() == 0U)
    {
        return;
    }

    BallBalance_Update(dt);

    if (BallBalance_GetState() == BALL_BALANCE_STATE_ERROR)
    {
        BallSequence_Fail(
            (BallBalance_GetError() == BALL_BALANCE_ERROR_VISION_LOST) ?
                BALL_SEQUENCE_ERROR_VISION :
                BALL_SEQUENCE_ERROR_BALANCE);
        return;
    }

    if ((s_context.state == BALL_SEQUENCE_STATE_SWEEP_TO_NEGATIVE) &&
        (BallBalance_GetPositionMM() <=
         BALL_SEQUENCE_REVERSAL_POSITION_MM))
    {
        if (BallBalance_SetTarget(BALL_SEQUENCE_POSITIVE_TARGET_MM) !=
            BALL_BALANCE_RESULT_OK)
        {
            BallSequence_Fail(BALL_SEQUENCE_ERROR_BALANCE);
            return;
        }
        s_context.targetMM = BALL_SEQUENCE_POSITIVE_TARGET_MM;
        s_context.state = BALL_SEQUENCE_STATE_SWEEP_TO_POSITIVE;
    }
    else if ((s_context.state == BALL_SEQUENCE_STATE_SWEEP_TO_POSITIVE) &&
             (BallBalance_IsStable() != 0U))
    {
        s_context.state = BALL_SEQUENCE_STATE_SWEEP_HOLDING_POSITIVE;
        TaskTimer_Stop(TASK_TIMER_OWNER_BALL);
    }

    if (s_context.elapsedTicks < UINT32_MAX)
    {
        s_context.elapsedTicks++;
    }
}

void BallSequence_Stop(void)
{
    BallBalance_Stop();
    TaskTimer_Stop(TASK_TIMER_OWNER_BALL);
    s_context.state = BALL_SEQUENCE_STATE_FINISHED;
}

BallSequence_State_t BallSequence_GetState(void)
{
    return s_context.state;
}

BallSequence_Error_t BallSequence_GetError(void)
{
    return s_context.error;
}

uint32_t BallSequence_GetElapsedTicks(void)
{
    return s_context.elapsedTicks;
}

float BallSequence_GetTargetMM(void)
{
    return s_context.targetMM;
}

uint8_t BallSequence_IsActive(void)
{
    return ((s_context.state == BALL_SEQUENCE_STATE_HOLDING) ||
            (s_context.state == BALL_SEQUENCE_STATE_SWEEP_TO_NEGATIVE) ||
            (s_context.state == BALL_SEQUENCE_STATE_SWEEP_TO_POSITIVE) ||
            (s_context.state ==
             BALL_SEQUENCE_STATE_SWEEP_HOLDING_POSITIVE)) ? 1U : 0U;
}

uint8_t BallSequence_IsStable(void)
{
    return ((s_context.state == BALL_SEQUENCE_STATE_HOLDING) &&
            (BallBalance_IsStable() != 0U)) ? 1U : 0U;
}
