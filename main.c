#include "ti_msp_dl_config.h"

#include "Application/Comms/BluetoothDebug.h"
#include "Application/Control/Drive.h"
#include "Application/Debug/DebugDisplay.h"
#include "Application/Servo/Servo.h"
#include "Application/State/Heading.h"
#include "Application/State/Odometry.h"
#include "Hardware/Board/Beep.h"
#include "Hardware/Board/Key.h"
#include "Hardware/Board/LED.h"
#include "Hardware/Comms/Serial.h"
#include "Hardware/Motor/Motor.h"
#include "Hardware/Sensors/Graydetect.h"
#include "System/Tick.h"

#define DRIVE_TEST_START_KEY    0x01U
#define DRIVE_TEST_STOP_KEY     0x02U

static uint8_t s_previousKeyMask;
static Drive_State_t s_previousDriveState;

static void App_HandleDriveKeys(uint8_t pressedEdges)
{
    Drive_Result_t result;

    if ((pressedEdges & DRIVE_TEST_STOP_KEY) != 0U)
    {
        Drive_Stop();
        return;
    }

    if ((pressedEdges & DRIVE_TEST_START_KEY) == 0U)
    {
        return;
    }

    result = Drive_StartForward(1200U, DRIVE_SPEED_NORMAL);
    if (result != DRIVE_RESULT_OK)
    {
        Beep_Long();
    }
}

static void App_ReportDriveState(void)
{
    Drive_State_t state = Drive_GetState();

    if (state == s_previousDriveState)
    {
        return;
    }

    if (state == DRIVE_STATE_COMPLETED)
    {
        Beep_Notify(2U);
    }
    else if (state == DRIVE_STATE_ERROR)
    {
        Beep_Long();
    }

    s_previousDriveState = state;
}

static void App_Init(void)
{
    Drive_Result_t driveResult;

    SYSCFG_DL_init();
    Tick_Init();

    LED_Init();
    Beep_Init();
    Key_Init();
    Graydetect_Init();
    Motor_Init();
    Servo_Init();
    Serial1_Init();
    Odometry_Init();

    __enable_irq();

    DebugDisplay_Init();
    Heading_Init();
    DebugDisplay_ShowHeadingCalibration(Heading_IsReady());
    Heading_Calibrate();

    /* 丢弃零漂标定期间累计的系统节拍和编码器计数。 */
    (void)Tick_PollCount();
    Odometry_Reset();

    BluetoothDebug_Init();
    driveResult = Drive_InitDefault();
    if (driveResult != DRIVE_RESULT_OK)
    {
        Beep_Long();
    }
    s_previousKeyMask = Key_GetPressedMask();
    s_previousDriveState = Drive_GetState();
    DebugDisplay_Update(DEBUG_DISPLAY_REFRESH_TICKS);
}

static void App_RunTick(uint8_t elapsedTicks)
{
    uint8_t index;
    uint8_t keyMask;
    uint8_t pressedEdges;
    float elapsedSeconds = (float)elapsedTicks * TICK_DT;

    Heading_Update(elapsedSeconds);
    Odometry_Update(elapsedTicks);

    keyMask = Key_GetPressedMask();
    pressedEdges = (uint8_t)(keyMask & (uint8_t)~s_previousKeyMask);
    s_previousKeyMask = keyMask;
    App_HandleDriveKeys(pressedEdges);

    Drive_Update(elapsedSeconds);
    BluetoothDebug_Update(elapsedTicks);
    App_ReportDriveState();

    for (index = 0U; index < elapsedTicks; index++)
    {
        Beep_Tick();
    }

    DebugDisplay_Update(elapsedTicks);
}

int main(void)
{
    App_Init();

    for (;;)
    {
        uint8_t elapsedTicks = Tick_PollCount();

        if (elapsedTicks == 0U)
        {
            __WFI();
            continue;
        }

        App_RunTick(elapsedTicks);
    }
}
