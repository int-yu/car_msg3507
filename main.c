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

#define MAIN_SELECT_KEY_MASK          0x01U
#define MAIN_CONFIRM_KEY_MASK         0x02U
#define MAIN_RETURN_KEY_MASK          0x04U
#define MAIN_EMERGENCY_CHORD_MASK     (0x01U | 0x02U)
#define MAIN_BALL_SWEEP_SIGNAL 2U
#define MAIN_BALL_START_SIGNAL 3U
#define MAIN_BALL_STOP_SIGNAL  4U
#define MAIN_REQUIREMENT_FIRST        2U
#define MAIN_REQUIREMENT_LAST         6U
#define MAIN_REQUIREMENT_4_SPEED_MMPS 500.0f
#define MAIN_REQUIREMENT_4_TIME_S      13.0f
#define MAIN_REQUIREMENT_5_SPEED_MMPS 400.0f
#define MAIN_REQUIREMENT_5_TIME_S      30.0f

typedef enum
{
    MAIN_UI_MENU = 0,
    MAIN_UI_REQUIREMENT_3_ARMED,
    MAIN_UI_REQUIREMENT_4_ARMED,
    MAIN_UI_REQUIREMENT_6_CAPTURE_ARMED,
    MAIN_UI_REQUIREMENT_6_CAPTURING,
    MAIN_UI_REQUIREMENT_6_READY,
    MAIN_UI_RUNNING_REQUIREMENT_2,
    MAIN_UI_RUNNING_REQUIREMENT_3,
    MAIN_UI_RUNNING_REQUIREMENT_4,
    MAIN_UI_RUNNING_REQUIREMENT_5,
    MAIN_UI_RUNNING_REQUIREMENT_6
} Main_UiState_t;

static float s_ballTargetMM = BALL_SEQUENCE_DEFAULT_TARGET_MM;
static uint8_t s_defaultBallHoldPending;
static uint8_t s_selectedRequirement;
static uint8_t s_requirement6HoldReady;
static Main_UiState_t s_uiState;

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

static void Main_ReturnToMenu(void)
{
    Accomplish26H_Cancel();
    TimedLineRun_Cancel();
    BallTargetCapture_Cancel();
    BallSequence_Stop();
    TaskTimer_Stop(TASK_TIMER_OWNER_BALL);
    s_ballTargetMM = BALL_SEQUENCE_DEFAULT_TARGET_MM;
    s_defaultBallHoldPending = 1U;
    s_requirement6HoldReady = 0U;
    s_uiState = MAIN_UI_MENU;
    DebugDisplay_ShowMenu(s_selectedRequirement);
    Serial1_SendString("OK MENU\r\n");
}

static uint8_t Main_StartTimedLine(float targetMM, float speedMMps,
                                   float durationSeconds,
                                   const char *errorMessage)
{
    float savedSpeed = TimedLineRun_TuneCruiseSpeedMMps;
    float savedDuration = TimedLineRun_TuneDurationSeconds;
    uint8_t result;

    s_ballTargetMM = targetMM;
    if (Main_StartOrRetargetBallTask(targetMM) == 0U)
    {
        return 0U;
    }

    TimedLineRun_TuneCruiseSpeedMMps = speedMMps;
    TimedLineRun_TuneDurationSeconds = durationSeconds;
    result = TimedLineRun_Start();
    TimedLineRun_TuneCruiseSpeedMMps = savedSpeed;
    TimedLineRun_TuneDurationSeconds = savedDuration;
    if (result == 0U)
    {
        BallSequence_Stop();
        Serial1_SendString(errorMessage);
        return 0U;
    }
    return 1U;
}

