#include "Application/Control/NavConfig.h"

/*
 * 以下数值仅用于第一次低速上机，尚未经过实车标定：
 * 1. 先以 60~80 mm/s 测试 rotationCommandSign，若角度误差增大则翻转符号。
 * 2. 再调整 angleToleranceDeg 和 settleTicks，兼顾到角精度与停止抖动。
 * 3. 接近目标时冲过角度，可增大 decelerationMMps2、增大 slowdownAngleDeg，
 *    或减小 minimumTurnSpeedMMps；每次只改一项并记录现象。
 * 4. 接近目标后无法继续转动，适当增大 minimumTurnSpeedMMps。
 */
const Nav_Config_t g_navConfig = {
    .maximumTurnSpeedMMps = 200.0f, /* 转向轮速软件上限，首次测试不要直接用满 */
    .minimumTurnSpeedMMps = 40.0f,  /* 接近目标时的初始最低轮速 */
    .slowdownAngleDeg = 30.0f,      /* 剩余 30° 开始降低轮速 */
    .accelerationMMps2 = 300.0f,    /* 起转轮速上升斜率 */
    .decelerationMMps2 = 300.0f,    /* 越大越能快速跟随下降目标，过大时停车会突兀 */
    .angleToleranceDeg = 2.0f,      /* 初始到角允许误差 */
    .settleTicks = 5U,              /* 连续稳定 50 ms 后完成 */
    .rotationCommandSign = 1,       /* 若转向后误差变大，改为 -1 */
};
