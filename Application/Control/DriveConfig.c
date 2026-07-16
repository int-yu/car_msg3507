#include "Application/Control/DriveConfig.h"

/* 第一轮低速上机测试参数，不是最终标定结果。 */
const Drive_Config_t g_driveConfig = {
    .speed = {
        .kp = 1.0f,                       /* 速度比例增益 */
        .ki = 0.0f,                       /* 第一轮关闭速度积分 */
        .integralLimit = 0.0f,            /* 积分关闭时不使用 */
        .feedforwardPWMPerMMps = 2.0f,    /* 速度前馈初值 */
        .staticFrictionPWM = 80.0f,       /* 克服静摩擦的 PWM 初值 */
    },
    .heading = {
        .kp = 6.0f,                       /* 航向比例增益初值 */
        .kd = 0.4f,                       /* 航向微分增益初值 */
        .correctionLimitPWM = 300.0f,     /* 航向差速限幅 */
        .correctionSign = -1,              /* 纠偏方向错误时改为 -1 */
    },
    .maximumSpeedMMps = 200.0f,           /* 第一轮最大测试速度 */
    .maximumCommandPWM = 300.0f,          /* 自动行驶允许的最大 PWM */
    .accelerationMMps2 = 120.0f,          /* 加速度初值 */
    .decelerationMMps2 = 200.0f,          /* 减速度初值 */
    .minimumApproachSpeedMMps = 20.0f,     /* 接近终点的最低速度 */
    .distanceToleranceMM = 5.0f,           /* 到点允许误差 */
    .brakeDurationS = 0.05f,               /* 到点主动制动时间 */
};
