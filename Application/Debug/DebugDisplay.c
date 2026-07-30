#include "Application/Debug/DebugDisplay.h"
#include "Application/Control/BallSensor.h"
#include "Hardware/Display/OLED.h"

#define DEBUG_DISPLAY_BALL_LABEL_X    0
#define DEBUG_DISPLAY_BALL_VALUE_X    30
#define DEBUG_DISPLAY_BALL_Y          0
#define DEBUG_DISPLAY_BALL_VALUE_WIDTH 66U

static uint8_t s_refreshTicks;

static void DebugDisplay_DrawBallValue(void)
{
    OLED_ClearArea(DEBUG_DISPLAY_BALL_VALUE_X,
                   DEBUG_DISPLAY_BALL_Y,
                   DEBUG_DISPLAY_BALL_VALUE_WIDTH,
                   8U);

    if (BallSensor_IsFresh() != 0U)
    {
        OLED_ShowFloatNum(DEBUG_DISPLAY_BALL_VALUE_X,
                          DEBUG_DISPLAY_BALL_Y,
                          BallSensor_GetPositionMM(),
                          3U,
                          1U,
                          OLED_6X8);
        OLED_ShowString(72, DEBUG_DISPLAY_BALL_Y, "mm", OLED_6X8);
    }
    else
    {
        OLED_ShowString(DEBUG_DISPLAY_BALL_VALUE_X,
                        DEBUG_DISPLAY_BALL_Y,
                        "WAIT",
                        OLED_6X8);
    }
}

void DebugDisplay_Init(void)
{
    s_refreshTicks = 0U;
    OLED_Init();
    OLED_Clear();
    OLED_ShowString(DEBUG_DISPLAY_BALL_LABEL_X,
                    DEBUG_DISPLAY_BALL_Y,
                    "BALL:",
                    OLED_6X8);
    DebugDisplay_DrawBallValue();
    OLED_UpdateArea(0, DEBUG_DISPLAY_BALL_Y, 96U, 8U);
}

void DebugDisplay_Update(uint8_t elapsedTicks)
{
    if ((uint16_t)s_refreshTicks + elapsedTicks <
        DEBUG_DISPLAY_REFRESH_TICKS)
    {
        s_refreshTicks = (uint8_t)(s_refreshTicks + elapsedTicks);
        return;
    }
    s_refreshTicks = 0U;

    DebugDisplay_DrawBallValue();
    OLED_UpdateArea(DEBUG_DISPLAY_BALL_VALUE_X,
                    DEBUG_DISPLAY_BALL_Y,
                    DEBUG_DISPLAY_BALL_VALUE_WIDTH,
                    8U);
}
