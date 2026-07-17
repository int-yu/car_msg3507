#ifndef APPLICATION_CONTROL_MOTION_LINE_H
#define APPLICATION_CONTROL_MOTION_LINE_H

#include <stdint.h>

/* 五路灰度巡线参数：灰度返回 1 表示检测到黑线。 */
#define MOTION_LINE_OUTER_WEIGHT        6       /* 左右最外侧灰度权重的绝对值。 */
#define MOTION_LINE_INNER_WEIGHT        3       /* 左右内侧灰度权重的绝对值。 */
#define MOTION_LINE_MAX_ADJUST_RATIO    0.2f    /* 最外侧压线时，每侧增减当前速度的比例。 */
#define MOTION_LINE_MAX_SPEED_MMPS      1000.0f /* MotionLine_Start() 允许的巡线速度上限。 */
#define MOTION_LINE_LOST_CONFIRM_TICKS  50U     /* 连续全白 50 个控制节拍后确认丢线。 */

typedef enum
{
    MOTION_LINE_STATE_IDLE = 0,
    MOTION_LINE_STATE_RUNNING,
    MOTION_LINE_STATE_FINISHED,
    MOTION_LINE_STATE_ERROR
} MotionLine_State_t;

typedef enum
{
    MOTION_LINE_ERROR_NONE = 0,
    MOTION_LINE_ERROR_UPDATE_PERIOD_INVALID,
    MOTION_LINE_ERROR_WHEEL
} MotionLine_Error_t;

typedef enum
{
    MOTION_LINE_RESULT_OK = 0,
    MOTION_LINE_RESULT_BUSY,
    MOTION_LINE_RESULT_INVALID_ARGUMENT,
    MOTION_LINE_RESULT_NOT_CONFIGURED
} MotionLine_Result_t;

MotionLine_Result_t MotionLine_Init(void);
/* 持续巡线，直到调用 MotionLine_Stop() 或确认丢线。 */
MotionLine_Result_t MotionLine_Start(float speedMMps);
void MotionLine_Update(float dt);
void MotionLine_Stop(void);

uint8_t MotionLine_IsConfigured(void);
uint8_t MotionLine_IsBusy(void);
uint8_t MotionLine_IsFinished(void);
MotionLine_State_t MotionLine_GetState(void);
MotionLine_Error_t MotionLine_GetError(void);
float MotionLine_GetLineError(void);

#endif
