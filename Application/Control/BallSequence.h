#ifndef APPLICATION_CONTROL_BALL_SEQUENCE_H
#define APPLICATION_CONTROL_BALL_SEQUENCE_H

#include <stdint.h>

/*
 * Ball-position task with generic hold and the KEY2 -50 mm -> +50 mm sweep.
 * Both modes keep running closed-loop until stopped or an error is reported.
 */

#define BALL_SEQUENCE_DEFAULT_TARGET_MM 0.0f
#define BALL_SEQUENCE_NEGATIVE_TARGET_MM (-50.0f)
#define BALL_SEQUENCE_POSITIVE_TARGET_MM 50.0f
#define BALL_SEQUENCE_REVERSAL_POSITION_MM (-41.0f)

#define BALL_SEQUENCE_POSITION_KP_PER_S          1.5f
#define BALL_SEQUENCE_VELOCITY_KP_DEG_PER_MMPS   0.35f
#define BALL_SEQUENCE_VELOCITY_KI_DEG_PER_MM     0.20f

/* KEY2 phase 2 (-50 mm -> +50 mm) starts with the same conservative values
 * as phase 1, but has an independent runtime tuning set. */
#define BALL_SEQUENCE_POSITIVE_POSITION_KP_PER_S \
    BALL_SEQUENCE_POSITION_KP_PER_S
#define BALL_SEQUENCE_POSITIVE_VELOCITY_KP_DEG_PER_MMPS \
    BALL_SEQUENCE_VELOCITY_KP_DEG_PER_MMPS
#define BALL_SEQUENCE_POSITIVE_VELOCITY_KI_DEG_PER_MM \
    BALL_SEQUENCE_VELOCITY_KI_DEG_PER_MM

extern float BallSequence_TunePositionKpPerS;
extern float BallSequence_TuneVelocityKpDegPerMMps;
extern float BallSequence_TuneVelocityKiDegPerMM;
extern float BallSequence_TunePositivePositionKpPerS;
extern float BallSequence_TunePositiveVelocityKpDegPerMMps;
extern float BallSequence_TunePositiveVelocityKiDegPerMM;

typedef enum
{
    BALL_SEQUENCE_STATE_READY = 0,
    BALL_SEQUENCE_STATE_HOLDING,
    BALL_SEQUENCE_STATE_SWEEP_TO_NEGATIVE,
    BALL_SEQUENCE_STATE_SWEEP_TO_POSITIVE,
    BALL_SEQUENCE_STATE_SWEEP_HOLDING_POSITIVE,
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

/* Move to -50 mm, reverse immediately near the target, then hold +50 mm. */
uint8_t BallSequence_StartSweep(void);

/* Update the target of an active hold without restarting the task. */
uint8_t BallSequence_SetTarget(float targetMM);

void BallSequence_Update(float dt);
void BallSequence_Stop(void);

BallSequence_State_t BallSequence_GetState(void);
BallSequence_Error_t BallSequence_GetError(void);
uint32_t BallSequence_GetElapsedTicks(void);
float BallSequence_GetTargetMM(void);
uint8_t BallSequence_IsActive(void);
/* 仅目标保持模式达到 BallBalance 的稳定判据时返回 1。 */
uint8_t BallSequence_IsStable(void);

#endif
