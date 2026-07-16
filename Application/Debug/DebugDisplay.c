#include "Application/Debug/DebugDisplay.h"
#include "Application/Comms/K230Link.h"
#include "Application/State/Heading.h"
#include "Application/State/Odometry.h"
#include "Hardware/Board/Key.h"
#include "Hardware/Display/OLED.h"
#include "Hardware/Sensors/Graydetect.h"

static uint8_t s_refreshTicks;

static void DebugDisplay_ShowGrayState(uint8_t state)
{
    uint8_t index;

    OLED_ShowString(0, 8, "GRAY:", OLED_6X8);
    for (index = 0U; index < 5U; index++)
    {
        OLED_ShowChar((int16_t)(30 + index * OLED_6X8), 8,
                      ((state >> index) & 1U) != 0U ? '1' : '0',
                      OLED_6X8);
    }
}

static void DebugDisplay_ShowKeyState(uint8_t pressedMask)
{
    uint8_t index;

    OLED_ShowString(0, 16, "KEY:", OLED_6X8);
    for (index = 0U; index < 4U; index++)
    {
        OLED_ShowChar((int16_t)(24 + index * OLED_6X8), 16,
                      ((pressedMask >> index) & 1U) != 0U ? '1' : '0',
                      OLED_6X8);
    }
}

static void DebugDisplay_ShowMotionValue(int16_t y,
                                         const char *label,
                                         float value,
                                         const char *unit)
{
    OLED_ShowString(0, y, label, OLED_6X8);
    OLED_ShowFloatNum(18, y, value, 5U, 1U, OLED_6X8);
    OLED_ShowString(72, y, unit, OLED_6X8);
}

static void DebugDisplay_ShowK230Target(void)
{
    K230Link_Target_t target;

    if (K230Link_IsReady() == 0U)
    {
        OLED_ShowString(0, 56, "K230:WAIT", OLED_6X8);
        return;
    }
    if (K230Link_GetTarget(&target) == 0U)
    {
        OLED_ShowString(0, 56, "K230:READY", OLED_6X8);
        return;
    }

    OLED_ShowString(0, 56, "K:", OLED_6X8);
    OLED_ShowNum(12, 56, target.valid, 1U, OLED_6X8);
    OLED_ShowString(18, 56, " X:", OLED_6X8);
    OLED_ShowSignedNum(36, 56, target.offsetX, 4U, OLED_6X8);
    OLED_ShowString(66, 56, " Y:", OLED_6X8);
    OLED_ShowSignedNum(84, 56, target.offsetY, 4U, OLED_6X8);
}

void DebugDisplay_Init(void)
{
    s_refreshTicks = DEBUG_DISPLAY_REFRESH_TICKS;
    OLED_Init();
}

void DebugDisplay_ShowHeadingCalibration(uint8_t mpuReady)
{
    OLED_Clear();
    OLED_ShowString(0, 0, "MPU6050 Z ANGLE", OLED_6X8);

    if (mpuReady != 0U)
    {
        OLED_ShowString(0, 16, "ZERO CALIBRATING...", OLED_6X8);
        OLED_ShowString(0, 32, "KEEP CAR STILL", OLED_6X8);
    }
    else
    {
        OLED_ShowString(0, 16, "MPU6050 OFFLINE", OLED_6X8);
        OLED_ShowString(0, 32, "CHECK PA10/PA11", OLED_6X8);
    }

    OLED_Update();
}

void DebugDisplay_Update(uint8_t elapsedTicks)
{
    uint8_t grayState;
    uint8_t keyMask;

    if ((uint16_t)s_refreshTicks + elapsedTicks <
        DEBUG_DISPLAY_REFRESH_TICKS)
    {
        s_refreshTicks = (uint8_t)(s_refreshTicks + elapsedTicks);
        return;
    }
    s_refreshTicks = 0U;

    grayState = Graydetect_GetState();
    keyMask = Key_GetPressedMask();

    OLED_Clear();
    OLED_ShowString(0, 0, "Z:", OLED_6X8);
    if (Heading_IsReady() != 0U)
    {
        OLED_ShowFloatNum(12, 0, Heading_GetYaw(), 6U, 1U, OLED_6X8);
        OLED_ShowString(72, 0, "deg", OLED_6X8);
    }
    else
    {
        OLED_ShowString(12, 0, "OFFLINE", OLED_6X8);
    }

    DebugDisplay_ShowGrayState(grayState);
    DebugDisplay_ShowKeyState(keyMask);
    DebugDisplay_ShowMotionValue(24, "LD:", Odometry_GetDistanceLMM(), "mm");
    DebugDisplay_ShowMotionValue(32, "LV:", Odometry_GetSpeedL(), "mm/s");
    DebugDisplay_ShowMotionValue(40, "RD:", Odometry_GetDistanceRMM(), "mm");
    DebugDisplay_ShowMotionValue(48, "RV:", Odometry_GetSpeedR(), "mm/s");

    DebugDisplay_ShowK230Target();
    OLED_Update();
}
