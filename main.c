#include "Accomplish/26H.h"
#include "Application/Comms/BluetoothDebug.h"
#include "Application/Control/BallSequence.h"
#include "Application/Control/BeamActuator.h"
#include "Application/Control/MotionManager.h"
#include "Application/Core/App.h"
#include "Application/Debug/DebugDisplay.h"
#include "Application/Debug/Telemetry.h"
#include "Hardware/Comms/Serial.h"
#include "System/Interrupt.h"

#include <stdint.h>

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

static uint8_t Main_StartBallTask(float targetMM)
{
    if (MotionManager_IsBusy() != 0U)
    {
        Serial1_SendString("ERR BALL CAR BUSY\r\n");
        return 0U;
    }
    if (BallSequence_IsActive() != 0U)
    {
        Serial1_SendString("ERR BALL BUSY\r\n");
        return 0U;
    }
    if (BallSequence_Start(targetMM) == 0U)
    {
        Serial1_SendString("ERR BALL VISION\r\n");
        return 0U;
    }

    Serial1_SendString("OK BALL START\r\n");
    return 1U;
}

int main(void)
{
    App_UpdateContext_t updateContext;

    App_Init();
    Accomplish26H_Init();
    BallSequence_Init();
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
            uint8_t startAllowed =
                ((emergencyStopRequested == 0U) &&
                 ((updateContext.pressedKeys &
                   MAIN_EMERGENCY_CHORD_MASK) !=
                  MAIN_EMERGENCY_CHORD_MASK)) ? 1U : 0U;
            float ballTargetMM = BALL_SEQUENCE_DEFAULT_TARGET_MM;

            if (ballStartRequested != 0U)
            {
                (void)BluetoothDebug_TakeBallTargetMM(&ballTargetMM);
            }

            Accomplish26H_Update(&updateContext);

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
                (void)Main_StartBallTask(ballTargetMM);
            }

            BallSequence_Update(updateContext.dt);
            Telemetry_Update(updateContext.elapsedTicks, updateContext.pressedKeys);
            BeamActuator_Update(updateContext.dt);
            DebugDisplay_Update(updateContext.elapsedTicks);
        }
    }
}
