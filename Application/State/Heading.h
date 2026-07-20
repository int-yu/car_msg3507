#ifndef HEADING_H
#define HEADING_H

#include <stdint.h>

#define HEADING_CALIBRATION_SAMPLES      400U
#define HEADING_CALIBRATION_INTERVAL_MS  2U

/* 航向角 —— 对 MPU6050 的 Z 轴角速度积分得到连续累计偏航角（度）。
 * 角度不做 ±180° 归一化，可直接表示多圈旋转。
 * 漂移处理：开机静止校准零偏；检测到外部地标时可用 Heading_SetYaw() 重置基准。
 * 尺度修正：标称灵敏度 32.8 LSB/(°/s) 未必完全匹配当前芯片，
 * 可通过原地旋转 N 圈标定 s_scale。
 * 用法：开机调用 Heading_Init() 和 Heading_Calibrate()，
 * 在 100 Hz 节拍中调用 Heading_Update(dt)。
 */
void Heading_Init(void);        /* 初始化 MPU6050，并复位角度、零偏和尺度因子 */
void Heading_Calibrate(void);   /* 静止采样取零漂均值，调用期间会阻塞 */
void Heading_Update(float dt);  /* 按 dt 积分偏航角；尺度标定时同步累计原始角 */
uint8_t Heading_IsReady(void);  /* MPU6050 是否在线；读取失败后返回 0 */

float Heading_GetYaw(void);      /* 累计偏航角（度），可超出 ±180° */
void  Heading_SetYaw(float yaw); /* 设置当前偏航角基准，可用于地标校正 */

/* 陀螺仪尺度标定：原地旋转 N 圈，真实角度为 N×360°，不依赖轴距或轮径。
 * 用法：对准地标后开始标定，原地旋转 N 整圈并回到地标，再结束标定。
 * 尺度因子 = 真实角度 / 标定期间按标称灵敏度积分的角度绝对值。 */
void  Heading_ScaleCalibStart(void);            /* 清零标定角并开始累计 */
float Heading_ScaleCalibFinish(uint16_t turns); /* 解算并应用尺度因子，返回新值 */
void  Heading_ScaleCalibCancel(void);           /* 中止标定，不改变尺度因子 */
float Heading_GetCalibAngle(void);              /* 返回标定期间累计的原始角度 */
uint8_t Heading_IsScaleCalibActive(void);       /* 是否正在尺度标定 */

float Heading_GetScale(void);                   /* 返回当前尺度修正因子，默认 1.0 */
void  Heading_SetScale(float scale);            /* 设置尺度因子，用于掉电恢复或手动标定 */

#endif
