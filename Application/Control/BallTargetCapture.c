#include "Application/Control/BallTargetCapture.h"
#include "Application/Control/BallBalance.h"
#include "Application/Control/BallSensor.h"

#include <math.h>

typedef struct
{
    BallTargetCapture_State_t state;
    uint8_t confirmedFrames;
    uint8_t hasSequence;
    uint8_t lastSequence;
    float averageMM;
    float targetMM;
} BallTargetCapture_Context_t;

static BallTargetCapture_Context_t s_context;

static void BallTargetCapture_ResetSamples(void)
{
    s_context.confirmedFrames = 0U;
    s_context.hasSequence = 0U;
    s_context.lastSequence = 0U;
    s_context.averageMM = 0.0f;
}

void BallTargetCapture_Init(void)
{
    s_context.state = BALL_TARGET_CAPTURE_STATE_IDLE;
    s_context.targetMM = 0.0f;
    BallTargetCapture_ResetSamples();
}

void BallTargetCapture_Start(void)
{
    BallTargetCapture_ResetSamples();
    s_context.targetMM = 0.0f;
    s_context.state = BALL_TARGET_CAPTURE_STATE_CAPTURING;
}

void BallTargetCapture_Update(void)
{
    uint8_t sequence;
    float positionMM;

    if (s_context.state != BALL_TARGET_CAPTURE_STATE_CAPTURING)
    {
        return;
    }
    if (BallSensor_IsFresh() == 0U)
    {
        BallTargetCapture_ResetSamples();
        return;
    }

    sequence = BallSensor_GetFrameSequence();
    if ((s_context.hasSequence != 0U) &&
        (sequence == s_context.lastSequence))
    {
        return;
    }
    s_context.hasSequence = 1U;
    s_context.lastSequence = sequence;

    positionMM = BallSensor_GetPositionMM();
    if ((!isfinite(positionMM)) ||
        (fabsf(positionMM) > BALL_BALANCE_TARGET_LIMIT_MM))
    {
        s_context.confirmedFrames = 0U;
        s_context.averageMM = 0.0f;
        return;
    }

    if ((s_context.confirmedFrames == 0U) ||
        (fabsf(positionMM - s_context.averageMM) >
         BALL_TARGET_CAPTURE_STABILITY_TOLERANCE_MM))
    {
        s_context.confirmedFrames = 1U;
        s_context.averageMM = positionMM;
        return;
    }

    s_context.averageMM +=
        (positionMM - s_context.averageMM) /
        (float)(s_context.confirmedFrames + 1U);
    s_context.confirmedFrames++;
    if (s_context.confirmedFrames >= BALL_TARGET_CAPTURE_CONFIRM_FRAMES)
    {
        s_context.targetMM = s_context.averageMM;
        s_context.state = BALL_TARGET_CAPTURE_STATE_CAPTURED;
    }
}

void BallTargetCapture_Cancel(void)
{
    s_context.state = BALL_TARGET_CAPTURE_STATE_IDLE;
    BallTargetCapture_ResetSamples();
}

BallTargetCapture_State_t BallTargetCapture_GetState(void)
{
    return s_context.state;
}

uint8_t BallTargetCapture_IsCapturing(void)
{
    return (s_context.state == BALL_TARGET_CAPTURE_STATE_CAPTURING) ? 1U : 0U;
}

uint8_t BallTargetCapture_IsCaptured(void)
{
    return (s_context.state == BALL_TARGET_CAPTURE_STATE_CAPTURED) ? 1U : 0U;
}

uint8_t BallTargetCapture_GetConfirmedFrames(void)
{
    return s_context.confirmedFrames;
}

float BallTargetCapture_GetTargetMM(void)
{
    return s_context.targetMM;
}
