#ifndef TICK_H
#define TICK_H

#include <stdint.h>

#define TICK_HZ 100U
#define TICK_DT 0.01f

void Tick_Init(void);
uint8_t Tick_Poll(void);
uint8_t Tick_PollCount(void);

#endif
