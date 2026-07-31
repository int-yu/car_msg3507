#include "Accomplish/26H.h"
#include "Application/Comms/BluetoothDebug.h"
#include "Application/Control/BallSequence.h"
#include "Application/Control/BeamActuator.h"
#include "Application/Control/MotionManager.h"
#include "Application/Control/TaskTimer.h"
#include "Application/Core/App.h"
#include "Application/Debug/DebugDisplay.h"
#include "Application/Debug/Telemetry.h"
#include "Hardware/Comms/Serial.h"
#include "System/Interrupt.h"

#include <stdint.h>

#define MAIN_LINE_START_KEY_MASK  0x01U
#define MAIN_BALL_START_KEY_MASK  0x02U
#define MAIN_EMERGENCY_CHORD_MASK (0x01U | 0x02U)
#define MAIN_BALL_START_SIGNAL 3U
#define MAIN_BALL_STOP_SIGNAL  4U

static float s_ballTargetMM = BALL_SEQUENCE_DEFAULT_TARGET_MM;

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

static uint8_t Main_StartOrRetargetBallTask(float targetMM)
{
    if (BallSequence_IsActive() != 0U)
    {
        if (BallSequence_SetTarget(targetMM) == 0U)
        {
            Serial1_SendString("ERR BALL TARGET\r\n");
            return 0U;
        }
    }
    else if (BallSequence_Start(targetMM) == 0U)
    {
        Serial1_SendString("ERR BALL VISION\r\n");
        return 0U;
    }

    Serial1_SendString("OK BALL HOLD 0\r\n");
    return 1U;
}

static uint8_t Main_StartBallSweep(void)
{
    if (MotionManager_IsBusy() != 0U)
    {
        Serial1_SendString("ERR BALL CAR BUSY\r\n");
        return 0U;
    }
    if (BallSequence_StartSweep() == 0U)
    {
        Serial1_SendString("ERR BALL VISION\r\n");
        return 0U;
    }

    TaskTimer_Start(TASK_TIMER_OWNER_BALL);
    Serial1_SendString("OK BALL SWEEP\r\n");
    return 1U;
}

static uint8_t Main_LineCanStart(void)
{
    Accomplish26H_State_t state = Accomplish26H_GetState();

    return ((state == ACCOMPLISH_26H_STATE_READY) ||
            (state == ACCOMPLISH_26H_STATE_FINISHED) ||
            (state == ACCOMPLISH_26H_STATE_ERROR)) ? 1U : 0U;
}

int main(void)
{
    App_UpdateContext_t updateContext;

    App_Init();
    Accomplish26H_Init();
    BallSequence_Init();
    s_ballTargetMM = BALL_SEQUENCE_DEFAULT_TARGET_MM;
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
            uint8_t lineStartKeyPressed =
                ((updateContext.pressedEdges &
                  MAIN_LINE_START_KEY_MASK) != 0U) ? 1U : 0U;
            uint8_t ballStartKeyPressed =
                ((updateContext.pressedEdges &
                  MAIN_BALL_START_KEY_MASK) != 0U) ? 1U : 0U;
            uint8_t startAllowed =
                ((emergencyStopRequested == 0U) &&
                 ((updateContext.pressedKeys &
                   MAIN_EMERGENCY_CHORD_MASK) !=
                  MAIN_EMERGENCY_CHORD_MASK)) ? 1U : 0U;
            float requestedBallTargetMM;

            if (BluetoothDebug_TakeBallTargetMM(
                    &requestedBallTargetMM) != 0U)
            {
                s_ballTargetMM = requestedBallTargetMM;
                if (BallSequence_IsActive() != 0U)
                {
                    (void)BallSequence_SetTarget(s_ballTargetMM);
                }
            }

            if ((lineStartKeyPressed != 0U) &&
                (Main_LineCanStart() != 0U))
            {
                s_ballTargetMM = BALL_SEQUENCE_DEFAULT_TARGET_MM;
                if ((startAllowed == 0U) ||
                    (ballStopRequested != 0U) ||
                    (Main_StartOrRetargetBallTask(
                         BALL_SEQUENCE_DEFAULT_TARGET_MM) == 0U))
                {
                    updateContext.pressedEdges &=
                        (uint8_t)~MAIN_LINE_START_KEY_MASK;
                }
            }

            Accomplish26H_Update(&updateContext);

            if (ballStopRequested != 0U)
            {
                BallSequence_Stop();
                Serial1_SendString("OK BALL STOP\r\n");
            }
            else if ((startAllowed != 0U) &&
                     (ballStartKeyPressed != 0U))
            {
                (void)Main_StartBallSweep();
            }
            else if ((startAllowed != 0U) &&
                     (ballStartRequested != 0U))
            {
                (void)Main_StartBallTask(s_ballTargetMM);
            }

            BallSequence_Update(updateContext.dt);
            Telemetry_Update(updateContext.elapsedTicks, updateContext.pressedKeys);
            BeamActuator_Update(updateContext.dt);
            DebugDisplay_Update();
        }
    }
}
