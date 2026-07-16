#ifndef APPLICATION_CONTROL_MOTION_STRAIGHT_H
#define APPLICATION_CONTROL_MOTION_STRAIGHT_H

#include <stdint.h>

/* MPU6050 航向保持 PD 参数。 */
typedef struct
{
    float kp;
    float kd;
    float correctionLimitPWM;
    int8_t correctionSign;
} MotionStraight_HeadingConfig_t;

/* 直线行驶控制参数，必须根据实车标定后填写。 */
typedef struct
{
    MotionStraight_HeadingConfig_t heading;
    float maximumSpeedMMps;
    float accelerationMMps2;
    float decelerationMMps2;
    float decelerationStartRatio;
    float distanceToleranceMM;
} MotionStraight_Config_t;

typedef enum
{
    MOTION_STRAIGHT_STATE_IDLE = 0,
    MOTION_STRAIGHT_STATE_RUNNING,
    MOTION_STRAIGHT_STATE_CONTINUING,
    MOTION_STRAIGHT_STATE_COMPLETED,
    MOTION_STRAIGHT_STATE_ERROR
} MotionStraight_State_t;

typedef enum
{
    MOTION_STRAIGHT_ERROR_NONE = 0,
    MOTION_STRAIGHT_ERROR_HEADING_OFFLINE,
    MOTION_STRAIGHT_ERROR_ODOMETRY_INVALID,
    MOTION_STRAIGHT_ERROR_UPDATE_PERIOD_INVALID,
    MOTION_STRAIGHT_ERROR_WHEEL
} MotionStraight_Error_t;

typedef enum
{
    MOTION_STRAIGHT_RESULT_OK = 0,
    MOTION_STRAIGHT_RESULT_BUSY,
    MOTION_STRAIGHT_RESULT_INVALID_ARGUMENT,
    MOTION_STRAIGHT_RESULT_NOT_CONFIGURED,
    MOTION_STRAIGHT_RESULT_SENSOR_NOT_READY
} MotionStraight_Result_t;

MotionStraight_Result_t MotionStraight_Init(const MotionStraight_Config_t *config);
MotionStraight_Result_t MotionStraight_InitDefault(void);
MotionStraight_Result_t MotionStraight_Start(
    float distanceMM, float speedMMps, float endSpeedMMps);
MotionStraight_Result_t MotionStraight_StartForward(
    uint32_t distanceMM, float speedMMps, float endSpeedMMps);
MotionStraight_Result_t MotionStraight_StartBackward(
    uint32_t distanceMM, float speedMMps, float endSpeedMMps);
void MotionStraight_Update(float dt);
void MotionStraight_Stop(void);

uint8_t MotionStraight_IsConfigured(void);
uint8_t MotionStraight_IsBusy(void);
uint8_t MotionStraight_IsFinished(void);
MotionStraight_State_t MotionStraight_GetState(void);
MotionStraight_Error_t MotionStraight_GetError(void);
float MotionStraight_GetRemainingDistanceMM(void);

#endif
