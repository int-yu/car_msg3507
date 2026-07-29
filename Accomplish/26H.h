#ifndef ACCOMPLISH_26H_H
#define ACCOMPLISH_26H_H

#include "Application/Core/App.h"
#include <stdint.h>

#define ACCOMPLISH_26H_START_STOP_KEY_MASK 0x01U

void Accomplish26H_Init(void);
void Accomplish26H_Update(const App_UpdateContext_t *context);
uint8_t Accomplish26H_IsTiming(void);
uint32_t Accomplish26H_GetElapsedTicks(void);

#endif
