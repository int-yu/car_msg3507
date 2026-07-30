#include "Accomplish/26H.h"
#include "Accomplish/26H_Ball.h"
#include "Application/Control/BeamActuator.h"
#include "Application/Control/MotionManager.h"
#include "Application/Core/App.h"
#include "Application/Debug/Telemetry.h"
#include "Hardware/Comms/Serial.h"
#include "System/Interrupt.h"

/* KEY2 单独按下启动要求 3 的摆球序列；KEY1 是要求 2 的单圈起跑，
 * 两键同时按下是物理急停，由 App 统一处理。 */
#define MAIN_BALL_START_KEY_MASK  0x02U
#define MAIN_EMERGENCY_CHORD_MASK (0x01U | 0x02U)
#define MAIN_BALL_START_SIGNAL 3U
#define MAIN_BALL_STOP_SIGNAL  4U

static uint8_t Main_HasSignal(const App_UpdateContext_t *context,
                              uint8_t signal)
{
    return ((context->hasBluetoothSignal != 0U) &&
            (context->bluetoothSignal == signal)) ? 1U : 0U;
}

static uint8_t Main_BallIsActive(void)
{
    Accomplish26HBall_State_t state = Accomplish26HBall_GetState();

    return ((state == ACCOMPLISH_26H_BALL_STATE_TO_PLUS) ||
            (state == ACCOMPLISH_26H_BALL_STATE_TO_MINUS) ||
            (state == ACCOMPLISH_26H_BALL_STATE_HOLD_MINUS)) ? 1U : 0U;
}

static void Main_ReportBallStart(void)
{
    if (MotionManager_IsBusy() != 0U)
    {
        Serial1_SendString("ERR BALL CAR BUSY\r\n");
        return;
    }
    if (Main_BallIsActive() != 0U)
    {
        Serial1_SendString("ERR BALL BUSY\r\n");
        return;
    }
    if (Accomplish26HBall_Start() == 0U)
    {
        Serial1_SendString("ERR BALL VISION\r\n");
        return;
    }
    Serial1_SendString("OK BALL START\r\n");
}

int main(void)
{
    App_UpdateContext_t updateContext;

    App_Init();
    Accomplish26H_Init();
    Accomplish26HBall_Init();
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

            Accomplish26H_Update(&updateContext);

            /* 要求 3 与要求 2 是不同测试项，不会同时进行。只在急停
             * 组合键未按下时，把 KEY2 的按下沿或网页 C3 当作摆球起跑。
             * C0 与 C4 都优先于同拍的 KEY2，避免刚停下又被重新启动。 */
            if (ballStopRequested != 0U)
            {
                Accomplish26HBall_Stop();
                Serial1_SendString("OK BALL STOP\r\n");
            }
            else if ((emergencyStopRequested == 0U) &&
                     ((updateContext.pressedKeys &
                       MAIN_EMERGENCY_CHORD_MASK) !=
                      MAIN_EMERGENCY_CHORD_MASK) &&
                     (((updateContext.pressedEdges &
                        MAIN_BALL_START_KEY_MASK) != 0U) ||
                      (ballStartRequested != 0U)))
            {
                Main_ReportBallStart();
            }

            Accomplish26HBall_Update(updateContext.dt);
            /* 摆球参考和 K230 球位必须在同一控制拍采样，曲线才能直接比较。 */
            Telemetry_Update(updateContext.elapsedTicks, updateContext.pressedKeys);
            /* 执行层放在任务层之后：本拍算出的倾角当拍下发给步进。 */
            BeamActuator_Update(updateContext.dt);
        }
    }
}
