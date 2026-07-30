#ifndef APPLICATION_SERVO_SERVO_H
#define APPLICATION_SERVO_SERVO_H

#include <stdint.h>

/*
 * 新板将 PB8/PB9 用于步进 ST/DIR，本阶段停用舵机 PWM。角度 API 继续保留，
 * 以免上层调试命令和测试入口失去链接；禁用时只记录角度，不访问硬件。
 */
#ifndef SERVO_ENABLED
#define SERVO_ENABLED 0
#endif

#define SERVO_PHYSICAL_RANGE_DEG          270U
#define SERVO_MIN_PULSE_US                500U
#define SERVO_MAX_PULSE_US                2500U
#define SERVO_FRAME_US                    20000U

/* 两个轴分别定义限位，便于按实测机械行程独立调整。 */
#define SERVO_VERTICAL_MIN_ANGLE          0U
#define SERVO_VERTICAL_MAX_ANGLE          270U
#define SERVO_VERTICAL_DEFAULT_ANGLE      135U

#define SERVO_HORIZONTAL_MIN_ANGLE        0U
#define SERVO_HORIZONTAL_MAX_ANGLE        270U
#define SERVO_HORIZONTAL_DEFAULT_ANGLE    135U

void Servo_Init(void);
void Servo_SetVerticalAngle(uint16_t angle);
void Servo_SetHorizontalAngle(uint16_t angle);
uint16_t Servo_GetVerticalAngle(void);
uint16_t Servo_GetHorizontalAngle(void);
void Servo_Reset(void);

#endif
