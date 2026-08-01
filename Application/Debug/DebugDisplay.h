#ifndef APPLICATION_DEBUG_DEBUG_DISPLAY_H
#define APPLICATION_DEBUG_DEBUG_DISPLAY_H

#include <stdint.h>

void DebugDisplay_Init(void);
void DebugDisplay_ShowMenu(uint8_t selectedRequirement);
void DebugDisplay_ShowPrompt(uint8_t requirement, const char *message);
void DebugDisplay_ShowRunning(uint8_t requirement);
void DebugDisplay_Update(void);

#endif
