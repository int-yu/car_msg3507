#include "Application/Debug/DebugDisplay.h"
#include "Hardware/Display/OLED.h"

#define DEBUG_DISPLAY_TIME_LABEL_X     0
#define DEBUG_DISPLAY_TIME_VALUE_X     12
#define DEBUG_DISPLAY_TIME_Y           0
#define DEBUG_DISPLAY_TIME_VALUE_WIDTH 54U
#define DEBUG_DISPLAY_TICKS_PER_SECOND 100U

static uint8_t s_refreshTicks;
static uint8_t s_secondTicks;
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
    s_refreshTicks = 0U;
    s_secondTicks = 0U;
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

void DebugDisplay_Update(uint8_t elapsedTicks)
{
    uint16_t ticks;

    if (elapsedTicks == 0U)
    {
        return;
    }

    ticks = (uint16_t)s_secondTicks + elapsedTicks;
    while (ticks >= DEBUG_DISPLAY_TICKS_PER_SECOND)
    {
        ticks = (uint16_t)(ticks - DEBUG_DISPLAY_TICKS_PER_SECOND);
        s_elapsedSeconds++;
    }
    s_secondTicks = (uint8_t)ticks;

    if ((uint16_t)s_refreshTicks + elapsedTicks <
        DEBUG_DISPLAY_REFRESH_TICKS)
    {
        s_refreshTicks = (uint8_t)(s_refreshTicks + elapsedTicks);
        return;
    }
    s_refreshTicks = 0U;

    DebugDisplay_DrawTime();
    OLED_UpdateArea(DEBUG_DISPLAY_TIME_VALUE_X,
                    DEBUG_DISPLAY_TIME_Y,
                    DEBUG_DISPLAY_TIME_VALUE_WIDTH,
                    8U);
}
