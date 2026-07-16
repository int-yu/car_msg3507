#ifndef APPLICATION_CONTROL_MOTION_WHEEL_H
#define APPLICATION_CONTROL_MOTION_WHEEL_H

#include <stdint.h>

/* 双轮速度控制参数：直线、巡线和转向共用。 */
#define MOTION_WHEEL_KP                       1.0f    /* 轮速误差的即时修正力度。 */
#define MOTION_WHEEL_KI                       0.0f    /* 消除稳态轮速误差；当前关闭。 */
#define MOTION_WHEEL_INTEGRAL_LIMIT           0.0f    /* KI 开启后限制积分累积。 */
#define MOTION_WHEEL_FEEDFORWARD_PWM_PER_MMPS 2.0f    /* 每 1 mm/s 对应的基础 PWM。 */
#define MOTION_WHEEL_STATIC_FRICTION_PWM      0.0f    /* 非零目标速度的静摩擦补偿。 */
#define MOTION_WHEEL_MAX_COMMAND_PWM          1000.0f /* 每侧车轮最终 PWM 上限。 */

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

MotionWheel_Result_t MotionWheel_Init(void);
MotionWheel_Result_t MotionWheel_Update(
    const MotionWheel_Command_t *command, float dt);
void MotionWheel_Reset(void);
void MotionWheel_Stop(void);

uint8_t MotionWheel_IsConfigured(void);
float MotionWheel_GetMaximumCommandPWM(void);
float MotionWheel_GetLeftCommandPWM(void);
float MotionWheel_GetRightCommandPWM(void);

#endif
