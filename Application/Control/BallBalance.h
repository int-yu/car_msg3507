#ifndef APPLICATION_CONTROL_BALL_BALANCE_H
#define APPLICATION_CONTROL_BALL_BALANCE_H

#include <stdint.h>

/*
 * Ball-on-beam position controller.
 *
 * The public command is the ball position to hold, in millimeters from the
 * beam center O. The controller is cascaded:
 *
 *     velocity_target = position_Kp * (target - position)
 *     tilt = velocity_Kp * (velocity_target - measured_velocity)
 *          + velocity_Ki * integral(velocity_target - measured_velocity)
 *          + feedforward_coeff * chassis_acceleration
 *
 * Chassis acceleration feedforward compensates for vehicle acceleration to
 * reduce ball position disturbance during startup and speed changes.
 * BeamActuator owns the conversion from tilt angle to stepper angle, gear
 * ratio, zero offset, direction and soft limits.
 */

#define BALL_BALANCE_POSITION_KP_PER_S          2.0f
#define BALL_BALANCE_VELOCITY_KP_DEG_PER_MMPS   0.25f
#define BALL_BALANCE_VELOCITY_KI_DEG_PER_MM     0.20f
#define BALL_BALANCE_MAX_VELOCITY_MMPS          150.0f
#define BALL_BALANCE_FEEDFORWARD_DEG_PER_MMPS2  0.02f
#define BALL_BALANCE_FEEDFORWARD_SPEED_THRESHOLD_MMPS 50.0f

#define BALL_BALANCE_VELOCITY_INTEGRAL_LIMIT_MM 400.0f

#define BALL_BALANCE_TARGET_LIMIT_MM    120.0f
#define BALL_BALANCE_SETTLE_TOLERANCE_MM 5.0f
#define BALL_BALANCE_SETTLE_CONFIRM_TICKS 15U

/* 100 Hz control loop: tolerate about 100 ms of missing vision. */
#define BALL_BALANCE_VISION_LOST_TICKS  10U

extern float BallBalance_TunePositionKpPerS;
extern float BallBalance_TuneVelocityKpDegPerMMps;
extern float BallBalance_TuneVelocityKiDegPerMM;
extern float BallBalance_TuneMaxVelocityMMps;
extern float BallBalance_TuneFeedforwardDegPerMMps2;
extern float BallBalance_TuneFeedforwardSpeedThresholdMMps;

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
float BallBalance_GetVelocityTargetMMps(void);
float BallBalance_GetTiltCommandDeg(void);
float BallBalance_GetIntegralMMs(void);

#endif
