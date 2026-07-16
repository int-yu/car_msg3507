#include "Application/Control/MotionStraightConfig.h"

/* 当前直线实车测试参数；调节后必须同步 README。 */
const MotionStraight_Config_t g_motionStraightConfig = {
    .heading = {
        .kp = 6.0f,                       /* 航向比例增益初值 */
        .kd = 0.4f,                       /* 航向微分增益初值 */
        .correctionLimitPWM = 300.0f,     /* 航向差速限幅 */
        .correctionSign = -1,             /* 纠偏方向错误时改为 1 */
    },
    .maximumSpeedMMps = 600.0f,           /* 直线请求速度上限 */
    .accelerationMMps2 = 200.0f,          /* 加速度初值 */
    .decelerationMMps2 = 250.0f,          /* 最大减速度，300 mm/s 测试可在末段 1/6 内降至零 */
    .decelerationStartRatio = 5.0f / 6.0f, /* 默认行驶到全程 5/6 后开始减速 */
    .distanceToleranceMM = 5.0f,          /* 到点允许误差 */
};
