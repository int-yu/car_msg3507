#include "Application/Debug/DebugDisplay.h"
#include "Application/Control/TaskTimer.h"
#include "Hardware/Display/OLED.h"

#include <limits.h>

#define DEBUG_DISPLAY_TIME_X            0
#define DEBUG_DISPLAY_TIME_Y           24
#define DEBUG_DISPLAY_TIME_WIDTH        56U
#define DEBUG_DISPLAY_TIME_HEIGHT       16U
#define DEBUG_DISPLAY_TICKS_PER_TENTH   10U

static uint8_t s_running;
static uint32_t s_elapsedTenths;

static void DebugDisplay_DrawTime(void)
{
    uint32_t wholeSeconds = s_elapsedTenths / 10U;
    uint32_t fraction = s_elapsedTenths % 10U;

    OLED_ClearArea(DEBUG_DISPLAY_TIME_X, DEBUG_DISPLAY_TIME_Y,
                   DEBUG_DISPLAY_TIME_WIDTH, DEBUG_DISPLAY_TIME_HEIGHT);
    OLED_ShowNum(DEBUG_DISPLAY_TIME_X, DEBUG_DISPLAY_TIME_Y,
                 wholeSeconds, 3U, OLED_8X16);
    OLED_ShowString(24, DEBUG_DISPLAY_TIME_Y, ".", OLED_8X16);
    OLED_ShowNum(32, DEBUG_DISPLAY_TIME_Y, fraction, 1U, OLED_8X16);
    OLED_ShowString(40, DEBUG_DISPLAY_TIME_Y, "s", OLED_8X16);
}

void DebugDisplay_Init(void)
{
    s_running = 0U;
    s_elapsedTenths = UINT32_MAX;

    OLED_Init();
    DebugDisplay_ShowMenu(2U);
}

void DebugDisplay_ShowMenu(uint8_t selectedRequirement)
{
    uint8_t requirement;

    s_running = 0U;
    OLED_Clear();
    for (requirement = 2U; requirement <= 6U; requirement++)
    {
        int16_t y = (int16_t)((requirement - 2U) * 12U);
        OLED_ShowString(8, y, "REQUIRE ", OLED_6X8);
        OLED_ShowNum(56, y, requirement, 1U, OLED_6X8);
        if (requirement == selectedRequirement)
        {
            OLED_ShowString(0, y, ">", OLED_6X8);
            OLED_ReverseArea(0, y, 68U, 8U);
        }
    }
    OLED_Update();
}

void DebugDisplay_ShowPrompt(uint8_t requirement, const char *message)
{
    s_running = 0U;
    OLED_Clear();
    OLED_ShowString(0, 0, "REQ ", OLED_8X16);
    OLED_ShowNum(32, 0, requirement, 1U, OLED_8X16);
    if (message != 0)
    {
        OLED_ShowString(0, 28, message, OLED_8X16);
    }
    OLED_Update();
}

void DebugDisplay_ShowRunning(uint8_t requirement)
{
    OLED_Clear();
    OLED_ShowString(0, 0, "REQ ", OLED_8X16);
    OLED_ShowNum(32, 0, requirement, 1U, OLED_8X16);
    OLED_ShowString(48, 0, "RUN", OLED_8X16);
    s_elapsedTenths = TaskTimer_GetElapsedTicks() /
        DEBUG_DISPLAY_TICKS_PER_TENTH;
    DebugDisplay_DrawTime();
    s_running = 1U;
    OLED_Update();
}

void DebugDisplay_Update(void)
{
    uint32_t elapsedTenths;

    if (s_running == 0U)
    {
        return;
    }

    elapsedTenths = TaskTimer_GetElapsedTicks() /
        DEBUG_DISPLAY_TICKS_PER_TENTH;
    if (elapsedTenths == s_elapsedTenths)
    {
        return;
    }

    s_elapsedTenths = elapsedTenths;
    DebugDisplay_DrawTime();
    OLED_UpdateArea(DEBUG_DISPLAY_TIME_X, DEBUG_DISPLAY_TIME_Y,
                    DEBUG_DISPLAY_TIME_WIDTH, DEBUG_DISPLAY_TIME_HEIGHT);
}
