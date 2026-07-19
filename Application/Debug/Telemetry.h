#ifndef APPLICATION_DEBUG_TELEMETRY_H
#define APPLICATION_DEBUG_TELEMETRY_H

#include <stdint.h>

/* CSV 遥测输出。数据全部通过各模块公共接口读取，本模块不采集也不缓存历史。 */

#define TELEMETRY_DEFAULT_RATE_HZ    20U  /* 默认频率；115200 下约占 13% 带宽。 */
#define TELEMETRY_MAX_RATE_HZ       100U  /* 上限等于主循环频率；约占 65% 带宽。 */

/* 字段掩码位定义。改变掩码会立即重发一行表头。 */
#define TELEMETRY_FIELD_YAW      0x01U  /* 连续累计航向角。 */
#define TELEMETRY_FIELD_SENSOR   0x02U  /* 五路灰度位图与按键位图。 */
#define TELEMETRY_FIELD_DISTANCE 0x04U  /* 左右轮累计路程 mm。 */
#define TELEMETRY_FIELD_SPEED    0x08U  /* 左右轮实测速度 mm/s。 */
#define TELEMETRY_FIELD_MODE     0x10U  /* 运动模式文本。 */
#define TELEMETRY_FIELD_K230     0x20U  /* 最近一次 K230 TARGET。 */
#define TELEMETRY_FIELD_ALL      0x3FU

void Telemetry_Init(void);
void Telemetry_Update(uint8_t elapsedTicks, uint8_t pressedKeys);
uint8_t Telemetry_SetRateHz(uint8_t rateHz);
uint8_t Telemetry_SetFieldMask(uint8_t mask);
uint8_t Telemetry_GetRateHz(void);
uint8_t Telemetry_GetFieldMask(void);

#endif
