#ifndef APPLICATION_DEBUG_DEBUG_DISPLAY_H
#define APPLICATION_DEBUG_DEBUG_DISPLAY_H

#include <stdint.h>

#define DEBUG_DISPLAY_REFRESH_TICKS 10U

void DebugDisplay_Init(void);
void DebugDisplay_Update(uint8_t elapsedTicks);

#endif
