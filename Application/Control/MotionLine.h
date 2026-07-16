#ifndef APPLICATION_CONTROL_MOTION_LINE_H
#define APPLICATION_CONTROL_MOTION_LINE_H

#include <stdint.h>

/* 五路灰度巡线参数：首次实车测试从低速开始，每次只调整一项。 */
#define MOTION_LINE_KP                   6.0f   /* 偏线后的回线力度。 */
#define MOTION_LINE_KD                   0.0f   /* 抑制快速摆动；当前关闭。 */
#define MOTION_LINE_CORRECTION_LIMIT_PWM 300.0f /* 左右轮附加差速上限。 */
#define MOTION_LINE_CORRECTION_SIGN      (-1)   /* 越修越偏时翻转符号。 */
#define MOTION_LINE_MAX_SPEED_MMPS       600.0f /* 巡线请求速度上限。 */

typedef enum
{
    MOTION_LINE_STATE_IDLE = 0,
    MOTION_LINE_STATE_RUNNING,
    MOTION_LINE_STATE_ERROR
} MotionLine_State_t;

typedef enum
{
    MOTION_LINE_ERROR_NONE = 0,
    MOTION_LINE_ERROR_LINE_LOST,
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
/* 持续巡线，直到调用 MotionLine_Stop() 或检测到丢线。 */
MotionLine_Result_t MotionLine_Start(float speedMMps);
void MotionLine_Update(float dt);
void MotionLine_Stop(void);

uint8_t MotionLine_IsConfigured(void);
uint8_t MotionLine_IsBusy(void);
MotionLine_State_t MotionLine_GetState(void);
MotionLine_Error_t MotionLine_GetError(void);
float MotionLine_GetLineError(void);

#endif
