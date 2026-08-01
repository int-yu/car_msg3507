#include "Accomplish/26H.h"
#include "Application/Comms/BluetoothDebug.h"
#include "Application/Control/BallSensor.h"
#include "Application/Control/BallSequence.h"
#include "Application/Control/BallTargetCapture.h"
#include "Application/Control/BeamActuator.h"
#include "Application/Control/MotionManager.h"
#include "Application/Control/TaskTimer.h"
#include "Application/Control/TimedLineRun.h"
#include "Application/Core/App.h"
#include "Application/Debug/DebugDisplay.h"
#include "Application/Debug/Telemetry.h"
#include "Hardware/Comms/Serial.h"
#include "Hardware/Motor/Stepper.h"
#include "System/Interrupt.h"

#include <math.h>
#include <stdint.h>

#define MAIN_LINE_START_KEY_MASK  0x01U
#define MAIN_BALL_START_KEY_MASK  0x02U
#define MAIN_TIMED_LINE_START_KEY_MASK 0x04U
#define MAIN_KEY4_START_KEY_MASK  0x08U
#define MAIN_EMERGENCY_CHORD_MASK (0x01U | 0x02U)
#define MAIN_BALL_SWEEP_SIGNAL 2U
#define MAIN_BALL_START_SIGNAL 3U
#define MAIN_BALL_STOP_SIGNAL  4U
static float s_ballTargetMM = BALL_SEQUENCE_DEFAULT_TARGET_MM;
static uint8_t s_defaultBallHoldPending;
static uint8_t s_key2SweepArmed;
static uint8_t s_key4CaptureArmed;
static uint8_t s_key4HoldReady;
static uint8_t s_key4RunActive;

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

    Serial1_SendString("OK BALL START\r\n");
    return 1U;
}

