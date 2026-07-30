#include "Application/Control/BallSequence.h"
#include "Application/Control/BallBalance.h"

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

void BallSequence_Update(float dt)
{
    if (s_context.state != BALL_SEQUENCE_STATE_HOLDING)
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

    if (s_context.elapsedTicks < UINT32_MAX)
    {
        s_context.elapsedTicks++;
    }
}

void BallSequence_Stop(void)
{
    BallBalance_Stop();
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
    return (s_context.state == BALL_SEQUENCE_STATE_HOLDING) ? 1U : 0U;
}
