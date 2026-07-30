#include "Accomplish/26H.h"
#include "Application/Control/BallSequence.h"
#include "Application/Control/BallHold.h"
#include "Application/Control/BeamActuator.h"
#include "Application/Control/MotionManager.h"
#include "Application/Core/App.h"
#include "Application/Debug/Telemetry.h"
#include "Hardware/Comms/Serial.h"
#include "System/Interrupt.h"

/* KEY2 单独按下启动要求 3 的摆球序列，KEY3 单独按下启动要求 4 的 O 点
 * 保持；KEY1 是要求 2 的单圈起跑，KEY1+KEY2 同时按下是物理急停，由 App
 * 统一处理。 */
#define MAIN_BALL_START_KEY_MASK  0x02U
#define MAIN_HOLD_START_KEY_MASK  0x04U
#define MAIN_EMERGENCY_CHORD_MASK (0x01U | 0x02U)
#define MAIN_BALL_START_SIGNAL 3U
#define MAIN_BALL_STOP_SIGNAL  4U
#define MAIN_HOLD_START_SIGNAL 5U
#define MAIN_HOLD_STOP_SIGNAL  6U

static uint8_t Main_HasSignal(const App_UpdateContext_t *context,
                              uint8_t signal)
{
    return ((context->hasBluetoothSignal != 0U) &&
            (context->bluetoothSignal == signal)) ? 1U : 0U;
}

static uint8_t Main_BallIsActive(void)
{
    BallSequence_State_t state = BallSequence_GetState();

    return ((state == BALL_SEQUENCE_STATE_TO_PLUS) ||
            (state == BALL_SEQUENCE_STATE_TO_MINUS) ||
            (state == BALL_SEQUENCE_STATE_HOLD_MINUS)) ? 1U : 0U;
}

static void Main_ReportBallStart(void)
{
    if (MotionManager_IsBusy() != 0U)
    {
        Serial1_SendString("ERR BALL CAR BUSY\r\n");
        return;
    }
    /* 要求 3 和要求 4 都独占摆杆，谁在跑都不许另一个插进来。 */
    if ((Main_BallIsActive() != 0U) ||
        (BallHold_IsActive() != 0U))
    {
        Serial1_SendString("ERR BALL BUSY\r\n");
        return;
    }
    if (BallSequence_Start() == 0U)
    {
        Serial1_SendString("ERR BALL VISION\r\n");
        return;
    }
    Serial1_SendString("OK BALL START\r\n");
}

/* 要求 4：钢球从任意位置放下后收敛并稳定在 O 点。小车静止，只驱动摆杆。 */
static void Main_ReportHoldStart(void)
{
    if (MotionManager_IsBusy() != 0U)
    {
        Serial1_SendString("ERR HOLD CAR BUSY\r\n");
        return;
    }
    if ((Main_BallIsActive() != 0U) ||
        (BallHold_IsActive() != 0U))
    {
        Serial1_SendString("ERR HOLD BUSY\r\n");
        return;
    }
    if (BallHold_Start() == 0U)
    {
        Serial1_SendString("ERR HOLD VISION\r\n");
        return;
    }
    Serial1_SendString("OK HOLD START\r\n");
}

int main(void)
{
    App_UpdateContext_t updateContext;

    App_Init();
    Accomplish26H_Init();
    BallSequence_Init();
    BallHold_Init();
    Interrupt_Enable();

    for (;;)
    {
        if (App_Update(&updateContext) != 0U)
        {
            uint8_t emergencyStopRequested =
                Main_HasSignal(&updateContext, 0U);
            uint8_t ballStopRequested =
                Main_HasSignal(&updateContext, MAIN_BALL_STOP_SIGNAL);
            uint8_t ballStartRequested =
                Main_HasSignal(&updateContext, MAIN_BALL_START_SIGNAL);
            uint8_t holdStopRequested =
                Main_HasSignal(&updateContext, MAIN_HOLD_STOP_SIGNAL);
            uint8_t holdStartRequested =
                Main_HasSignal(&updateContext, MAIN_HOLD_START_SIGNAL);
            /* 急停组合键按下的那一拍，任何摆杆起跑都不许成立。 */
            uint8_t startAllowed =
                ((emergencyStopRequested == 0U) &&
                 ((updateContext.pressedKeys &
                   MAIN_EMERGENCY_CHORD_MASK) !=
                  MAIN_EMERGENCY_CHORD_MASK)) ? 1U : 0U;

            Accomplish26H_Update(&updateContext);

            /* 要求 3 与要求 2 是不同测试项，不会同时进行。只在急停
             * 组合键未按下时，把 KEY2 的按下沿或网页 C3 当作摆球起跑。
             * C0 与 C4 都优先于同拍的 KEY2，避免刚停下又被重新启动。 */
            if (ballStopRequested != 0U)
            {
                BallSequence_Stop();
                Serial1_SendString("OK BALL STOP\r\n");
            }
            else if ((startAllowed != 0U) &&
                     (((updateContext.pressedEdges &
                        MAIN_BALL_START_KEY_MASK) != 0U) ||
                      (ballStartRequested != 0U)))
            {
                Main_ReportBallStart();
            }

            /* 要求 4 走同一套规则，只是换成 KEY3 / C5 / C6。 */
            if (holdStopRequested != 0U)
            {
                BallHold_Stop();
                Serial1_SendString("OK HOLD STOP\r\n");
            }
            else if ((startAllowed != 0U) &&
                     (((updateContext.pressedEdges &
                        MAIN_HOLD_START_KEY_MASK) != 0U) ||
                      (holdStartRequested != 0U)))
            {
                Main_ReportHoldStart();
            }

            BallSequence_Update(updateContext.dt);
            BallHold_Update(updateContext.dt);
            /* 摆球参考和 K230 球位必须在同一控制拍采样，曲线才能直接比较。 */
            Telemetry_Update(updateContext.elapsedTicks, updateContext.pressedKeys);
            /* 执行层放在任务层之后：本拍算出的倾角当拍下发给步进。 */
            BeamActuator_Update(updateContext.dt);
        }
    }
}