static uint8_t Main_StartRequirement2(
    const App_UpdateContext_t *context)
{
    App_UpdateContext_t lineContext = *context;

    if ((Main_LineCanStart() == 0U) ||
        (Main_StartOrRetargetBallTask(
             BALL_SEQUENCE_DEFAULT_TARGET_MM) == 0U))
    {
        Serial1_SendString("ERR REQ2 START\r\n");
        return 0U;
    }

    lineContext.pressedEdges = MAIN_SELECT_KEY_MASK;
    lineContext.pressedKeys &= (uint8_t)~MAIN_CONFIRM_KEY_MASK;
    Accomplish26H_Update(&lineContext);
    if (Accomplish26H_IsTiming() == 0U)
    {
        Serial1_SendString("ERR REQ2 LINE START\r\n");
        return 0U;
    }

    s_defaultBallHoldPending = 0U;
    s_uiState = MAIN_UI_RUNNING_REQUIREMENT_2;
    DebugDisplay_ShowRunning(2U);
    Serial1_SendString("OK REQ2 RUN\r\n");
    return 1U;
}

static void Main_ArmRequirement3(void)
{
    float inheritedTiltDeg = BeamActuator_GetTiltDeg();

    s_defaultBallHoldPending = 0U;
    BallSequence_Stop();
    BeamActuator_SetTiltDeg(inheritedTiltDeg);
    s_uiState = MAIN_UI_REQUIREMENT_3_ARMED;
    DebugDisplay_ShowPrompt(3U, "KEY2 START");
    Serial1_SendString("OK REQ3 ARMED\r\n");
}

static void Main_ArmRequirement6(void)
{
    float inheritedTiltDeg = BeamActuator_GetTiltDeg();

    s_defaultBallHoldPending = 0U;
    BallSequence_Stop();
    BeamActuator_SetTiltDeg(inheritedTiltDeg);
    BallTargetCapture_Cancel();
    s_requirement6HoldReady = 0U;
    s_uiState = MAIN_UI_REQUIREMENT_6_CAPTURE_ARMED;
    DebugDisplay_ShowPrompt(6U, "KEY2 SCAN");
    Serial1_SendString("OK REQ6 ARMED\r\n");
}

static void Main_HandleMenuConfirm(const App_UpdateContext_t *context)
{
    switch (s_selectedRequirement)
    {
        case 2U:
            (void)Main_StartRequirement2(context);
            break;

        case 3U:
            Main_ArmRequirement3();
            break;

        case 4U:
            s_uiState = MAIN_UI_REQUIREMENT_4_ARMED;
            DebugDisplay_ShowPrompt(4U, "KEY2 START");
            Serial1_SendString("OK REQ4 ARMED\r\n");
            break;

        case 5U:
            if ((Main_LineCanStart() != 0U) &&
                (Main_StartTimedLine(BALL_SEQUENCE_DEFAULT_TARGET_MM,
                    MAIN_REQUIREMENT_5_SPEED_MMPS,
                    MAIN_REQUIREMENT_5_TIME_S,
                    "ERR REQ5 START\r\n") != 0U))
            {
                s_defaultBallHoldPending = 0U;
                s_uiState = MAIN_UI_RUNNING_REQUIREMENT_5;
                DebugDisplay_ShowRunning(5U);
                Serial1_SendString("OK REQ5 RUN\r\n");
            }
            break;

        case 6U:
            Main_ArmRequirement6();
            break;

        default:
            break;
    }
}

