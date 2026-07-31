#include "Application/Core/Main26H.h"

#include "Accomplish/26H.h"
#include "Application/Comms/BluetoothDebug.h"
#include "Application/Control/BallHold.h"
#include "Application/Control/BallSensor.h"
#include "Application/Control/BallSequence.h"
#include "Application/Control/BeamActuator.h"
#include "Application/Control/MotionManager.h"
#include "Application/Core/App.h"
#include "Application/Debug/DebugDisplay.h"
#include "Application/Debug/Telemetry.h"
#include "Hardware/Comms/Serial.h"
#include "Hardware/Motor/Stepper.h"
#include "System/Interrupt.h"

#define MAIN_BALL_START_KEY_MASK  0x02U
#define MAIN_HOLD_START_KEY_MASK  0x04U
#define MAIN_EMERGENCY_CHORD_MASK (0x01U | 0x02U)
#define MAIN_BALL_START_SIGNAL 3U
#define MAIN_BALL_STOP_SIGNAL  4U
#define MAIN_HOLD_START_SIGNAL 5U
#define MAIN_HOLD_STOP_SIGNAL  6U

static uint8_t s_ballAutoStartPending;
static float s_ballTargetMM = BALL_SEQUENCE_DEFAULT_TARGET_MM;

static uint8_t Main26H_HasSignal(const App_UpdateContext_t *context,
                                 uint8_t signal)
{
    return ((context->hasBluetoothSignal != 0U) &&
            (context->bluetoothSignal == signal)) ? 1U : 0U;
}

static uint8_t Main26H_BallIsActive(void)
{
    return BallSequence_IsActive();
}

static uint8_t Main26H_ReportBallStart(float targetMM)
{
    if (MotionManager_IsBusy() != 0U)
    {
        Serial1_SendString("ERR BALL CAR BUSY\r\n");
        return 0U;
    }
    if ((Main26H_BallIsActive() != 0U) ||
        (BallHold_IsActive() != 0U))
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

static uint8_t Main26H_AutoStartIsReady(void)
{
    Stepper_Status_t status;
    float horizontalErrorDeg;

    if ((MAIN26H_BALL_AUTO_START_ENABLED == 0U) ||
        (BallSensor_IsFresh() == 0U) ||
        (MotionManager_IsBusy() != 0U) ||
        (Main26H_BallIsActive() != 0U) ||
        (BallHold_IsActive() != 0U))
    {
        return 0U;
    }

    Stepper_GetStatus(&status);
    if ((!status.enabled) || (!status.ready) || status.busy ||
        (!status.pwmValid))
    {
        return 0U;
    }

    horizontalErrorDeg =
        status.absoluteAngleDeg - STEPPER_INITIAL_ANGLE_DEG;
    if (horizontalErrorDeg < 0.0f)
    {
        horizontalErrorDeg = -horizontalErrorDeg;
    }
    return (horizontalErrorDeg <=
            MAIN26H_HORIZONTAL_READY_TOLERANCE_DEG) ? 1U : 0U;
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
    s_ballTargetMM = BALL_SEQUENCE_DEFAULT_TARGET_MM;
    BallHold_Init();
    s_ballAutoStartPending =
        (MAIN26H_BALL_AUTO_START_ENABLED != 0U) ? 1U : 0U;
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
            float requestedBallTargetMM;

            if (BluetoothDebug_TakeBallTargetMM(
                    &requestedBallTargetMM) != 0U)
            {
                s_ballTargetMM = requestedBallTargetMM;
                if (Main26H_BallIsActive() != 0U)
                {
                    (void)BallSequence_SetTarget(s_ballTargetMM);
                }
            }

            Accomplish26H_Update(&updateContext);

            if ((emergencyStopRequested != 0U) ||
                (ballStopRequested != 0U))
            {
                s_ballAutoStartPending = 0U;
            }

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
                s_ballAutoStartPending = 0U;
                (void)Main26H_ReportBallStart(s_ballTargetMM);
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

            if ((s_ballAutoStartPending != 0U) &&
                (startAllowed != 0U) &&
                (holdStartRequested == 0U) &&
                (Main26H_AutoStartIsReady() != 0U) &&
                (Main26H_ReportBallStart(s_ballTargetMM) != 0U))
            {
                s_ballAutoStartPending = 0U;
            }

            BallSequence_Update(updateContext.dt);
            BallHold_Update(updateContext.dt);
            Telemetry_Update(updateContext.elapsedTicks, updateContext.pressedKeys);
            BeamActuator_Update(updateContext.dt);
            DebugDisplay_Update(updateContext.elapsedTicks);
        }
    }
}
