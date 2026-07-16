#include "Hardware/Motor/Motor.h"
#include "Hardware/Motor/PWM.h"
#include "ti_msp_dl_config.h"

/* 实测: 物理左电机接在驱动器 B 通道、物理右电机接在 A 通道(接线左右反了)。
 * 故左轮走 B 通道(前进符号 +1)、右轮走 A 通道(前进符号 -1)。
 * 交叉的后果: 左环算的 PWM 驱动到右电机 → 巡线/航向差速全反 → 发 s 冲出去/打转。
 * 此处把左右各自绑定到正确的物理通道(与 ti_24_h 一致)。 */
#define LEFT_MOTOR_DIR_SIGN  (+1)   /* 左电机 = B 通道 */
#define RIGHT_MOTOR_DIR_SIGN (-1)   /* 右电机 = A 通道 */

static int16_t Motor_ClampPWM(int16_t pwm)
{
    if (pwm > (int16_t)PWM_MAX_DUTY) return (int16_t)PWM_MAX_DUTY;
    if (pwm < -(int16_t)PWM_MAX_DUTY) return -(int16_t)PWM_MAX_DUTY;
    return pwm;
}

static void Motor_WritePin(GPIO_Regs *port, uint32_t pin, uint8_t high)
{
    if (high != 0U) DL_GPIO_setPins(port, pin);
    else DL_GPIO_clearPins(port, pin);
}

void Motor_Init(void)
{
    PWM_Init();
    Motor_StopAll();
}

void Motor_SetLeftPWM(int16_t pwm)   /* 物理左电机 = 驱动器 B 通道(BIN/CompareB) */
{
    int16_t value = Motor_ClampPWM((int16_t)(pwm * LEFT_MOTOR_DIR_SIGN));
    Motor_WritePin(BOARD_OUTPUTS_MOTOR_BIN1_PORT, BOARD_OUTPUTS_MOTOR_BIN1_PIN, value >= 0);
    Motor_WritePin(BOARD_OUTPUTS_MOTOR_BIN2_PORT, BOARD_OUTPUTS_MOTOR_BIN2_PIN, value < 0);
    PWM_SetCompareB((uint16_t)((value < 0) ? -value : value));
}

void Motor_SetRightPWM(int16_t pwm)  /* 物理右电机 = 驱动器 A 通道(AIN/CompareA) */
{
    int16_t value = Motor_ClampPWM((int16_t)(pwm * RIGHT_MOTOR_DIR_SIGN));
    Motor_WritePin(BOARD_OUTPUTS_MOTOR_AIN1_PORT, BOARD_OUTPUTS_MOTOR_AIN1_PIN, value >= 0);
    Motor_WritePin(BOARD_OUTPUTS_MOTOR_AIN2_PORT, BOARD_OUTPUTS_MOTOR_AIN2_PIN, value < 0);
    PWM_SetCompareA((uint16_t)((value < 0) ? -value : value));
}

void Motor_SetPWM(int16_t leftPWM, int16_t rightPWM)
{
    Motor_SetLeftPWM(leftPWM);
    Motor_SetRightPWM(rightPWM);
}

void Motor_StopAll(void)
{
    PWM_SetCompareA(0U);
    PWM_SetCompareB(0U);
    DL_GPIO_clearPins(GPIOA, BOARD_OUTPUTS_MOTOR_AIN1_PIN | BOARD_OUTPUTS_MOTOR_AIN2_PIN);
    DL_GPIO_clearPins(GPIOB, BOARD_OUTPUTS_MOTOR_BIN1_PIN | BOARD_OUTPUTS_MOTOR_BIN2_PIN);
}

void Motor_Brake(void)
{
    DL_GPIO_setPins(GPIOA, BOARD_OUTPUTS_MOTOR_AIN1_PIN | BOARD_OUTPUTS_MOTOR_AIN2_PIN);
    DL_GPIO_setPins(GPIOB, BOARD_OUTPUTS_MOTOR_BIN1_PIN | BOARD_OUTPUTS_MOTOR_BIN2_PIN);
    PWM_SetCompareA(PWM_MAX_DUTY);
    PWM_SetCompareB(PWM_MAX_DUTY);
}

void Motor_Run(int16_t leftSpeed, int16_t rightSpeed) { Motor_SetPWM(leftSpeed, rightSpeed); }
void Motor_Forward(int16_t speed) { Motor_SetPWM(speed, speed); }
void Motor_Backward(int16_t speed) { Motor_SetPWM(-speed, -speed); }
void Motor_TurnLeft(int16_t speed) { Motor_SetPWM(0, speed); }
void Motor_TurnRight(int16_t speed) { Motor_SetPWM(speed, 0); }
void Motor_SpinLeft(int16_t speed) { Motor_SetPWM(-speed, speed); }
void Motor_SpinRight(int16_t speed) { Motor_SetPWM(speed, -speed); }
void Motor_Stop(void) { Motor_StopAll(); }
