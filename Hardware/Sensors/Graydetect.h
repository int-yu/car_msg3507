#ifndef __GRAYDETECT_H
#define __GRAYDETECT_H

/* 灰度硬件当前停用；接口保留用于兼容完整应用源码。 */

#include <stdint.h>
#include "Application/Core/CarRole.h"

#ifndef GRAYDETECT_ENABLED
#define GRAYDETECT_ENABLED 0U
#endif

/*
 * 启用灰度前必须恢复GRAY_INPUTS SysConfig引脚组，并把
 * GRAYDETECT_ENABLED改为1。停用时所有读取接口返回0。
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