static uint8_t Main_StartOrRetargetBallTask(float targetMM)
{
    if (BallSequence_IsActive() != 0U)
    {
        if ((fabsf(BallSequence_GetTargetMM() - targetMM) > 0.01f) &&
            (BallSequence_SetTarget(targetMM) == 0U))
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

    Serial1_SendString("OK BALL HOLD\r\n");
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

    return ((((state == ACCOMPLISH_26H_STATE_READY) ||
              (state == ACCOMPLISH_26H_STATE_FINISHED) ||
              (state == ACCOMPLISH_26H_STATE_ERROR))) &&
            (TimedLineRun_IsActive() == 0U) &&
            (TimedLineRun_CanStart() != 0U)) ? 1U : 0U;
}

static uint8_t Main_BallHoldCanStart(void)
{
    Stepper_Status_t status;

    if ((BallSensor_IsFresh() == 0U) ||
        (BeamActuator_IsZeroCalibrated() == 0U) ||
        (MotionManager_IsBusy() != 0U) ||
        (BallSequence_IsActive() != 0U))
    {
        return 0U;
    }

    Stepper_GetStatus(&status);
    if ((!status.enabled) || (!status.ready) || status.busy ||
        (!status.pwmValid))
    {
        return 0U;
    }
    return 1U;
}

static void Main_CancelKey4Preparation(void)
{
    if (s_key4RunActive == 0U)
    {
        BallTargetCapture_Cancel();
        s_key4CaptureArmed = 0U;
        s_key4HoldReady = 0U;
    }
}

static void Main_CancelKey2Preparation(void)
{
    s_key2SweepArmed = 0U;
}

static void Main_ResetKey4Task(void)
{
    BallTargetCapture_Cancel();
    s_key4CaptureArmed = 0U;
    s_key4HoldReady = 0U;
    s_key4RunActive = 0U;
}

static uint8_t Main_StartTimedLineAtBallTarget(
    float targetMM, const char *errorMessage)
{
    s_ballTargetMM = targetMM;
    if (Main_StartOrRetargetBallTask(targetMM) == 0U)
    {
        return 0U;
    }
    if (TimedLineRun_Start() == 0U)
    {
        BallSequence_Stop();
        Serial1_SendString(errorMessage);
        return 0U;
    }
    return 1U;
}

static void Main_HandleKey2Press(uint8_t startAllowed,
                                 uint8_t ballStopRequested)
{
    float inheritedTiltDeg;

    if ((startAllowed == 0U) || (ballStopRequested != 0U))
    {
        return;
    }

    if ((s_key2SweepArmed == 0U) &&
        (MotionManager_IsBusy() != 0U))
    {
        Serial1_SendString("ERR KEY2 CAR BUSY\r\n");
        return;
    }

    Main_CancelKey4Preparation();
    s_defaultBallHoldPending = 0U;

    if (s_key2SweepArmed != 0U)
    {
        if (Main_StartBallSweep() != 0U)
        {
            s_key2SweepArmed = 0U;
        }
        return;
    }

    inheritedTiltDeg = BeamActuator_GetTiltDeg();
    BallSequence_Stop();
    BeamActuator_SetTiltDeg(inheritedTiltDeg);
    s_key2SweepArmed = 1U;
    Serial1_SendString("OK KEY2 ARMED\r\n");
}

static void Main_HandleKey4Press(uint8_t startAllowed,
                                 uint8_t ballStopRequested)
{
    float inheritedTiltDeg;

    if ((startAllowed == 0U) || (ballStopRequested != 0U))
    {
        return;
    }

    if (BallTargetCapture_IsCaptured() != 0U)
    {
        if ((s_key4HoldReady == 0U) ||
            (BallSequence_IsActive() == 0U))
        {
            Serial1_SendString("ERR KEY4 BALL WAIT\r\n");
        }
        else if (Main_LineCanStart() == 0U)
        {
            Serial1_SendString("ERR KEY4 LINE BUSY\r\n");
        }
        else if (Main_StartTimedLineAtBallTarget(
                     BallTargetCapture_GetTargetMM(),
                     "ERR KEY4 LINE START\r\n") != 0U)
        {
            s_key4RunActive = 1U;
            Serial1_SendString("OK KEY4 RUN\r\n");
        }
        return;
    }

    if (BallTargetCapture_IsCapturing() != 0U)
    {
        Serial1_SendString("ERR KEY4 TARGET WAIT\r\n");
        return;
    }

    if (s_key4CaptureArmed != 0U)
    {
        s_key4CaptureArmed = 0U;
        BallTargetCapture_Start();
        Serial1_SendString("OK KEY4 CAPTURE\r\n");
        return;
    }

    if ((Main_LineCanStart() == 0U) ||
        (MotionManager_IsBusy() != 0U))
    {
        Serial1_SendString("ERR KEY4 BUSY\r\n");
        return;
    }

    s_defaultBallHoldPending = 0U;
    inheritedTiltDeg = BeamActuator_GetTiltDeg();
    BallSequence_Stop();
    BeamActuator_SetTiltDeg(inheritedTiltDeg);
    BallTargetCapture_Cancel();
    s_key4CaptureArmed = 1U;
    s_key4HoldReady = 0U;
    s_key4RunActive = 0U;
    Serial1_SendString("OK KEY4 ARMED\r\n");
}

static void Main_UpdateKey4Target(void)
{
    BallTargetCapture_Update();

    if ((s_key4RunActive == 0U) &&
        (BallTargetCapture_IsCaptured() != 0U) &&
        (s_key4HoldReady != 0U) &&
        (BallSequence_IsActive() == 0U))
    {
        s_key4HoldReady = 0U;
    }

    if ((BallTargetCapture_IsCaptured() != 0U) &&
        (s_key4HoldReady == 0U) &&
        (Main_BallHoldCanStart() != 0U))
    {
        s_ballTargetMM = BallTargetCapture_GetTargetMM();
        if (Main_StartOrRetargetBallTask(s_ballTargetMM) != 0U)
        {
            s_key4HoldReady = 1U;
            Serial1_SendString("OK KEY4 TARGET LOCKED\r\n");
        }
    }
}

int main(void)
{
    App_UpdateContext_t updateContext;

    App_Init();
    Accomplish26H_Init();
    TimedLineRun_Init();
    BallSequence_Init();
    BallTargetCapture_Init();
    s_ballTargetMM = BALL_SEQUENCE_DEFAULT_TARGET_MM;
    s_defaultBallHoldPending = 1U;
    s_key2SweepArmed = 0U;
    s_key4CaptureArmed = 0U;
    s_key4HoldReady = 0U;
    s_key4RunActive = 0U;
    Interrupt_Enable();

    for (;;)
    {
        if (App_Update(&updateContext) != 0U)
        {
            uint8_t emergencyStopRequested =
                Main_HasSignal(&updateContext, 0U);
            uint8_t ballStopRequested =
                Main_HasSignal(&updateContext, MAIN_BALL_STOP_SIGNAL);
            uint8_t ballSweepRequested =
                Main_HasSignal(&updateContext, MAIN_BALL_SWEEP_SIGNAL);
            uint8_t ballStartRequested =
                Main_HasSignal(&updateContext, MAIN_BALL_START_SIGNAL);
            uint8_t lineStartKeyPressed =
                ((updateContext.pressedEdges &
                  MAIN_LINE_START_KEY_MASK) != 0U) ? 1U : 0U;
            uint8_t timedLineStartKeyPressed =
                ((updateContext.pressedEdges &
                  MAIN_TIMED_LINE_START_KEY_MASK) != 0U) ? 1U : 0U;
            uint8_t ballStartKeyPressed =
                ((updateContext.pressedEdges &
                  MAIN_BALL_START_KEY_MASK) != 0U) ? 1U : 0U;
            uint8_t key4StartKeyPressed =
                ((updateContext.pressedEdges &
                  MAIN_KEY4_START_KEY_MASK) != 0U) ? 1U : 0U;
            uint8_t startAllowed =
                ((emergencyStopRequested == 0U) &&
                 ((updateContext.pressedKeys &
                   MAIN_EMERGENCY_CHORD_MASK) !=
                  MAIN_EMERGENCY_CHORD_MASK)) ? 1U : 0U;
            float requestedBallTargetMM;

            if (BluetoothDebug_TakeBallTargetMM(
                    &requestedBallTargetMM) != 0U)
            {
                Main_CancelKey2Preparation();
                Main_CancelKey4Preparation();
                s_ballTargetMM = requestedBallTargetMM;
                if (BallSequence_IsActive() != 0U)
                {
                    (void)BallSequence_SetTarget(s_ballTargetMM);
                }
            }

            if ((lineStartKeyPressed != 0U) &&
                (Main_LineCanStart() != 0U))
            {
                Main_CancelKey2Preparation();
                Main_CancelKey4Preparation();
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

            if ((timedLineStartKeyPressed != 0U) &&
                (Main_LineCanStart() != 0U))
            {
                Main_CancelKey2Preparation();
                Main_CancelKey4Preparation();
                s_ballTargetMM = BALL_SEQUENCE_DEFAULT_TARGET_MM;
                if ((startAllowed != 0U) &&
                    (ballStopRequested == 0U))
                {
                    (void)Main_StartTimedLineAtBallTarget(
                        BALL_SEQUENCE_DEFAULT_TARGET_MM,
                        "ERR KEY3 LINE START\r\n");
                }
            }

            if ((key4StartKeyPressed != 0U) &&
                (lineStartKeyPressed == 0U) &&
                (timedLineStartKeyPressed == 0U) &&
                (ballStartKeyPressed == 0U) &&
                (ballStartRequested == 0U) &&
                (ballStopRequested == 0U))
            {
                Main_CancelKey2Preparation();
                Main_HandleKey4Press(startAllowed, ballStopRequested);
            }

            TimedLineRun_Update(&updateContext);

            if ((s_key4RunActive != 0U) &&
                (TimedLineRun_IsActive() == 0U))
            {
                if (TimedLineRun_GetState() ==
                    TIMED_LINE_RUN_STATE_FINISHED)
                {
                    Serial1_SendString("OK KEY4 LINE DONE\r\n");
                }
                else
                {
                    Serial1_SendString("ERR KEY4 LINE STOP\r\n");
                }
                Main_ResetKey4Task();
            }

            if (ballStopRequested != 0U)
            {
                s_defaultBallHoldPending = 0U;
                Main_CancelKey2Preparation();
                Main_CancelKey4Preparation();
                BallSequence_Stop();
                Serial1_SendString("OK BALL STOP\r\n");
            }
            else if ((startAllowed != 0U) &&
                     (ballSweepRequested != 0U))
            {
                s_defaultBallHoldPending = 0U;
                Main_CancelKey2Preparation();
                Main_CancelKey4Preparation();
                (void)Main_StartBallSweep();
            }
            else if ((startAllowed != 0U) &&
                     (ballStartKeyPressed != 0U))
            {
                Main_HandleKey2Press(startAllowed, ballStopRequested);
            }
            else if ((startAllowed != 0U) &&
                     (ballStartRequested != 0U))
            {
                s_defaultBallHoldPending = 0U;
                Main_CancelKey2Preparation();
                Main_CancelKey4Preparation();
                (void)Main_StartBallTask(s_ballTargetMM);
            }

            if ((emergencyStopRequested == 0U) &&
                (ballStopRequested == 0U) &&
                (startAllowed != 0U))
            {
                Main_UpdateKey4Target();
            }

            if (emergencyStopRequested != 0U)
            {
                s_defaultBallHoldPending = 0U;
                Main_CancelKey2Preparation();
                Main_ResetKey4Task();
            }
            else if ((BallSequence_IsActive() != 0U) &&
                     ((lineStartKeyPressed != 0U) ||
                      (timedLineStartKeyPressed != 0U) ||
                      (key4StartKeyPressed != 0U)))
            {
                s_defaultBallHoldPending = 0U;
            }
            else if ((s_defaultBallHoldPending != 0U) &&
                     (startAllowed != 0U) &&
                     (Main_BallHoldCanStart() != 0U) &&
                     (Main_StartOrRetargetBallTask(
                          BALL_SEQUENCE_DEFAULT_TARGET_MM) != 0U))
            {
                s_defaultBallHoldPending = 0U;
            }

            BallSequence_Update(updateContext.dt);
            Telemetry_Update(updateContext.elapsedTicks, updateContext.pressedKeys);
            BeamActuator_Update(updateContext.dt);
            DebugDisplay_Update();
        }
    }
}
