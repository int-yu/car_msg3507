#ifndef APPLICATION_CONTROL_BALL_TARGET_CAPTURE_H
#define APPLICATION_CONTROL_BALL_TARGET_CAPTURE_H

#include <stdint.h>

/* KEY4 第一次按下后的目标采样参数，现场需要时只改这里。 */
#define BALL_TARGET_CAPTURE_CONFIRM_FRAMES       8U
#define BALL_TARGET_CAPTURE_STABILITY_TOLERANCE_MM 5.0f

typedef enum
{
    BALL_TARGET_CAPTURE_STATE_IDLE = 0,
    BALL_TARGET_CAPTURE_STATE_CAPTURING,
    BALL_TARGET_CAPTURE_STATE_CAPTURED
} BallTargetCapture_State_t;

void BallTargetCapture_Init(void);
void BallTargetCapture_Start(void);
void BallTargetCapture_Update(void);
void BallTargetCapture_Cancel(void);

BallTargetCapture_State_t BallTargetCapture_GetState(void);
uint8_t BallTargetCapture_IsCapturing(void);
uint8_t BallTargetCapture_IsCaptured(void);
uint8_t BallTargetCapture_GetConfirmedFrames(void);
float BallTargetCapture_GetTargetMM(void);

#endif