static void Main_HandleConfirm(const App_UpdateContext_t *context)
{
    if (s_uiState == MAIN_UI_MENU)
    {
        Main_HandleMenuConfirm(context);
    }
    else if (s_uiState == MAIN_UI_REQUIREMENT_3_ARMED)
    {
        if (Main_StartBallSweep() != 0U)
        {
            s_uiState = MAIN_UI_RUNNING_REQUIREMENT_3;
            DebugDisplay_ShowRunning(3U);
        }
    }
    else if (s_uiState == MAIN_UI_REQUIREMENT_4_ARMED)
    {
        if ((Main_LineCanStart() != 0U) &&
            (Main_StartTimedLine(BALL_SEQUENCE_DEFAULT_TARGET_MM,
                MAIN_REQUIREMENT_4_SPEED_MMPS,
                MAIN_REQUIREMENT_4_TIME_S,
                "ERR REQ4 START\r\n") != 0U))
        {
            s_defaultBallHoldPending = 0U;
            s_uiState = MAIN_UI_RUNNING_REQUIREMENT_4;
            DebugDisplay_ShowRunning(4U);
            Serial1_SendString("OK REQ4 RUN\r\n");
        }
    }
    else if (s_uiState == MAIN_UI_REQUIREMENT_6_CAPTURE_ARMED)
    {
        BallTargetCapture_Start();
        s_uiState = MAIN_UI_REQUIREMENT_6_CAPTURING;
        DebugDisplay_ShowPrompt(6U, "SCANNING");
        Serial1_SendString("OK REQ6 CAPTURE\r\n");
    }
    else if (s_uiState == MAIN_UI_REQUIREMENT_6_READY)
    {
        if ((Main_LineCanStart() != 0U) &&
            (Main_StartTimedLine(BallTargetCapture_GetTargetMM(),
                MAIN_REQUIREMENT_5_SPEED_MMPS,
                MAIN_REQUIREMENT_5_TIME_S,
                "ERR REQ6 START\r\n") != 0U))
        {
            s_uiState = MAIN_UI_RUNNING_REQUIREMENT_6;
            DebugDisplay_ShowRunning(6U);
            Serial1_SendString("OK REQ6 RUN\r\n");
        }
    }
}

static void Main_UpdateRequirement6Capture(void)
{
    if ((s_uiState != MAIN_UI_REQUIREMENT_6_CAPTURING) &&
        (s_uiState != MAIN_UI_REQUIREMENT_6_READY))
    {
        return;
    }

    BallTargetCapture_Update();
    if ((BallTargetCapture_IsCaptured() != 0U) &&
        (s_requirement6HoldReady == 0U) &&
        (Main_BallHoldCanStart() != 0U))
    {
        s_ballTargetMM = BallTargetCapture_GetTargetMM();
        if (Main_StartOrRetargetBallTask(s_ballTargetMM) != 0U)
        {
            s_requirement6HoldReady = 1U;
            s_uiState = MAIN_UI_REQUIREMENT_6_READY;
            DebugDisplay_ShowPrompt(6U, "KEY2 RUN");
            Serial1_SendString("OK REQ6 TARGET LOCKED\r\n");
        }
    }
}

