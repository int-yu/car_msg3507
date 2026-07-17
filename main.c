#include "ti_msp_dl_config.h"

#include "Application/Comms/BluetoothDebug.h"
#include "Application/Control/MotionLine.h"
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

/* 用户上机测试参数：首次巡线从低速开始，在这里直接修改速度。 */
#define MOTION_LINE_TEST_SPEED_MMPS  100.0f

#define MOTION_LINE_TEST_START_KEY  0x01U /* KEY1：开始持续巡线。 */
#define MOTION_LINE_TEST_STOP_KEY   0x02U /* KEY2：立即停止巡线。 */

static uint8_t s_previousKeyMask;
static MotionLine_State_t s_previousMotionLineState;

static void App_HandleMotionLineKeys(uint8_t pressedEdges)
{
    MotionLine_Result_t result;

    if ((pressedEdges & MOTION_LINE_TEST_STOP_KEY) != 0U)
    {
        MotionLine_Stop();
        return;
    }

    if ((pressedEdges & MOTION_LINE_TEST_START_KEY) == 0U)
    {
        return;
    }

    result = MotionLine_Start(MOTION_LINE_TEST_SPEED_MMPS);
    if (result != MOTION_LINE_RESULT_OK)
    {
        Beep_Long();
    }
}

static void App_ReportMotionLineState(void)
{
    MotionLine_State_t state = MotionLine_GetState();

    if (state == s_previousMotionLineState)
    {
        return;
    }

    if (state == MOTION_LINE_STATE_ERROR)
    {
        Beep_Long();
    }

    s_previousMotionLineState = state;
}

static void App_Init(void)
{
    MotionLine_Result_t motionLineResult;

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
    motionLineResult = MotionLine_Init();
    if (motionLineResult != MOTION_LINE_RESULT_OK)
    {
        Beep_Long();
    }

    s_previousKeyMask = Key_GetPressedMask();
    s_previousMotionLineState = MotionLine_GetState();
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

    /* 当前为独立巡线测试，不等待 K230 握手。 */
    App_HandleMotionLineKeys(pressedEdges);
    MotionLine_Update(elapsedSeconds);
    BluetoothDebug_Update(elapsedTicks);

    App_ReportMotionLineState();

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
