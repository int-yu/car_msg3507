#ifndef __GRAYDETECT_H
#define __GRAYDETECT_H

/* 五路灰度传感器驱动和基础巡线偏差计算。 */

#include <stdint.h>

/* 五路灰度用于巡线和黑线到点判断。
 * 位序约定：bit0 为最左侧，bit4 为最右侧，检测到黑线时对应位为 1。
 * 左右半区可分别用于交叉点判断，中心通道由两侧共享。
 */
#define GRAY_SIDE_ALL    0
#define GRAY_SIDE_LEFT   1
#define GRAY_SIDE_RIGHT  2

void    Graydetect_Init(void);
uint8_t Graydetect_GetState(void);            /* 返回五路状态位图，黑线对应位为 1 */
uint8_t Graydetect_GetBit(uint8_t index);     /* 返回第 index 路状态，index 范围为 0~4 */

/* 加权位置误差：中心为 0，线偏左为负，偏右为正。
 * 左半区使用 bit0~bit2，右半区使用 bit2~bit4，中心 bit2 由两侧共享。
 * side 选择全部、左半区或右半区参与加权。 */
float   Graydetect_GetError(uint8_t side);
uint8_t Graydetect_OnLine(uint8_t side);      /* 所选区域任一路检测到黑线时返回 1 */

#endif
