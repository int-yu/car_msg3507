#ifndef APPLICATION_CONTROL_MOTION_STRAIGHT_H
#define APPLICATION_CONTROL_MOTION_STRAIGHT_H

#include <stdint.h>

/* 直线行驶参数：换电池、电机、轮胎或路面后需要重新检查。 */
#define MOTION_STRAIGHT_HEADING_KP              6.0f   /* 偏航回正力度。 */
#define MOTION_STRAIGHT_HEADING_KD              0.4f   /* 抑制航向左右摆动。 */
#define MOTION_STRAIGHT_HEADING_LIMIT_PWM        700.0f /* 航向差速修正上限。 */
#define MOTION_STRAIGHT_CORRECTION_SIGN          (-1)   /* 越修越偏时翻转符号。 */
#define MOTION_STRAIGHT_MAX_SPEED_MMPS           1000.0f /* 直线请求速度上限。 */
#define MOTION_STRAIGHT_ACCELERATION_MMPS2       300.0f /* 起步加速度。 */
#define MOTION_STRAIGHT_DECELERATION_START_RATIO (5.0f / 6.0f) /* 按全程比例进入匀减速。 */
#define MOTION_STRAIGHT_DISTANCE_TOLERANCE_MM    5.0f   /* 到达距离允许误差。 */
#define MOTION_STRAIGHT_ZERO_SPEED_HOLD_SECONDS  0.05f  /* 到零速目标后继续闭环保持的时间。 */

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

MotionStraight_Result_t MotionStraight_Init(void);
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
