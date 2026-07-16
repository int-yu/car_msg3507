#ifndef __ENCODER_H
#define __ENCODER_H

/* 软件正交解码器：GPIO 中断记录计数，主循环读取增量。 */

#include <stdint.h>

void Encoder_Init(void);
int16_t Encoder_Get(uint8_t n);

#endif
