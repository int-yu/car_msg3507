#include "ti_msp_dl_config.h"

#include "Application/Comms/BluetoothDebug.h"
#include "Application/Control/MotionStraight.h"
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

/* 用户上机测试参数：请求速度超过 MotionStraight 上限时会自动限幅。 */
#define MOTION_STRAIGHT_TEST_DISTANCE_MM   1200U
#define MOTION_STRAIGHT_TEST_SPEED_MMPS    300.0f
#define MOTION_STRAIGHT_TEST_END_SPEED_MMPS 0.0f
#define MOTION_STRAIGHT_TEST_START_KEY     0x01U
#define MOTION_STRAIGHT_TEST_STOP_KEY      0x02U

static uint8_t s_previousKeyMask;
static MotionStraight_State_t s_previousStraightState;

static void App_HandleStraightKeys(uint8_t pressedEdges)
{
    MotionStraight_Result_t result;

    if ((pressedEdges & MOTION_STRAIGHT_TEST_STOP_KEY) != 0U)
    {
        MotionStraight_Stop();
        return;
    }

    if ((pressedEdges & MOTION_STRAIGHT_TEST_START_KEY) == 0U)
    {
        return;
    }

    result = MotionStraight_StartForward(
        MOTION_STRAIGHT_TEST_DISTANCE_MM,
        MOTION_STRAIGHT_TEST_SPEED_MMPS,
        MOTION_STRAIGHT_TEST_END_SPEED_MMPS);
    if (result != MOTION_STRAIGHT_RESULT_OK)
    {
        Beep_Long();
    }
}

static void App_ReportStraightState(void)
{
    MotionStraight_State_t state = MotionStraight_GetState();

    if (state == s_previousStraightState)
    {
        return;
    }

    if (state == MOTION_STRAIGHT_STATE_COMPLETED)
    {
        Beep_Notify(2U);
    }
    else if (state == MOTION_STRAIGHT_STATE_ERROR)
    {
        Beep_Long();
    }

    s_previousStraightState = state;
}

static void App_Init(void)
{
    MotionStraight_Result_t straightResult;

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
    straightResult = MotionStraight_InitDefault();
    if (straightResult != MOTION_STRAIGHT_RESULT_OK)
    {
        Beep_Long();
    }
    s_previousKeyMask = Key_GetPressedMask();
    s_previousStraightState = MotionStraight_GetState();
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
    App_HandleStraightKeys(pressedEdges);

    /* 当前只运行直线模式；MotionLine 尚未接入主流程。 */
    MotionStraight_Update(elapsedSeconds);
    BluetoothDebug_Update(elapsedTicks);
    App_ReportStraightState();

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
