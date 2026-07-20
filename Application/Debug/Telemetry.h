#ifndef APPLICATION_DEBUG_TELEMETRY_H
#define APPLICATION_DEBUG_TELEMETRY_H

#include <stdint.h>

/* CSV 遥测输出。数据全部通过各模块公共接口读取，本模块不采集也不缓存历史。 */

#define TELEMETRY_DEFAULT_RATE_HZ        20U  /* 默认频率；115200 下约占 13% 带宽。 */

/* 频率硬上限，等于主循环频率。实际可用上限还要按字段掩码算，见 Telemetry_GetMaxRateHz()。 */
#define TELEMETRY_RATE_HARD_LIMIT_HZ       100U

/* 字段掩码位定义（16 位）。改变掩码会立即重发一行表头。 */
#define TELEMETRY_FIELD_YAW      0x01U   /* 连续累计航向角。 */
#define TELEMETRY_FIELD_SENSOR   0x02U   /* 五路灰度位图与按键位图。 */
#define TELEMETRY_FIELD_DISTANCE 0x04U   /* 左右轮累计路程 mm。 */
#define TELEMETRY_FIELD_SPEED    0x08U   /* 左右轮实测速度 mm/s。 */
#define TELEMETRY_FIELD_MODE     0x10U   /* 运动模式文本。 */
#define TELEMETRY_FIELD_K230     0x20U   /* 最近一次 K230 TARGET。 */
#define TELEMETRY_FIELD_TARGET   0x40U   /* TL,TR 本拍目标轮速 mm/s。 */
#define TELEMETRY_FIELD_PWM      0x80U   /* PL,PR 双轮最终输出 PWM。 */
#define TELEMETRY_FIELD_NAV      0x100U  /* navT,navE 转向目标角与角误差。 */
#define TELEMETRY_FIELD_LINE     0x200U  /* lerr 巡线离散权重误差。 */

/* 单侧字段：调单个轮子时只传该侧，行长减半、可用频率翻倍。
 * 与上方成对字段并存——成对位保持原语义不变，避免既有掩码失效；
 * 同时置位成对与单侧时，同一列会输出两次，由上位机自行避免。 */
#define TELEMETRY_FIELD_SPEED_L  0x400U  /* 只输出 LV。 */
#define TELEMETRY_FIELD_SPEED_R  0x800U  /* 只输出 RV。 */
#define TELEMETRY_FIELD_TARGET_L 0x1000U /* 只输出 TL。 */
#define TELEMETRY_FIELD_TARGET_R 0x2000U /* 只输出 TR。 */
#define TELEMETRY_FIELD_PWM_L    0x4000U /* 只输出 PL。 */
#define TELEMETRY_FIELD_PWM_R    0x8000U /* 只输出 PR。 */
#define TELEMETRY_FIELD_ALL      0xFFFFU

void Telemetry_Init(void);
void Telemetry_Update(uint8_t elapsedTicks, uint8_t pressedKeys);
uint8_t Telemetry_SetRateHz(uint8_t rateHz);
uint8_t Telemetry_SetFieldMask(uint16_t mask);
uint8_t Telemetry_GetRateHz(void);
uint16_t Telemetry_GetFieldMask(void);
uint8_t Telemetry_GetMaxRateHz(void);

#endif
