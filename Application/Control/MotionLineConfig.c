#include "Application/Control/MotionLineConfig.h"

/*
 * 巡线参数尚未实车标定，明日上车时建议按以下顺序调整：
 * 1. 先使用低速并保持 kd=0，增大 kp，直到能纠偏但不过度左右摆动。
 * 2. 出现快速摆动时再少量增加 kd；灰度误差是离散量，kd 不宜一次加太大。
 * 3. 若车越修越偏，只翻转 correctionSign，不要同时修改 kp/kd。
 * 4. 最后提高巡线速度，并按需要增大 correctionLimitPWM。
 */
const MotionLine_Config_t g_motionLineConfig = {
    .kp = 6.0f,                   /* 初始比例增益；偏离后回线太慢则增大 */
    .kd = 0.0f,                   /* 首轮关闭；摆动明显时从小量开始增加 */
    .correctionLimitPWM = 300.0f, /* 初始差速限幅，与直线航向修正限幅一致 */
    .correctionSign = -1,         /* 若误差增大而不是减小，改为 1 */
    .maximumSpeedMMps = 600.0f,   /* 软件请求上限；初次巡线不要直接跑到上限 */
};
