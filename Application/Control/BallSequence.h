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
#define BALL_SEQUENCE_POSITIVE_HOLD_MS 5000U

#define BALL_SEQUENCE_POSITION_KP_PER_S          2.0f
#define BALL_SEQUENCE_VELOCITY_KP_DEG_PER_MMPS   0.4f
#define BALL_SEQUENCE_VELOCITY_KI_DEG_PER_MM     0.1f

/* KEY2 phase 2 (-50 mm -> +50 mm) uses a slightly more assertive set. */
#define BALL_SEQUENCE_POSITIVE_POSITION_KP_PER_S    3.0f
#define BALL_SEQUENCE_POSITIVE_VELOCITY_KP_DEG_PER_MMPS 0.4f
#define BALL_SEQUENCE_POSITIVE_VELOCITY_KI_DEG_PER_MM   0.1f

#define BALL_SEQUENCE_NEGATIVE_MAX_VELOCITY_MMPS       60.0f
#define BALL_SEQUENCE_NEGATIVE_APPROACH_DISTANCE_MM    14.0f
#define BALL_SEQUENCE_NEGATIVE_TERMINAL_VELOCITY_MMPS  30.0f
#define BALL_SEQUENCE_NEGATIVE_MAX_TILT_DEG             50.0f
#define BALL_SEQUENCE_NEGATIVE_INTEGRAL_LIMIT_MM       300.0f

#define BALL_SEQUENCE_POSITIVE_MAX_VELOCITY_MMPS       90.0f
#define BALL_SEQUENCE_POSITIVE_APPROACH_DISTANCE_MM    10.0f
#define BALL_SEQUENCE_POSITIVE_TERMINAL_VELOCITY_MMPS  40.0f
#define BALL_SEQUENCE_POSITIVE_MAX_TILT_DEG             30.0f
#define BALL_SEQUENCE_POSITIVE_INTEGRAL_LIMIT_MM       300.0f

/* Web/K-command range; phase defaults remain independently configurable. */
#define BALL_SEQUENCE_MAX_CONFIGURABLE_TILT_DEG          60.0f

extern float BallSequence_TunePositionKpPerS;
extern float BallSequence_TuneVelocityKpDegPerMMps;
extern float BallSequence_TuneVelocityKiDegPerMM;
extern float BallSequence_TunePositivePositionKpPerS;
extern float BallSequence_TunePositiveVelocityKpDegPerMMps;
extern float BallSequence_TunePositiveVelocityKiDegPerMM;
extern float BallSequence_TuneNegativeMaxVelocityMMps;
extern float BallSequence_TuneNegativeApproachDistanceMM;
extern float BallSequence_TuneNegativeTerminalVelocityMMps;
extern float BallSequence_TuneNegativeMaxTiltDeg;
extern float BallSequence_TuneNegativeIntegralLimitMM;
extern float BallSequence_TunePositiveMaxVelocityMMps;
extern float BallSequence_TunePositiveApproachDistanceMM;
extern float BallSequence_TunePositiveTerminalVelocityMMps;
extern float BallSequence_TunePositiveMaxTiltDeg;
extern float BallSequence_TunePositiveIntegralLimitMM;

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
    BALL_SEQUENCE_ERROR_BALANCE,
    BALL_SEQUENCE_ERROR_TIMEOUT_NEGATIVE,
    BALL_SEQUENCE_ERROR_TIMEOUT_POSITIVE,
    BALL_SEQUENCE_ERROR_STALL,
    BALL_SEQUENCE_ERROR_OVERSHOOT
} BallSequence_Error_t;

void BallSequence_Init(void);

/* Start holding targetMM. Returns 0 if vision or target validation fails. */
uint8_t BallSequence_Start(float targetMM);

/* Move toward -50 mm, switch at -40 mm, then hold +50 mm for 5 seconds. */
uint8_t BallSequence_StartSweep(void);

/* Update the target of an active hold without restarting the task. */
uint8_t BallSequence_SetTarget(float targetMM);

void BallSequence_Update(float dt);
void BallSequence_Stop(void);

BallSequence_State_t BallSequence_GetState(void);
BallSequence_Error_t BallSequence_GetError(void);
uint32_t BallSequence_GetElapsedTicks(void);
uint32_t BallSequence_GetElapsedMilliseconds(void);
float BallSequence_GetTargetMM(void);
uint8_t BallSequence_IsActive(void);
uint8_t BallSequence_IsStable(void);
uint8_t BallSequence_GetTelemetryPhaseCode(void);
uint8_t BallSequence_GetTelemetryResultCode(void);

#endif
