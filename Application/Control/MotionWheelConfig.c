#include "Application/Control/MotionWheelConfig.h"

/* 当前实车测试参数；直线与巡线模式共用。 */
const MotionWheel_Config_t g_motionWheelConfig = {
    .kp = 1.0f,                       /* 单轮速度比例增益 */
    .ki = 0.0f,                       /* 当前关闭速度积分 */
    .integralLimit = 0.0f,            /* 积分关闭时不使用 */
    .feedforwardPWMPerMMps = 2.0f,    /* 速度前馈斜率 */
    .staticFrictionPWM = 80.0f,       /* 克服静摩擦的 PWM */
    .maximumCommandPWM = 1000.0f,      /* 单轮最终 PWM 限幅 */
};
