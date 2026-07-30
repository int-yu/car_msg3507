#ifndef APPLICATION_CONTROL_BALL_BALANCE_H
#define APPLICATION_CONTROL_BALL_BALANCE_H

#include <stdint.h>

/*
 * Ball-on-beam position controller.
 *
 * The public command is the ball position to hold, in millimeters from the
 * beam center O. This version is a direct PID controller:
 *
 *     tilt = Kp * (target - position)
 *          + Ki * integral(target - position)
 *          + Kd * (0 - measured_speed)
 *
 * No chassis acceleration feedforward and no generated O->+5->-5 task are
 * used here. BeamActuator owns the conversion from tilt angle to stepper
 * angle, gear ratio, zero offset, direction and soft limits.
 */

#define BALL_BALANCE_KP_DEG_PER_MM      0.24f
#define BALL_BALANCE_KI_DEG_PER_MM_S    0.0f
#define BALL_BALANCE_KD_DEG_PER_MMPS    0.087f

#define BALL_BALANCE_MAX_TILT_DEG       6.0f
#define BALL_BALANCE_INTEGRAL_LIMIT_MM_S 80.0f

#define BALL_BALANCE_TARGET_LIMIT_MM    120.0f
#define BALL_BALANCE_SETTLE_TOLERANCE_MM 5.0f
#define BALL_BALANCE_SETTLE_CONFIRM_TICKS 15U

/* 100 Hz control loop: tolerate about 100 ms of missing vision. */
#define BALL_BALANCE_VISION_LOST_TICKS  10U

extern float BallBalance_TuneKp;
extern float BallBalance_TuneKi;
extern float BallBalance_TuneKd;

typedef enum
{
    BALL_BALANCE_STATE_IDLE = 0,
    BALL_BALANCE_STATE_RUNNING,
    BALL_BALANCE_STATE_ERROR
} BallBalance_State_t;

typedef enum
{
    BALL_BALANCE_ERROR_NONE = 0,
    BALL_BALANCE_ERROR_VISION_LOST,
    BALL_BALANCE_ERROR_UPDATE_PERIOD_INVALID
} BallBalance_Error_t;

typedef enum
{
    BALL_BALANCE_RESULT_OK = 0,
    BALL_BALANCE_RESULT_INVALID_ARGUMENT,
    BALL_BALANCE_RESULT_NOT_RUNNING
} BallBalance_Result_t;

void BallBalance_Init(void);

/* Start holding targetMM. Ball vision must already be fresh. */
BallBalance_Result_t BallBalance_Start(float targetMM);

/* Retarget the running PID controller to another hold position. */
BallBalance_Result_t BallBalance_SetTarget(float targetMM);

void BallBalance_Update(float dt);
void BallBalance_Stop(void);

BallBalance_State_t BallBalance_GetState(void);
BallBalance_Error_t BallBalance_GetError(void);
uint8_t BallBalance_IsStable(void);
float BallBalance_GetPositionMM(void);
float BallBalance_GetTargetMM(void);
float BallBalance_GetProfilePositionMM(void);
float BallBalance_GetTiltCommandDeg(void);
float BallBalance_GetIntegralMMs(void);

#endif
