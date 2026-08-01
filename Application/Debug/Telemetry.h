#ifndef APPLICATION_DEBUG_TELEMETRY_H
#define APPLICATION_DEBUG_TELEMETRY_H

#include <stdint.h>

/*
 * 二进制遥测输出。数据全部通过各模块公共接口读取，本模块不采集也不缓存历史。
 * 协议见 README 3.3.0 与 TelemFrame.h。
 *
 * 掩码变化（含上电）时先发一帧 SCHEMA（列名 + 单位），随后每个输出周期发一帧
 * SAMPLE（ms + 各选中通道 float32）。所有通道都是 float，网页按 schema 列序解析。
 */

#define TELEMETRY_DEFAULT_RATE_HZ    50U   /* 默认频率；DMA 后可显著高于旧 20Hz。 */
#define TELEMETRY_RATE_HARD_LIMIT_HZ 100U  /* 上限等于主循环频率。 */

/* 通道位定义（uint32_t）。位序即 SCHEMA/SAMPLE 的列序，一经发布不得重排。 */
#define TELEMETRY_CH_TL     0x0001U /* 左轮目标速度 mm/s */
#define TELEMETRY_CH_LV     0x0002U /* 左轮实测速度 mm/s */
#define TELEMETRY_CH_PL     0x0004U /* 左轮输出 PWM */
#define TELEMETRY_CH_TR     0x0008U /* 右轮目标速度 mm/s */
#define TELEMETRY_CH_RV     0x0010U /* 右轮实测速度 mm/s */
#define TELEMETRY_CH_PR     0x0020U /* 右轮输出 PWM */
#define TELEMETRY_CH_YAW    0x0040U /* 连续累计航向角 度 */
#define TELEMETRY_CH_NAVE   0x0080U /* 转向角误差 度 */
#define TELEMETRY_CH_LERR   0x0100U /* 巡线权重误差 */
#define TELEMETRY_CH_GRAY   0x0200U /* 六路红外位图，低 6 位 CH1~CH6（CH1 在左） */
#define TELEMETRY_CH_LD     0x0400U /* 左轮累计路程 mm */
#define TELEMETRY_CH_RD     0x0800U /* 右轮累计路程 mm */
#define TELEMETRY_CH_VX     0x1000U /* 视觉最近带偏差 千分比，车道偏右为正 */
#define TELEMETRY_CH_VAD    0x2000U /* 视觉差速修正量 mm/s，正 = 右转 */
#define TELEMETRY_CH_BPOS   0x4000U /* 要求 3 钢球实测位置 mm */
#define TELEMETRY_CH_BREF   0x8000U /* 要求 3 梯形轨迹参考位置 mm */
#define TELEMETRY_CH_SANG   0x10000UL /* Stepper MT6816 PWM absolute angle deg */
#define TELEMETRY_CH_BVEL   0x20000UL /* 钢球实测速度 mm/s */
#define TELEMETRY_CH_BVREF  0x40000UL /* 串级速度环目标 mm/s */
#define TELEMETRY_CH_BINT   0x80000UL /* 球速度误差积分 mm */
#define TELEMETRY_CH_BTCMD  0x100000UL /* 球控制器请求倾角 deg */
#define TELEMETRY_CH_BTAPP  0x200000UL /* 斜率限制后的应用倾角 deg */
#define TELEMETRY_CH_LACC   0x400000UL /* 巡线速度规划加速度 mm/s2 */
#define TELEMETRY_CH_K2PH   0x800000UL /* KEY2 阶段码 */
#define TELEMETRY_CH_K2RS   0x1000000UL /* KEY2 结果码 */
#define TELEMETRY_CH_K2TM   0x2000000UL /* KEY2 已用时间 ms */
#define TELEMETRY_CH_BFRM   0x4000000UL /* 钢球视觉帧序号 */
#define TELEMETRY_CH_BFSH   0x8000000UL /* 钢球视觉新帧标记 */
#define TELEMETRY_CH_ALL    0xFFFFFFFUL

void Telemetry_Init(void);
void Telemetry_Update(uint8_t elapsedTicks, uint8_t pressedKeys);
uint8_t Telemetry_SetRateHz(uint8_t rateHz);
uint8_t Telemetry_SetFieldMask(uint32_t mask);
uint8_t Telemetry_GetRateHz(void);
uint32_t Telemetry_GetFieldMask(void);
uint8_t Telemetry_GetMaxRateHz(void);

#endif
