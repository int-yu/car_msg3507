#include "Application/Debug/DebugDisplay.h"
#include "Application/Control/TaskTimer.h"
#include "Hardware/Display/OLED.h"

#define DEBUG_DISPLAY_TIME_LABEL_X     0
#define DEBUG_DISPLAY_TIME_VALUE_X     12
#define DEBUG_DISPLAY_TIME_Y           0
#define DEBUG_DISPLAY_TIME_VALUE_WIDTH 54U
#define DEBUG_DISPLAY_TICKS_PER_SECOND 100U

static uint32_t s_elapsedSeconds;

static void DebugDisplay_DrawTime(void)
{
    OLED_ClearArea(DEBUG_DISPLAY_TIME_VALUE_X,
                   DEBUG_DISPLAY_TIME_Y,
                   DEBUG_DISPLAY_TIME_VALUE_WIDTH,
                   8U);
    OLED_ShowNum(DEBUG_DISPLAY_TIME_VALUE_X,
                 DEBUG_DISPLAY_TIME_Y,
                 s_elapsedSeconds,
                 5U,
                 OLED_6X8);
    OLED_ShowString(48, DEBUG_DISPLAY_TIME_Y, "s", OLED_6X8);
}

void DebugDisplay_Init(void)
{
    s_elapsedSeconds = 0U;

    OLED_Init();
    OLED_Clear();
    OLED_ShowString(DEBUG_DISPLAY_TIME_LABEL_X,
                    DEBUG_DISPLAY_TIME_Y,
                    "T:",
                    OLED_6X8);
    DebugDisplay_DrawTime();
    OLED_UpdateArea(0, DEBUG_DISPLAY_TIME_Y, 54U, 8U);
}

void DebugDisplay_Update(void)
{
    uint32_t elapsedSeconds =
        TaskTimer_GetElapsedTicks() / DEBUG_DISPLAY_TICKS_PER_SECOND;

    if (elapsedSeconds == s_elapsedSeconds)
    {
        return;
    }

    s_elapsedSeconds = elapsedSeconds;
    DebugDisplay_DrawTime();
    OLED_UpdateArea(DEBUG_DISPLAY_TIME_VALUE_X,
                    DEBUG_DISPLAY_TIME_Y,
                    DEBUG_DISPLAY_TIME_VALUE_WIDTH,
                    8U);
}
