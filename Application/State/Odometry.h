#ifndef __ODOMETRY_H
#define __ODOMETRY_H

/* 基于编码器增量的车轮里程状态，由 100 Hz 系统节拍更新。 */

#include <stdint.h>

/* 里程计 —— 基于左右编码器增量积分前进距离
 * 用法：每个 100 Hz 节拍调用一次 Odometry_Update(ticks)；
 * 每段起点调用 Odometry_Reset()。编码器计数到毫米的换算需现场标定。
 */
void  Odometry_Init(void);            /* 含 Encoder_Init */
void  Odometry_Update(uint8_t ticks); /* 读取编码器增量，并更新路程和速度 */
void  Odometry_Reset(void);           /* 在当前分段起点清零里程 */

float Odometry_GetDistanceMM(void); /* 返回自上次复位起的左右平均路程，单位 mm */
float Odometry_GetDistanceLMM(void);
float Odometry_GetDistanceRMM(void);
float Odometry_GetSpeedL(void);     /* 左轮速度(mm/s) */
float Odometry_GetSpeedR(void);     /* 右轮速度(mm/s) */

extern float Odometry_CountsPerMM;  /* 标定常量：每毫米对应编码器计数，现场标定 */

#endif
