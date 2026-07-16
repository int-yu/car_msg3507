#ifndef APPLICATION_CONTROL_DRIVE_H
#define APPLICATION_CONTROL_DRIVE_H

#include <stdint.h>

/* 常用直线速度档位，枚举值直接对应 mm/s。 */
typedef enum
{
    DRIVE_SPEED_SLOW = 60,
    DRIVE_SPEED_NORMAL = 100,
    DRIVE_SPEED_FAST = 120
} Drive_Speed_t;

/* 单轮速度 PI 与前馈参数。 */
typedef struct
{
    float kp;
    float ki;
    float integralLimit;
    float feedforwardPWMPerMMps;
    float staticFrictionPWM;
} Drive_SpeedControlConfig_t;

/* MPU6050 航向保持 PD 参数。 */
typedef struct
{
    float kp;
    float kd;
    float correctionLimitPWM;
    int8_t correctionSign;
} Drive_HeadingControlConfig_t;

/* 直线行驶控制参数，必须根据实车标定后填写。 */
typedef struct
{
    Drive_SpeedControlConfig_t speed;
    Drive_HeadingControlConfig_t heading;
    float maximumSpeedMMps;
    float maximumCommandPWM;
    float accelerationMMps2;
    float decelerationMMps2;
    float minimumApproachSpeedMMps;
    float distanceToleranceMM;
    float brakeDurationS;
} Drive_Config_t;

typedef enum
{
    DRIVE_STATE_IDLE = 0,
    DRIVE_STATE_RUNNING,
    DRIVE_STATE_BRAKING,
    DRIVE_STATE_COMPLETED,
    DRIVE_STATE_ERROR
} Drive_State_t;

typedef enum
{
    DRIVE_ERROR_NONE = 0,
    DRIVE_ERROR_HEADING_OFFLINE,
    DRIVE_ERROR_ODOMETRY_INVALID,
    DRIVE_ERROR_UPDATE_PERIOD_INVALID
} Drive_Error_t;

typedef enum
{
    DRIVE_RESULT_OK = 0,
    DRIVE_RESULT_BUSY,
    DRIVE_RESULT_INVALID_ARGUMENT,
    DRIVE_RESULT_NOT_CONFIGURED,
    DRIVE_RESULT_SENSOR_NOT_READY
} Drive_Result_t;

Drive_Result_t Drive_Init(const Drive_Config_t *config);
Drive_Result_t Drive_InitDefault(void);
Drive_Result_t Drive_StartStraight(float distanceMM, float speedMMps);
Drive_Result_t Drive_StartForward(uint32_t distanceMM, Drive_Speed_t speed);
Drive_Result_t Drive_StartBackward(uint32_t distanceMM, Drive_Speed_t speed);
void Drive_Update(float dt);
void Drive_Stop(void);

uint8_t Drive_IsConfigured(void);
uint8_t Drive_IsBusy(void);
uint8_t Drive_IsFinished(void);
Drive_State_t Drive_GetState(void);
Drive_Error_t Drive_GetError(void);
float Drive_GetRemainingDistanceMM(void);

#endif
