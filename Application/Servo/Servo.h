#ifndef APPLICATION_SERVO_SERVO_H
#define APPLICATION_SERVO_SERVO_H

#include <stdint.h>

/*
 * TIMA0 舵机 PWM 映射：
 *   PB8 / CCP0 -> 横向舵机
 *   PB9 / CCP1 -> 纵向舵机
 */
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
