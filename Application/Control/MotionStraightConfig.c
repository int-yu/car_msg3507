#include "Application/Control/MotionStraightConfig.h"

/*
 * 当前参数已经完成本轮直线实车调试；后续换电池、电机、轮胎或路面后仍需复核。
 * 推荐顺序：先标定 MotionWheel，再调航向 kp/kd，最后调加减速度和距离误差。
 */
const MotionStraight_Config_t g_motionStraightConfig = {
    .heading = {
        .kp = 6.0f,                       /* 偏航回正力度；偏差收敛太慢可增大 */
        .kd = 0.4f,                       /* 抑制左右摆动；噪声放大时减小 */
        .correctionLimitPWM = 300.0f,     /* 航向修正最多可占用的差速 PWM */
        .correctionSign = -1,             /* 越修越偏时只翻转为 1 */
    },
    .maximumSpeedMMps = 600.0f,           /* 所有直线任务的请求速度上限 */
    .accelerationMMps2 = 200.0f,          /* 数值越小，起步越柔和但加速距离越长 */
    .decelerationMMps2 = 250.0f,          /* 数值越小，停车越柔和但需更早开始减速 */
    .decelerationStartRatio = 5.0f / 6.0f, /* 首选在全程 5/6 处开始减速 */
    .distanceToleranceMM = 5.0f,          /* 越小越准确，但编码器抖动影响越明显 */
};
