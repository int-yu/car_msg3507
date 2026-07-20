#include "Application/Core/App.h"
#include "Application/Comms/BluetoothDebug.h"
#include "Application/Comms/K230Link.h"
#include "Application/Control/MotionManager.h"
#include "Application/Debug/DebugDisplay.h"
#include "Application/Debug/Capture.h"
#include "Application/Debug/Telemetry.h"
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
#include "ti_msp_dl_config.h"
#include <stddef.h>

static uint8_t s_previousKeyMask;
static MotionManager_Error_t s_previousMotionError;

static void App_ReportMotionError(void)
{
    MotionManager_Error_t error = MotionManager_GetError();

    if ((error != MOTION_MANAGER_ERROR_NONE) &&
        (error != s_previousMotionError))
    {
        Beep_Long();
    }
    s_previousMotionError = error;
}

void App_Init(void)
{
    __disable_irq();

    SYSCFG_DL_init();
    Tick_Init();

    LED_Init();
    Beep_Init();
    Key_Init();
    Graydetect_Init();
    Motor_Init();
    Servo_Init();
    Serial1_Init();
    K230Link_Init();
    Odometry_Init();

    DebugDisplay_Init();
    Heading_Init();
    DebugDisplay_ShowHeadingCalibration(Heading_IsReady());
    Heading_Calibrate();

    /* 标定期间全局中断保持关闭，从正式流程的零时刻重新开始计数。 */
    Tick_Init();
    Odometry_Reset();

    BluetoothDebug_Init();
    Telemetry_Init();
    Capture_Init();
    if (MotionManager_Init() != MOTION_MANAGER_RESULT_OK)
    {
        Beep_Long();
    }

    s_previousKeyMask = Key_GetPressedMask();
    s_previousMotionError = MotionManager_GetError();
    DebugDisplay_Update(DEBUG_DISPLAY_REFRESH_TICKS);
}

uint8_t App_Update(App_UpdateContext_t *context)
{
    uint8_t index;
    uint8_t elapsedTicks;
    uint8_t keyMask;

    if (context == NULL)
    {
        return 0U;
    }

    elapsedTicks = Tick_PollCount();
    if (elapsedTicks == 0U)
    {
        __WFI();
        return 0U;
    }

    context->elapsedTicks = elapsedTicks;
    context->dt = (float)elapsedTicks * TICK_DT;
    context->hasBluetoothSignal = 0U;
    context->bluetoothSignal = 0U;

    Heading_Update(context->dt);
    Odometry_Update(elapsedTicks);

    keyMask = Key_GetPressedMask();
    context->pressedKeys = keyMask;
    context->pressedEdges =
        (uint8_t)(keyMask & (uint8_t)~s_previousKeyMask);
    s_previousKeyMask = keyMask;

    K230Link_Update(elapsedTicks);

    BluetoothDebug_Update(
        elapsedTicks, (MotionManager_IsBusy() == 0U) ? 1U : 0U);
    context->hasBluetoothSignal =
        BluetoothDebug_PopSignal(&context->bluetoothSignal);

    /* C0 是不受 Mission 可打断属性限制的全局停车信号。 */
    if ((context->hasBluetoothSignal != 0U) &&
        (context->bluetoothSignal == 0U))
    {
        MotionManager_Stop();
        Motor_StopAll();
    }

    MotionManager_Update(context->dt);
    App_ReportMotionError();

    {
        uint8_t captureOk;
        uint16_t captureIndex;

        if (K230Link_PopCaptureAck(&captureOk, &captureIndex) != 0U)
        {
            if (captureOk != 0U)
            {
                Serial1_Printf("OK CAP %u\r\n", (unsigned)captureIndex);
            }
            else
            {
                Serial1_SendString("ERR CAP FAIL\r\n");
            }
        }
    }

    /* 必须在 MotionManager_Update() 之后：目标速度和输出 PWM 都是本拍算出的，
     * 提前采会记录到上一拍的值，阶跃起点会整体偏移一个控制周期。 */
    Capture_Update((uint32_t)elapsedTicks * 10U);

    Telemetry_Update(elapsedTicks, context->pressedKeys);
    /* dump 与实时流互斥：捕获输出期间车已停稳，让它独占串口。 */
    Capture_DumpNext();

    for (index = 0U; index < elapsedTicks; index++)
    {
        Beep_Tick();
    }

    DebugDisplay_Update(elapsedTicks);
    return 1U;
}
