#ifndef __GRAYDETECT_H
#define __GRAYDETECT_H

/* 灰度传感器驱动和基础巡线偏差计算。 */

#include <stdint.h>
#include "Application/Core/CarRole.h"

/*
 * 主车使用 8 路灰度，从车保留 5 路灰度；两车共用包含 CH0~CH7 的
 * SysConfig。通道按从左到右排列，检测到黑线时对应位为 1。
 */
#if CAR_IS_MASTER
#define GRAY_CHANNEL_COUNT 8U
#else
#define GRAY_CHANNEL_COUNT 5U
#endif

#define GRAY_SIDE_ALL    0
#define GRAY_SIDE_LEFT   1
#define GRAY_SIDE_RIGHT  2

void    Graydetect_Init(void);
uint8_t Graydetect_GetState(void);            /* 返回有效通道状态位图 */
uint8_t Graydetect_GetBit(uint8_t index);     /* 返回指定通道状态 */

/* 加权位置误差：中心为 0，线偏左为负，偏右为正。
 * side 选择全部、左半区或右半区参与加权。 */
float   Graydetect_GetError(uint8_t side);
uint8_t Graydetect_OnLine(uint8_t side);      /* 所选区域任一路检测到黑线时返回 1 */

#endif
