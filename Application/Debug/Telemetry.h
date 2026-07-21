#ifndef APPLICATION_DEBUG_TELEMETRY_H
#define APPLICATION_DEBUG_TELEMETRY_H

#include <stdint.h>

/*
 * 二进制遥测输出。数据全部通过各模块公共接口读取，本模块不采集也不缓存历史。
 * 协议见 docs/superpowers/specs/2026-07-21-二进制DMA架构-design.md 与 TelemFrame.h。
 *
 * 掩码变化（含上电）时先发一帧 SCHEMA（列名 + 单位），随后每个输出周期发一帧
 * SAMPLE（ms + 各选中通道 float32）。所有通道都是 float，网页按 schema 列序解析。
 */

#define TELEMETRY_DEFAULT_RATE_HZ    50U   /* 默认频率；DMA 后可显著高于旧 20Hz。 */
#define TELEMETRY_RATE_HARD_LIMIT_HZ 100U  /* 上限等于主循环频率。 */

/* 通道位定义（12 位）。位序即 SCHEMA/SAMPLE 的列序，一经发布不得重排。 */
#define TELEMETRY_CH_TL     0x0001U /* 左轮目标速度 mm/s */
#define TELEMETRY_CH_LV     0x0002U /* 左轮实测速度 mm/s */
#define TELEMETRY_CH_PL     0x0004U /* 左轮输出 PWM */
#define TELEMETRY_CH_TR     0x0008U /* 右轮目标速度 mm/s */
#define TELEMETRY_CH_RV     0x0010U /* 右轮实测速度 mm/s */
#define TELEMETRY_CH_PR     0x0020U /* 右轮输出 PWM */
#define TELEMETRY_CH_YAW    0x0040U /* 连续累计航向角 度 */
#define TELEMETRY_CH_NAVE   0x0080U /* 转向角误差 度 */
#define TELEMETRY_CH_LERR   0x0100U /* 巡线权重误差 */
#define TELEMETRY_CH_GRAY   0x0200U /* 五路灰度位图（转 float） */
#define TELEMETRY_CH_LD     0x0400U /* 左轮累计路程 mm */
#define TELEMETRY_CH_RD     0x0800U /* 右轮累计路程 mm */
#define TELEMETRY_CH_ALL    0x0FFFU

void Telemetry_Init(void);
void Telemetry_Update(uint8_t elapsedTicks, uint8_t pressedKeys);
uint8_t Telemetry_SetRateHz(uint8_t rateHz);
uint8_t Telemetry_SetFieldMask(uint16_t mask);
uint8_t Telemetry_GetRateHz(void);
uint16_t Telemetry_GetFieldMask(void);
uint8_t Telemetry_GetMaxRateHz(void);
/* 供捕获模块复用同一张通道表：按掩码把选中通道当前值写进 out，返回通道数。 */
uint8_t Telemetry_SampleChannels(uint16_t mask, float *out);
/* 供捕获 dump 复用：发一帧 SCHEMA（type 由调用方给，实时流与捕获类型不同）。 */
void Telemetry_SendSchema(uint16_t mask, uint8_t frameType);

#endif
