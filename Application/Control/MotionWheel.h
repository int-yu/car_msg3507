#ifndef APPLICATION_CONTROL_MOTION_WHEEL_H
#define APPLICATION_CONTROL_MOTION_WHEEL_H

#include <stdint.h>

/* 双轮速度 PI、前馈和最终 PWM 限幅参数。 */
typedef struct
{
    float kp;
    float ki;
    float integralLimit;
    float feedforwardPWMPerMMps;
    float staticFrictionPWM;
    float maximumCommandPWM;
} MotionWheel_Config_t;

/* 上层控制器每个周期提交的左右轮目标和附加差速修正。 */
typedef struct
{
    float targetSpeedLMMps;
    float targetSpeedRMMps;
    float trimLPWM;
    float trimRPWM;
} MotionWheel_Command_t;

typedef enum
{
    MOTION_WHEEL_RESULT_OK = 0,
    MOTION_WHEEL_RESULT_INVALID_ARGUMENT,
    MOTION_WHEEL_RESULT_NOT_CONFIGURED,
    MOTION_WHEEL_RESULT_ODOMETRY_INVALID
} MotionWheel_Result_t;

MotionWheel_Result_t MotionWheel_Init(const MotionWheel_Config_t *config);
MotionWheel_Result_t MotionWheel_InitDefault(void);
MotionWheel_Result_t MotionWheel_Update(
    const MotionWheel_Command_t *command, float dt);
void MotionWheel_Reset(void);
void MotionWheel_Stop(void);

uint8_t MotionWheel_IsConfigured(void);
float MotionWheel_GetMaximumCommandPWM(void);
float MotionWheel_GetLeftCommandPWM(void);
float MotionWheel_GetRightCommandPWM(void);

#endif
