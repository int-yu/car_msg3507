#include "Application/Core/TestApp.h"
#include "Application/Comms/BluetoothDebug.h"
#include "Application/Control/MotionManager.h"
#include "Application/Gimbal/Gimbal.h"
#include "Application/Servo/Servo.h"
#include "Hardware/Board/Key.h"
#include "Hardware/Comms/Serial.h"
#include "Hardware/Motor/Motor.h"
#include "System/Tick.h"
#include "ti_msp_dl_config.h"
#include <stddef.h>

static uint8_t s_previousKeyMask;

void TestApp_Init(void)
{
    __disable_irq();

    SYSCFG_DL_init();
    Tick_Init();

    /* 保留 KEY2 停止、蓝牙 C0 停止以及舵机调试，其他整车硬件不初始化。 */
    Key_Init();
    Motor_Init();
    Servo_Init();
    Serial1_Init();
    BluetoothDebug_Init();
    (void)MotionManager_Init();
    (void)Gimbal_Init();

    s_previousKeyMask = Key_GetPressedMask();
}

uint8_t TestApp_Update(App_UpdateContext_t *context)
{
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

    Gimbal_Update(context->dt);

    keyMask = Key_GetPressedMask();
    context->pressedKeys = keyMask;
    context->pressedEdges =
        (uint8_t)(keyMask & (uint8_t)~s_previousKeyMask);
    s_previousKeyMask = keyMask;

    /* 测试通道拒绝 L/R/U 小车手动电机命令，但保留 C0 和 O/D。 */
    BluetoothDebug_Update(elapsedTicks, 0U);
    context->hasBluetoothSignal =
        BluetoothDebug_PopSignal(&context->bluetoothSignal);

    if ((context->hasBluetoothSignal != 0U) &&
        (context->bluetoothSignal == 0U))
    {
        MotionManager_Stop();
        Motor_StopAll();
        (void)Gimbal_Disable();
    }

    MotionManager_Update(context->dt);
    return 1U;
}