static void Main_UpdateTaskCompletion(void)
{
    if ((s_uiState == MAIN_UI_RUNNING_REQUIREMENT_2) &&
        ((Accomplish26H_GetState() == ACCOMPLISH_26H_STATE_FINISHED) ||
         (Accomplish26H_GetState() == ACCOMPLISH_26H_STATE_ERROR)))
    {
        Main_ReturnToMenu();
    }
    else if ((s_uiState == MAIN_UI_RUNNING_REQUIREMENT_3) &&
             ((BallSequence_GetState() ==
               BALL_SEQUENCE_STATE_SWEEP_HOLDING_POSITIVE) ||
              (BallSequence_GetState() == BALL_SEQUENCE_STATE_ERROR)))
    {
        Main_ReturnToMenu();
    }
    else if (((s_uiState == MAIN_UI_RUNNING_REQUIREMENT_4) ||
              (s_uiState == MAIN_UI_RUNNING_REQUIREMENT_5) ||
              (s_uiState == MAIN_UI_RUNNING_REQUIREMENT_6)) &&
             (TimedLineRun_IsActive() == 0U))
    {
        Main_ReturnToMenu();
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
    s_selectedRequirement = MAIN_REQUIREMENT_FIRST;
    s_requirement6HoldReady = 0U;
    s_uiState = MAIN_UI_MENU;
    DebugDisplay_ShowMenu(s_selectedRequirement);
    Interrupt_Enable();

    for (;;)
    {
        if (App_Update(&updateContext) != 0U)
        {
            App_UpdateContext_t lineContext = updateContext;
            uint8_t emergencyStopRequested =
                Main_HasSignal(&updateContext, 0U);
            uint8_t ballStopRequested =
                Main_HasSignal(&updateContext, MAIN_BALL_STOP_SIGNAL);
            uint8_t selectPressed =
                ((updateContext.pressedEdges & MAIN_SELECT_KEY_MASK) != 0U);
            uint8_t confirmPressed =
                ((updateContext.pressedEdges & MAIN_CONFIRM_KEY_MASK) != 0U);
            uint8_t returnPressed =
                ((updateContext.pressedEdges & MAIN_RETURN_KEY_MASK) != 0U);
            uint8_t startAllowed =
                ((emergencyStopRequested == 0U) &&
                 ((updateContext.pressedKeys & MAIN_EMERGENCY_CHORD_MASK) !=
                  MAIN_EMERGENCY_CHORD_MASK)) ? 1U : 0U;
            float requestedBallTargetMM;

            /* Menu KEY1 must never leak into the requirement-2 controller. */
            lineContext.pressedEdges &= (uint8_t)~MAIN_SELECT_KEY_MASK;
            Accomplish26H_Update(&lineContext);
            TimedLineRun_Update(&updateContext);

            if (returnPressed != 0U)
            {
                Main_ReturnToMenu();
            }
            else if (emergencyStopRequested != 0U)
            {
                s_defaultBallHoldPending = 0U;
                BallTargetCapture_Cancel();
                s_uiState = MAIN_UI_MENU;
                DebugDisplay_ShowMenu(s_selectedRequirement);
            }
            else if ((s_uiState == MAIN_UI_MENU) &&
                     (selectPressed != 0U) && (startAllowed != 0U))
            {
                s_selectedRequirement++;
                if (s_selectedRequirement > MAIN_REQUIREMENT_LAST)
                {
                    s_selectedRequirement = MAIN_REQUIREMENT_FIRST;
                }
                DebugDisplay_ShowMenu(s_selectedRequirement);
            }
            else if ((confirmPressed != 0U) && (startAllowed != 0U) &&
                     (ballStopRequested == 0U))
            {
                Main_HandleConfirm(&updateContext);
            }

            if (BluetoothDebug_TakeBallTargetMM(
                    &requestedBallTargetMM) != 0U)
            {
                s_ballTargetMM = requestedBallTargetMM;
                if (BallSequence_IsActive() != 0U)
                {
                    (void)BallSequence_SetTarget(s_ballTargetMM);
                }
            }

            if (ballStopRequested != 0U)
            {
                s_defaultBallHoldPending = 0U;
                BallSequence_Stop();
                Serial1_SendString("OK BALL STOP\r\n");
            }
            else if ((startAllowed != 0U) &&
                     (Main_HasSignal(&updateContext,
                                     MAIN_BALL_SWEEP_SIGNAL) != 0U))
            {
                s_defaultBallHoldPending = 0U;
                (void)Main_StartBallSweep();
            }
            else if ((startAllowed != 0U) &&
                     (Main_HasSignal(&updateContext,
                                     MAIN_BALL_START_SIGNAL) != 0U))
            {
                s_defaultBallHoldPending = 0U;
                (void)Main_StartBallTask(s_ballTargetMM);
            }

            Main_UpdateRequirement6Capture();
            BallSequence_Update(updateContext.dt);
            Main_UpdateTaskCompletion();

            if ((s_defaultBallHoldPending != 0U) &&
                (startAllowed != 0U) &&
                (Main_BallHoldCanStart() != 0U) &&
                (Main_StartOrRetargetBallTask(
                     BALL_SEQUENCE_DEFAULT_TARGET_MM) != 0U))
            {
                s_defaultBallHoldPending = 0U;
            }

            Telemetry_Update(updateContext.elapsedTicks, updateContext.pressedKeys);
            BeamActuator_Update(updateContext.dt);
            DebugDisplay_Update();
        }
    }
}
