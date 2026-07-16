#ifndef __KEY_H
#define __KEY_H

/* 非阻塞板级按键 GPIO 驱动；按下时读取为低电平。 */

#include <stdint.h>

void Key_Init(void);
uint8_t Key_GetPressedMask(void);
uint8_t Key_GetNum(void);

#endif
