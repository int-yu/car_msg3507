#include "Application/Core/Main26H.h"

#include "Accomplish/26H.h"
#include "Application/Control/BallHold.h"
#include "Application/Control/BallSequence.h"
#include "Application/Control/BeamActuator.h"
#include "Application/Control/MotionManager.h"
#include "Application/Core/App.h"
#include "Application/Debug/Telemetry.h"
#include "Hardware/Comms/Serial.h"
#include "System/Interrupt.h"

#define MAIN_BALL_START_KEY_MASK  0x02U
#define MAIN_HOLD_START_KEY_MASK  0x04U
#define MAIN_EMERGENCY_CHORD_MASK (0x01U | 0x02U)
#define MAIN_BALL_START_SIGNAL 3U
#define MAIN_BALL_STOP_SIGNAL  4U
#define MAIN_HOLD_START_SIGNAL 5U
#define MAIN_HOLD_STOP_SIGNAL  6U

static uint8_t Main26H_HasSignal(const App_UpdateContext_t *context,
                                 uint8_t signal)
{
    return ((context->hasBluetoothSignal != 0U) &&
            (context->bluetoothSignal == signal)) ? 1U : 0U;
}

static uint8_t Main26H_BallIsActive(void)
{
    BallSequence_State_t state = BallSequence_GetState();

    return ((state == BALL_SEQUENCE_STATE_TO_PLUS) ||
            (state == BALL_SEQUENCE_STATE_TO_MINUS) ||
            (state == BALL_SEQUENCE_STATE_HOLD_MINUS)) ? 1U : 0U;
}

static void Main26H_ReportBallStart(void)
{
    if (MotionManager_IsBusy() != 0U)
    {
        Serial1_SendString("ERR BALL CAR BUSY\r\n");
        return;
    }
    if ((Main26H_BallIsActive() != 0U) ||
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

static void Main26H_ReportHoldStart(void)
{
    if (MotionManager_IsBusy() != 0U)
    {
        Serial1_SendString("ERR HOLD CAR BUSY\r\n");
        return;
    }
    if ((Main26H_BallIsActive() != 0U) ||
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

void Main26H_Run(void)
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
                Main26H_HasSignal(&updateContext, 0U);
            uint8_t ballStopRequested =
                Main26H_HasSignal(&updateContext, MAIN_BALL_STOP_SIGNAL);
            uint8_t ballStartRequested =
                Main26H_HasSignal(&updateContext, MAIN_BALL_START_SIGNAL);
            uint8_t holdStopRequested =
                Main26H_HasSignal(&updateContext, MAIN_HOLD_STOP_SIGNAL);
            uint8_t holdStartRequested =
                Main26H_HasSignal(&updateContext, MAIN_HOLD_START_SIGNAL);
            uint8_t startAllowed =
                ((emergencyStopRequested == 0U) &&
                 ((updateContext.pressedKeys &
                   MAIN_EMERGENCY_CHORD_MASK) !=
                  MAIN_EMERGENCY_CHORD_MASK)) ? 1U : 0U;

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
                Main26H_ReportBallStart();
            }

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
                Main26H_ReportHoldStart();
            }

            BallSequence_Update(updateContext.dt);
            BallHold_Update(updateContext.dt);
            Telemetry_Update(updateContext.elapsedTicks, updateContext.pressedKeys);
            BeamActuator_Update(updateContext.dt);
        }
    }
}
