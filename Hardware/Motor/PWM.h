#ifndef __PWM_H
#define __PWM_H

/* 电机双通道底层 PWM 定时器驱动。 */

#include <stdint.h>

#define PWM_MAX_DUTY    1000U

void PWM_Init(void);
void PWM_SetCompareA(uint16_t Compare);
void PWM_SetCompareB(uint16_t Compare);

#endif
