#ifndef APPLICATION_CONTROL_MOTION_MANAGER_H
#define APPLICATION_CONTROL_MOTION_MANAGER_H

#include <stdint.h>

#define MOTION_MANAGER_BRAKE_RELEASE_SECONDS 0.01f /* 主动刹车前释放 PWM 的时间。 */
#define MOTION_MANAGER_BRAKE_HOLD_SECONDS    0.05f /* Motor_Brake() 保持时间。 */

/* 同一时刻只允许一种上层运动模式控制双轮。 */
typedef enum
{
    MOTION_MANAGER_MODE_IDLE = 0,
    MOTION_MANAGER_MODE_MANUAL,
    MOTION_MANAGER_MODE_STRAIGHT,
    MOTION_MANAGER_MODE_LINE,
    MOTION_MANAGER_MODE_TURN,
    MOTION_MANAGER_MODE_BRAKE
} MotionManager_Mode_t;

typedef enum
{
    MOTION_MANAGER_ERROR_NONE = 0,
    MOTION_MANAGER_ERROR_INIT,
    MOTION_MANAGER_ERROR_MANUAL,
    MOTION_MANAGER_ERROR_STRAIGHT,
    MOTION_MANAGER_ERROR_LINE,
    MOTION_MANAGER_ERROR_TURN,
    MOTION_MANAGER_ERROR_BRAKE
} MotionManager_Error_t;

typedef enum
{
    MOTION_MANAGER_RESULT_OK = 0,
    MOTION_MANAGER_RESULT_INVALID_ARGUMENT,
    MOTION_MANAGER_RESULT_NOT_CONFIGURED,
    MOTION_MANAGER_RESULT_SENSOR_NOT_READY,
    MOTION_MANAGER_RESULT_START_FAILED
} MotionManager_Result_t;

MotionManager_Result_t MotionManager_Init(void);

/* 设置调试摇杆的左右轮目标速度，单位 mm/s；传入双零会退出手动模式。 */
MotionManager_Result_t MotionManager_SetManualWheelSpeeds(
    float leftSpeedMMps, float rightSpeedMMps);
MotionManager_Result_t MotionManager_StartForward(
    uint32_t distanceMM, float speedMMps, float endSpeedMMps);
MotionManager_Result_t MotionManager_StartBackward(
    uint32_t distanceMM, float speedMMps, float endSpeedMMps);
MotionManager_Result_t MotionManager_StartLine(float speedMMps);
MotionManager_Result_t MotionManager_TurnTo(
    float targetYawDeg, float speedMMps);
MotionManager_Result_t MotionManager_TurnBy(
    float deltaYawDeg, float speedMMps);
MotionManager_Result_t MotionManager_StartBrake(void);

void MotionManager_Update(float dt);
void MotionManager_Stop(void);

uint8_t MotionManager_IsConfigured(void);
uint8_t MotionManager_IsBusy(void);
uint8_t MotionManager_IsFinished(void);
MotionManager_Mode_t MotionManager_GetMode(void);
MotionManager_Error_t MotionManager_GetError(void);

#endif
