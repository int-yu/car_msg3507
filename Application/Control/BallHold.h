#ifndef APPLICATION_CONTROL_BALL_HOLD_H
#define APPLICATION_CONTROL_BALL_HOLD_H

#include <stdint.h>

/*
 * Compatibility wrapper for holding the ball at O.
 *
 * BallSequence now accepts an arbitrary hold target. This module is kept for
 * callers that still mean "hold O" explicitly. It uses the same PID-only
 * BallBalance controller and does not apply chassis acceleration feedforward.
 */

#define BALL_HOLD_TARGET_MM 0.0f
#define BALL_HOLD_CONVERGE_TIMEOUT_S 8.0f

typedef enum
{
    BALL_HOLD_STATE_READY = 0,
    BALL_HOLD_STATE_CONVERGING,
    BALL_HOLD_STATE_HOLDING,
    BALL_HOLD_STATE_FINISHED,
    BALL_HOLD_STATE_ERROR
} BallHold_State_t;

typedef enum
{
    BALL_HOLD_ERROR_NONE = 0,
    BALL_HOLD_ERROR_VISION,
    BALL_HOLD_ERROR_BALANCE,
    BALL_HOLD_ERROR_CONVERGE_TIMEOUT
} BallHold_Error_t;

void BallHold_Init(void);
uint8_t BallHold_Start(void);
void BallHold_Update(float dt);
void BallHold_Stop(void);
uint8_t BallHold_IsActive(void);

BallHold_State_t BallHold_GetState(void);
BallHold_Error_t BallHold_GetError(void);
uint32_t BallHold_GetElapsedTicks(void);
uint32_t BallHold_GetConvergeTicks(void);

/* Compatibility telemetry. PID-only ball control always reports 0 here. */
float BallHold_GetFeedforwardMMps2(void);

#endif
