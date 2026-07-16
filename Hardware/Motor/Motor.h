#ifndef __MOTOR_H
#define __MOTOR_H

/* 基于 PWM 和 GPIO 方向控制的电机驱动；本模块不包含导航逻辑。 */

#include <stdint.h>

void Motor_Init(void);
void Motor_SetLeftPWM(int16_t PWM);
void Motor_SetRightPWM(int16_t PWM);
void Motor_SetPWM(int16_t LeftPWM, int16_t RightPWM);
void Motor_StopAll(void);
void Motor_Brake(void);     /* 主动刹车(短接绕组急停)，到点停车用以减少冲过线 */

void Motor_Run(int16_t leftSpeed, int16_t rightSpeed);
void Motor_Forward(int16_t speed);
void Motor_Backward(int16_t speed);
void Motor_TurnLeft(int16_t speed);
void Motor_TurnRight(int16_t speed);
void Motor_SpinLeft(int16_t speed);
void Motor_SpinRight(int16_t speed);
void Motor_Stop(void);

#endif
