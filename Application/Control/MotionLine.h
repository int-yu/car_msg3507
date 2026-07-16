#ifndef APPLICATION_CONTROL_MOTION_LINE_H
#define APPLICATION_CONTROL_MOTION_LINE_H

#include <stdint.h>

typedef struct
{
    float kp;
    float kd;
    float correctionLimitPWM;
    int8_t correctionSign;
    float maximumSpeedMMps;
} MotionLine_Config_t;

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

MotionLine_Result_t MotionLine_Init(const MotionLine_Config_t *config);
MotionLine_Result_t MotionLine_InitDefault(void);
MotionLine_Result_t MotionLine_Start(float speedMMps);
void MotionLine_Update(float dt);
void MotionLine_Stop(void);

uint8_t MotionLine_IsConfigured(void);
uint8_t MotionLine_IsBusy(void);
MotionLine_State_t MotionLine_GetState(void);
MotionLine_Error_t MotionLine_GetError(void);
float MotionLine_GetLineError(void);

#endif
