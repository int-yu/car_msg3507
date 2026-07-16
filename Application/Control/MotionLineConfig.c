#include "Application/Control/MotionLineConfig.h"

/* 未接入主流程的当前初始值，必须在实车巡线前继续标定。 */
const MotionLine_Config_t g_motionLineConfig = {
    .kp = 6.0f,                   /* 灰度位置误差比例增益 */
    .kd = 0.0f,                   /* 第一轮关闭微分 */
    .correctionLimitPWM = 300.0f, /* 左右轮差速修正限幅 */
    .correctionSign = -1,          /* 转向相反时改为 -1 */
    .maximumSpeedMMps = 600.0f,   /* 巡线请求速度上限 */
};
