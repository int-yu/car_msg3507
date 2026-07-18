#ifndef APPLICATION_CONTROL_MOTION_MANAGER_H
#define APPLICATION_CONTROL_MOTION_MANAGER_H

#include <stdint.h>

/*
 * 定距动作已经按速度曲线减速至零后，先保持 PWM 释放一小段时间，
 * 再短暂调用 Motor_Brake()。前段释放避免从驱动状态直接切到全制动，
 * 后段主动刹车用于抑制车辆残余滑行。
 */
#define MOTION_MANAGER_BRAKE_RELEASE_SECONDS  0.01f  /* 平滑过渡的 PWM 释放时间。 */
#define MOTION_MANAGER_BRAKE_HOLD_SECONDS     0.05f  /* 主动刹车保持时间；过大可能导致顿挫。 */

/* 统一运动调度层：任意时刻只允许一个上层运动模块控制双轮。 */
typedef enum
{
    MOTION_MANAGER_MODE_IDLE = 0,
    MOTION_MANAGER_MODE_STRAIGHT,
    MOTION_MANAGER_MODE_LINE,
    MOTION_MANAGER_MODE_TURN,
    MOTION_MANAGER_MODE_BRAKE
} MotionManager_Mode_t;

typedef enum
{
    MOTION_MANAGER_ERROR_NONE = 0,
    MOTION_MANAGER_ERROR_INIT,
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

MotionManager_Result_t MotionManager_StartForward(
    uint32_t distanceMM, float speedMMps, float endSpeedMMps);
MotionManager_Result_t MotionManager_StartBackward(
    uint32_t distanceMM, float speedMMps, float endSpeedMMps);
MotionManager_Result_t MotionManager_StartLine(float speedMMps);
MotionManager_Result_t MotionManager_TurnTo(
    float targetYawDeg, float speedMMps);
MotionManager_Result_t MotionManager_TurnBy(
    float deltaYawDeg, float speedMMps);
/* 启动固定时长的平滑短暂主动刹车；仅由题目状态机在定距完成后调用。 */
MotionManager_Result_t MotionManager_StartBrake(void);

void MotionManager_Update(float dt);
void MotionManager_Stop(void);

uint8_t MotionManager_IsConfigured(void);
uint8_t MotionManager_IsBusy(void);
uint8_t MotionManager_IsFinished(void);
MotionManager_Mode_t MotionManager_GetMode(void);
MotionManager_Error_t MotionManager_GetError(void);

#endif
