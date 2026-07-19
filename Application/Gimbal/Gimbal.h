#ifndef APPLICATION_GIMBAL_GIMBAL_H
#define APPLICATION_GIMBAL_GIMBAL_H

#include <stdint.h>

/* 二自由度云台公共配置：X=地址1，Y=地址2。 */
#define GIMBAL_X_MOTOR_ADDRESS                 1U
#define GIMBAL_Y_MOTOR_ADDRESS                 2U
#define GIMBAL_DEFAULT_SPEED_RPM               100
#define GIMBAL_DEFAULT_ACCELERATION_RPMS2      100U
#define GIMBAL_FEEDBACK_REQUEST_PERIOD_SECONDS 0.03f
#define GIMBAL_FEEDBACK_TIMEOUT_SECONDS        1.0f
#define GIMBAL_POSITION_TOLERANCE_DEG          2.0f
#define GIMBAL_POSITION_SETTLE_FEEDBACKS       2U

typedef enum
{
    GIMBAL_AXIS_X = 0,
    GIMBAL_AXIS_Y,
    GIMBAL_AXIS_COUNT
} Gimbal_Axis_t;

typedef enum
{
    GIMBAL_STATE_UNINITIALIZED = 0,
    GIMBAL_STATE_DISABLED,
    GIMBAL_STATE_ENABLED,
    GIMBAL_STATE_ERROR
} Gimbal_State_t;

typedef enum
{
    GIMBAL_ERROR_NONE = 0,
    GIMBAL_ERROR_PARAMETER,
    GIMBAL_ERROR_PROTOCOL,
    GIMBAL_ERROR_FEEDBACK_TIMEOUT
} Gimbal_Error_t;

typedef enum
{
    GIMBAL_RESULT_OK = 0,
    GIMBAL_RESULT_INVALID_ARGUMENT,
    GIMBAL_RESULT_NOT_INITIALIZED,
    GIMBAL_RESULT_NOT_ENABLED,
    GIMBAL_RESULT_PROTOCOL_ERROR
} Gimbal_Result_t;

Gimbal_Result_t Gimbal_Init(void);
Gimbal_Result_t Gimbal_Enable(void);
Gimbal_Result_t Gimbal_Disable(void);
Gimbal_Result_t Gimbal_SetTargetAngle(
    Gimbal_Axis_t axis, float targetAngleDeg);
Gimbal_Result_t Gimbal_SetTargetAngles(
    float targetXDeg, float targetYDeg);
void Gimbal_Update(float dt);

uint8_t Gimbal_IsEnabled(void);
uint8_t Gimbal_HasFeedback(Gimbal_Axis_t axis);
uint8_t Gimbal_IsAxisAtTarget(Gimbal_Axis_t axis);
uint8_t Gimbal_AreTargetsReached(void);
float Gimbal_GetCurrentAngleDeg(Gimbal_Axis_t axis);
float Gimbal_GetTargetAngleDeg(Gimbal_Axis_t axis);
Gimbal_State_t Gimbal_GetState(void);
Gimbal_Error_t Gimbal_GetError(void);

#endif
