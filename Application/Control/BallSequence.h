#ifndef APPLICATION_CONTROL_BALL_SEQUENCE_H
#define APPLICATION_CONTROL_BALL_SEQUENCE_H

#include <stdint.h>

/*
 * Generic ball-position hold task.
 *
 * The old O->+50mm->-50mm sequence has been removed. The public start
 * function now takes the desired ball hold position in millimeters from the
 * beam center O. The task keeps running closed-loop until the caller stops it
 * or the balance controller reports an error.
 */

#define BALL_SEQUENCE_DEFAULT_TARGET_MM 0.0f

typedef enum
{
    BALL_SEQUENCE_STATE_READY = 0,
    BALL_SEQUENCE_STATE_HOLDING,
    BALL_SEQUENCE_STATE_FINISHED,
    BALL_SEQUENCE_STATE_ERROR
} BallSequence_State_t;

typedef enum
{
    BALL_SEQUENCE_ERROR_NONE = 0,
    BALL_SEQUENCE_ERROR_VISION,
    BALL_SEQUENCE_ERROR_BALANCE
} BallSequence_Error_t;

void BallSequence_Init(void);

/* Start holding targetMM. Returns 0 if vision or target validation fails. */
uint8_t BallSequence_Start(float targetMM);

/* Update the target of an active hold without restarting the task. */
uint8_t BallSequence_SetTarget(float targetMM);

void BallSequence_Update(float dt);
void BallSequence_Stop(void);

BallSequence_State_t BallSequence_GetState(void);
BallSequence_Error_t BallSequence_GetError(void);
uint32_t BallSequence_GetElapsedTicks(void);
float BallSequence_GetTargetMM(void);
uint8_t BallSequence_IsActive(void);

#endif
