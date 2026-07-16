#ifndef __BEEP_H
#define __BEEP_H

/* 蜂鸣器与 LED 提示驱动；每个 100 Hz 系统节拍调用一次 Beep_Tick。 */

#include <stdint.h>

/* 声光提示 —— PB17 低电平驱动蜂鸣器，并同步控制 LED2。
 * 非阻塞：Beep_Notify() 触发后由 Beep_Tick()(100 Hz)推进，不阻塞控制循环。
 */
void Beep_Init(void);
void Beep_On(void);
void Beep_Off(void);

void Beep_Notify(uint8_t times); /* 触发 times 次短鸣并同步闪烁 LED2 */
void Beep_Long(void);            /* 停车长鸣 */
void Beep_Tick(void);            /* 100Hz 节拍调用，推进鸣叫状态机 */

#endif
