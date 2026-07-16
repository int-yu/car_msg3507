#include "ti_msp_dl_config.h"

#include "Application/Comms/BluetoothDebug.h"
#include "Application/Control/Nav.h"
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

/* 用户上机测试参数：角度和轮速均可在此处直接修改。 */
#define NAV_TEST_ABSOLUTE_YAW_DEG  -90.0f
#define NAV_TEST_RELATIVE_YAW_DEG  -90.0f
#define NAV_TEST_BASE_SPEED_MMPS   80.0f

#define NAV_TEST_TO_KEY          0x01U /* KEY1：转到绝对角。 */
#define NAV_TEST_BY_KEY          0x02U /* KEY2：按设定相对角转向。 */
#define NAV_TEST_BY_REVERSE_KEY  0x04U /* KEY3：向相反方向转相同角度。 */
#define NAV_TEST_STOP_KEY        0x08U /* KEY4：立即停止当前转向。 */

static uint8_t s_previousKeyMask;
static Nav_State_t s_previousNavState;

static void App_HandleNavKeys(uint8_t pressedEdges)
{
    Nav_Result_t result;

    if ((pressedEdges & NAV_TEST_STOP_KEY) != 0U)
    {
        Nav_Stop();
        return;
    }

    if ((pressedEdges & NAV_TEST_TO_KEY) != 0U)
    {
        result = Nav_StartTo(
            NAV_TEST_ABSOLUTE_YAW_DEG,
            NAV_TEST_BASE_SPEED_MMPS);
    }
    else if ((pressedEdges & NAV_TEST_BY_KEY) != 0U)
    {
        result = Nav_StartBy(
            NAV_TEST_RELATIVE_YAW_DEG,
            NAV_TEST_BASE_SPEED_MMPS);
    }
    else if ((pressedEdges & NAV_TEST_BY_REVERSE_KEY) != 0U)
    {
        result = Nav_StartBy(
            -NAV_TEST_RELATIVE_YAW_DEG,
            NAV_TEST_BASE_SPEED_MMPS);
    }
    else
    {
        return;
    }

    if (result != NAV_RESULT_OK)
    {
        Beep_Long();
    }
}

static void App_ReportNavState(void)
{
    Nav_State_t state = Nav_GetState();

    if (state == s_previousNavState)
    {
        return;
    }

    if (state == NAV_STATE_COMPLETED)
    {
        Beep_Notify(2U);
    }
    else if (state == NAV_STATE_ERROR)
    {
        Beep_Long();
    }

    s_previousNavState = state;
}

static void App_Init(void)
{
    Nav_Result_t navResult;

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
    navResult = Nav_Init();
    if (navResult != NAV_RESULT_OK)
    {
        Beep_Long();
    }

    s_previousKeyMask = Key_GetPressedMask();
    s_previousNavState = Nav_GetState();
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
    App_HandleNavKeys(pressedEdges);

    /* 当前主流程只运行 Nav 测试；直线和巡线模式不更新电机。 */
    Nav_Update(elapsedSeconds);
    BluetoothDebug_Update(elapsedTicks);
    App_ReportNavState();

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
