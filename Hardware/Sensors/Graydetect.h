#ifndef __GRAYDETECT_H
#define __GRAYDETECT_H

/*
 * 六路红外学习巡线模块的 I2C 采样与基础巡线偏差计算。
 *
 * 文件名沿用 Graydetect，只为保留现有 MotionLine、Mission、Telemetry 的调用
 * 接口；底层已不再读取旧的八路 GPIO 灰度输入。
 */

#include <stdint.h>

/* 教程协议：固定地址 0x5C，寄存器 0x05 返回 bit0=CH1 ... bit5=CH6。 */
#define GRAY_CHANNEL_COUNT          6U
#define GRAYDETECT_I2C_ADDRESS      0x5CU
#define GRAYDETECT_STATE_REGISTER   0x05U
#define GRAYDETECT_OFFLINE_CONFIRM_FAILURES 3U

/* 教程俯视图中 CH1 在右、CH6 在左。若实际安装方向反了，只改为 0。 */
#define GRAYDETECT_CHANNEL1_IS_RIGHT 1U

#define GRAY_SIDE_ALL    0
#define GRAY_SIDE_LEFT   1
#define GRAY_SIDE_RIGHT  2

void    Graydetect_Init(void);
void    Graydetect_Update(void);              /* 每个 App 有效节拍最多调用一次。 */
uint8_t Graydetect_IsOnline(void);            /* 最近一次 I2C 读状态是否 ACK 成功。 */
uint32_t Graydetect_GetReadErrorCount(void);  /* 便于现场排查 SDA/SCL/供电。 */
uint8_t Graydetect_GetState(void);            /* bit0=CH1 ... bit5=CH6；1=识别到目标色。 */
uint8_t Graydetect_GetBit(uint8_t index);     /* index 为 0~5，对应 CH1~CH6。 */

/* 加权位置误差：中心为 0，线偏左为负、偏右为正；side 选择参与的物理半区。 */
float   Graydetect_GetError(uint8_t side);
uint8_t Graydetect_OnLine(uint8_t side);      /* 所选区域任一路检测到黑线时返回 1 */

#endif
