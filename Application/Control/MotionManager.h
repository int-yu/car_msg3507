#ifndef APPLICATION_CONTROL_MOTION_MANAGER_H
#define APPLICATION_CONTROL_MOTION_MANAGER_H

#include <stdint.h>

/* 统一运动调度层：任意时刻只允许一个上层运动模块控制双轮。 */
typedef enum
{
    MOTION_MANAGER_MODE_IDLE = 0,
    MOTION_MANAGER_MODE_STRAIGHT,
    MOTION_MANAGER_MODE_LINE,
    MOTION_MANAGER_MODE_TURN
} MotionManager_Mode_t;

typedef enum
{
    MOTION_MANAGER_ERROR_NONE = 0,
    MOTION_MANAGER_ERROR_INIT,
    MOTION_MANAGER_ERROR_STRAIGHT,
    MOTION_MANAGER_ERROR_LINE,
    MOTION_MANAGER_ERROR_TURN
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

MotionManager_Result_t MotionManager_StartForward(
    uint32_t distanceMM, float speedMMps, float endSpeedMMps);
MotionManager_Result_t MotionManager_StartBackward(
    uint32_t distanceMM, float speedMMps, float endSpeedMMps);
MotionManager_Result_t MotionManager_StartLine(float speedMMps);
MotionManager_Result_t MotionManager_TurnTo(
    float targetYawDeg, float speedMMps);
MotionManager_Result_t MotionManager_TurnBy(
    float deltaYawDeg, float speedMMps);

void MotionManager_Update(float dt);
void MotionManager_Stop(void);

uint8_t MotionManager_IsConfigured(void);
uint8_t MotionManager_IsBusy(void);
uint8_t MotionManager_IsFinished(void);
MotionManager_Mode_t MotionManager_GetMode(void);
MotionManager_Error_t MotionManager_GetError(void);

#endif
