#ifndef __LED_H
#define __LED_H

/* 板级 LED GPIO 驱动，硬件电平逻辑只在本模块维护。 */

void LED_Init(void);
void LED1_ON(void);
void LED1_OFF(void);
void LED1_Turn(void);
void LED2_ON(void);
void LED2_OFF(void);
void LED2_Turn(void);
void LED_RGB_ON(void);
void LED_RGB_OFF(void);

#endif
