#include "Application/Control/BallSequence.h"
#include "Application/Control/BallBalance.h"
#include "Application/Control/BallSensor.h"
#include "Application/Control/TaskTimer.h"
#include <math.h>

#define BALL_SEQUENCE_TICK_MS 10U
#define BALL_SEQUENCE_NEGATIVE_REVERSAL_POSITION_MM (-45.0f)
#define BALL_SEQUENCE_POSITIVE_CONFIRM_TICKS 4U
#define BALL_SEQUENCE_NEGATIVE_TIMEOUT_TICKS 2000U
#define BALL_SEQUENCE_POSITIVE_TIMEOUT_TICKS 2200U
#define BALL_SEQUENCE_TOTAL_TIMEOUT_TICKS 4500U
#define BALL_SEQUENCE_NEGATIVE_OVERSHOOT_POSITION_MM (-65.0f)
#define BALL_SEQUENCE_PHASE_STALL_TICKS 120U
#define BALL_SEQUENCE_PHASE_STALL_PROGRESS_MM 1.0f
#define BALL_SEQUENCE_PHASE_STALL_SPEED_MMPS 1.5f

static const BallBalance_MotionProfile_t s_positiveRecoveryProfile = {
    28.0f, 30.0f, 8.0f, 12.0f, 5.0f, 14.0f, 220.0f, 4.0f
};

float BallSequence_TunePositionKpPerS = BALL_SEQUENCE_POSITION_KP_PER_S;
float BallSequence_TuneVelocityKpDegPerMMps =
    BALL_SEQUENCE_VELOCITY_KP_DEG_PER_MMPS;
float BallSequence_TuneVelocityKiDegPerMM =
    BALL_SEQUENCE_VELOCITY_KI_DEG_PER_MM;
float BallSequence_TunePositivePositionKpPerS =
    BALL_SEQUENCE_POSITIVE_POSITION_KP_PER_S;
float BallSequence_TunePositiveVelocityKpDegPerMMps =
    BALL_SEQUENCE_POSITIVE_VELOCITY_KP_DEG_PER_MMPS;
float BallSequence_TunePositiveVelocityKiDegPerMM =
    BALL_SEQUENCE_POSITIVE_VELOCITY_KI_DEG_PER_MM;
float BallSequence_TuneNegativeMaxVelocityMMps =
    BALL_SEQUENCE_NEGATIVE_MAX_VELOCITY_MMPS;
float BallSequence_TuneNegativeApproachDistanceMM =
    BALL_SEQUENCE_NEGATIVE_APPROACH_DISTANCE_MM;
float BallSequence_TuneNegativeTerminalVelocityMMps =
    BALL_SEQUENCE_NEGATIVE_TERMINAL_VELOCITY_MMPS;
float BallSequence_TuneNegativeMaxTiltDeg =
    BALL_SEQUENCE_NEGATIVE_MAX_TILT_DEG;
float BallSequence_TuneNegativeIntegralLimitMM =
    BALL_SEQUENCE_NEGATIVE_INTEGRAL_LIMIT_MM;
float BallSequence_TunePositiveMaxVelocityMMps =
    BALL_SEQUENCE_POSITIVE_MAX_VELOCITY_MMPS;
float BallSequence_TunePositiveApproachDistanceMM =
    BALL_SEQUENCE_POSITIVE_APPROACH_DISTANCE_MM;
float BallSequence_TunePositiveTerminalVelocityMMps =
    BALL_SEQUENCE_POSITIVE_TERMINAL_VELOCITY_MMPS;
float BallSequence_TunePositiveMaxTiltDeg =
    BALL_SEQUENCE_POSITIVE_MAX_TILT_DEG;
float BallSequence_TunePositiveIntegralLimitMM =
    BALL_SEQUENCE_POSITIVE_INTEGRAL_LIMIT_MM;

typedef struct
{
    BallSequence_State_t state;
    BallSequence_Error_t error;
    uint32_t elapsedTicks;
    uint32_t phaseElapsedTicks;
    uint8_t confirmTicks;
    uint8_t recoveryCount;
    float targetMM;
    float positivePositionKpPerS;
    float positiveVelocityKpDegPerMMps;
    float positiveVelocityKiDegPerMM;
    float phaseStartPositionMM;
    BallBalance_MotionProfile_t negativeProfile;
    BallBalance_MotionProfile_t positiveProfile;
} BallSequence_Context_t;

static BallSequence_Context_t s_context;

static float BallSequence_TicksToMs(uint32_t ticks)
{
    return (float)ticks * (float)BALL_SEQUENCE_TICK_MS;
}

static uint8_t BallSequence_ApplyMotionProfile(
    const BallBalance_MotionProfile_t *profile)
{
    return (BallBalance_SetMotionProfile(profile) ==
            BALL_BALANCE_RESULT_OK) ? 1U : 0U;
}

static void BallSequence_ResetPhaseTracking(void)
{
    s_context.phaseElapsedTicks = 0U;
    s_context.confirmTicks = 0U;
    s_context.phaseStartPositionMM = BallBalance_GetPositionMM();
}

static uint8_t BallSequence_IsFreshStable(float positionMM,
                                          float speedMMps,
                                          float targetMM,
                                          float toleranceMM,
                                          float speedLimitMMps)
{
    return ((BallSensor_IsFresh() != 0U) &&
            (fabsf(positionMM - targetMM) <= toleranceMM) &&
            (fabsf(speedMMps) <= speedLimitMMps)) ? 1U : 0U;
}

static void BallSequence_Fail(BallSequence_Error_t error)
{
    BallBalance_Stop();
    TaskTimer_Stop(TASK_TIMER_OWNER_BALL);
    s_context.error = error;
    s_context.state = BALL_SEQUENCE_STATE_ERROR;
    s_context.phaseElapsedTicks = 0U;
    s_context.confirmTicks = 0U;
}

static uint8_t BallSequence_StartPositivePhase(uint8_t recovery)
{
    BallBalance_Result_t result;
    const BallBalance_MotionProfile_t *profile;

    result = BallBalance_StartWithGains(
        BALL_SEQUENCE_POSITIVE_TARGET_MM,
        s_context.positivePositionKpPerS,
        s_context.positiveVelocityKpDegPerMMps,
        s_context.positiveVelocityKiDegPerMM);
    if (result != BALL_BALANCE_RESULT_OK)
    {
        return 0U;
    }

    profile = (recovery != 0U) ? &s_positiveRecoveryProfile :
        &s_context.positiveProfile;
    if (BallSequence_ApplyMotionProfile(profile) == 0U)
    {
        return 0U;
    }

    s_context.targetMM = BALL_SEQUENCE_POSITIVE_TARGET_MM;
    s_context.state = BALL_SEQUENCE_STATE_SWEEP_TO_POSITIVE;
    s_context.recoveryCount = recovery;
    BallSequence_ResetPhaseTracking();
    return 1U;
}

void BallSequence_Init(void)
{
    s_context.state = BALL_SEQUENCE_STATE_READY;
    s_context.error = BALL_SEQUENCE_ERROR_NONE;
    s_context.elapsedTicks = 0U;
    s_context.phaseElapsedTicks = 0U;
    s_context.confirmTicks = 0U;
    s_context.recoveryCount = 0U;
    s_context.targetMM = BALL_SEQUENCE_DEFAULT_TARGET_MM;
    s_context.positivePositionKpPerS =
        BALL_SEQUENCE_POSITIVE_POSITION_KP_PER_S;
    s_context.positiveVelocityKpDegPerMMps =
        BALL_SEQUENCE_POSITIVE_VELOCITY_KP_DEG_PER_MMPS;
    s_context.positiveVelocityKiDegPerMM =
        BALL_SEQUENCE_POSITIVE_VELOCITY_KI_DEG_PER_MM;
    s_context.phaseStartPositionMM = 0.0f;
    BallSequence_TunePositionKpPerS =
        BALL_SEQUENCE_POSITION_KP_PER_S;
    BallSequence_TuneVelocityKpDegPerMMps =
        BALL_SEQUENCE_VELOCITY_KP_DEG_PER_MMPS;
    BallSequence_TuneVelocityKiDegPerMM =
        BALL_SEQUENCE_VELOCITY_KI_DEG_PER_MM;
    BallSequence_TunePositivePositionKpPerS =
        BALL_SEQUENCE_POSITIVE_POSITION_KP_PER_S;
    BallSequence_TunePositiveVelocityKpDegPerMMps =
        BALL_SEQUENCE_POSITIVE_VELOCITY_KP_DEG_PER_MMPS;
    BallSequence_TunePositiveVelocityKiDegPerMM =
        BALL_SEQUENCE_POSITIVE_VELOCITY_KI_DEG_PER_MM;
    BallSequence_TuneNegativeMaxVelocityMMps =
        BALL_SEQUENCE_NEGATIVE_MAX_VELOCITY_MMPS;
    BallSequence_TuneNegativeApproachDistanceMM =
        BALL_SEQUENCE_NEGATIVE_APPROACH_DISTANCE_MM;
    BallSequence_TuneNegativeTerminalVelocityMMps =
        BALL_SEQUENCE_NEGATIVE_TERMINAL_VELOCITY_MMPS;
    BallSequence_TuneNegativeMaxTiltDeg =
        BALL_SEQUENCE_NEGATIVE_MAX_TILT_DEG;
    BallSequence_TuneNegativeIntegralLimitMM =
        BALL_SEQUENCE_NEGATIVE_INTEGRAL_LIMIT_MM;
    BallSequence_TunePositiveMaxVelocityMMps =
        BALL_SEQUENCE_POSITIVE_MAX_VELOCITY_MMPS;
    BallSequence_TunePositiveApproachDistanceMM =
        BALL_SEQUENCE_POSITIVE_APPROACH_DISTANCE_MM;
    BallSequence_TunePositiveTerminalVelocityMMps =
        BALL_SEQUENCE_POSITIVE_TERMINAL_VELOCITY_MMPS;
    BallSequence_TunePositiveMaxTiltDeg =
        BALL_SEQUENCE_POSITIVE_MAX_TILT_DEG;
    BallSequence_TunePositiveIntegralLimitMM =
        BALL_SEQUENCE_POSITIVE_INTEGRAL_LIMIT_MM;
    BallBalance_Init();
}

uint8_t BallSequence_Start(float targetMM)
{
    if (BallBalance_Start(targetMM) != BALL_BALANCE_RESULT_OK)
    {
        s_context.error = BALL_SEQUENCE_ERROR_VISION;
        s_context.state = BALL_SEQUENCE_STATE_ERROR;
        return 0U;
    }

    s_context.elapsedTicks = 0U;
    s_context.phaseElapsedTicks = 0U;
    s_context.confirmTicks = 0U;
    s_context.recoveryCount = 0U;
    s_context.targetMM = targetMM;
    s_context.error = BALL_SEQUENCE_ERROR_NONE;
    s_context.state = BALL_SEQUENCE_STATE_HOLDING;
    return 1U;
}

uint8_t BallSequence_StartSweep(void)
{
    BallBalance_Result_t result;

    s_context.positivePositionKpPerS =
        BallSequence_TunePositivePositionKpPerS;
    s_context.positiveVelocityKpDegPerMMps =
        BallSequence_TunePositiveVelocityKpDegPerMMps;
    s_context.positiveVelocityKiDegPerMM =
        BallSequence_TunePositiveVelocityKiDegPerMM;
    s_context.negativeProfile = (BallBalance_MotionProfile_t) {
        BallSequence_TuneNegativeMaxVelocityMMps,
        BallSequence_TuneNegativeApproachDistanceMM,
        BallSequence_TuneNegativeTerminalVelocityMMps,
        4.0f, 6.0f,
        BallSequence_TuneNegativeMaxTiltDeg,
        BallSequence_TuneNegativeIntegralLimitMM,
        2.5f
    };
    s_context.positiveProfile = (BallBalance_MotionProfile_t) {
        BallSequence_TunePositiveMaxVelocityMMps,
        BallSequence_TunePositiveApproachDistanceMM,
        BallSequence_TunePositiveTerminalVelocityMMps,
        8.0f, 8.0f,
        BallSequence_TunePositiveMaxTiltDeg,
        BallSequence_TunePositiveIntegralLimitMM,
        3.5f
    };

    result = BallBalance_StartWithGains(
        BALL_SEQUENCE_NEGATIVE_TARGET_MM,
        BallSequence_TunePositionKpPerS,
        BallSequence_TuneVelocityKpDegPerMMps,
        BallSequence_TuneVelocityKiDegPerMM);
    if (result != BALL_BALANCE_RESULT_OK)
    {
        s_context.error = BALL_SEQUENCE_ERROR_VISION;
        s_context.state = BALL_SEQUENCE_STATE_ERROR;
        return 0U;
    }
    if (BallSequence_ApplyMotionProfile(&s_context.negativeProfile) == 0U)
    {
        BallBalance_Stop();
        s_context.error = BALL_SEQUENCE_ERROR_BALANCE;
        s_context.state = BALL_SEQUENCE_STATE_ERROR;
        return 0U;
    }

    s_context.elapsedTicks = 0U;
    s_context.phaseElapsedTicks = 0U;
    s_context.confirmTicks = 0U;
    s_context.recoveryCount = 0U;
    s_context.targetMM = BALL_SEQUENCE_NEGATIVE_TARGET_MM;
    s_context.error = BALL_SEQUENCE_ERROR_NONE;
    s_context.state = BALL_SEQUENCE_STATE_SWEEP_TO_NEGATIVE;
    s_context.phaseStartPositionMM = BallBalance_GetPositionMM();
    return 1U;
}

uint8_t BallSequence_SetTarget(float targetMM)
{
    BallSequence_State_t previousState = s_context.state;

    if ((BallSequence_IsActive() == 0U) ||
        (BallBalance_SetTarget(targetMM) != BALL_BALANCE_RESULT_OK))
    {
        return 0U;
    }

    s_context.targetMM = targetMM;
    s_context.state = BALL_SEQUENCE_STATE_HOLDING;
    s_context.confirmTicks = 0U;
    s_context.phaseElapsedTicks = 0U;
    if (previousState != BALL_SEQUENCE_STATE_HOLDING)
    {
        TaskTimer_Stop(TASK_TIMER_OWNER_BALL);
    }
    return 1U;
}

void BallSequence_Update(float dt)
{
    float positionMM;
    float speedMMps;

    if (BallSequence_IsActive() == 0U)
    {
        return;
    }

    BallBalance_Update(dt);
    if (BallBalance_GetState() == BALL_BALANCE_STATE_ERROR)
    {
        BallSequence_Fail(
            (BallBalance_GetError() == BALL_BALANCE_ERROR_VISION_LOST) ?
                BALL_SEQUENCE_ERROR_VISION :
                BALL_SEQUENCE_ERROR_BALANCE);
        return;
    }

    positionMM = BallBalance_GetPositionMM();
    speedMMps = BallSensor_GetSpeedMMps();

    if (s_context.elapsedTicks < UINT32_MAX)
    {
        s_context.elapsedTicks++;
    }
    if (s_context.phaseElapsedTicks < UINT32_MAX)
    {
        s_context.phaseElapsedTicks++;
    }

    if (s_context.elapsedTicks >= BALL_SEQUENCE_TOTAL_TIMEOUT_TICKS)
    {
        BallSequence_Fail(
            (s_context.state == BALL_SEQUENCE_STATE_SWEEP_TO_NEGATIVE) ?
                BALL_SEQUENCE_ERROR_TIMEOUT_NEGATIVE :
                BALL_SEQUENCE_ERROR_TIMEOUT_POSITIVE);
        return;
    }

    if (s_context.state == BALL_SEQUENCE_STATE_HOLDING)
    {
        return;
    }

    if (s_context.state == BALL_SEQUENCE_STATE_SWEEP_TO_NEGATIVE)
    {
        if (positionMM <= BALL_SEQUENCE_NEGATIVE_OVERSHOOT_POSITION_MM)
        {
            BallSequence_Fail(BALL_SEQUENCE_ERROR_OVERSHOOT);
            return;
        }
        if (s_context.phaseElapsedTicks >= BALL_SEQUENCE_NEGATIVE_TIMEOUT_TICKS)
        {
            BallSequence_Fail(BALL_SEQUENCE_ERROR_TIMEOUT_NEGATIVE);
            return;
        }

        /* The controller still aims at -50 mm, but phase 2 starts on the
         * first sample at or beyond -45 mm. */
        if (positionMM <= BALL_SEQUENCE_NEGATIVE_REVERSAL_POSITION_MM)
        {
            if (BallSequence_StartPositivePhase(0U) == 0U)
            {
                BallSequence_Fail(BALL_SEQUENCE_ERROR_BALANCE);
            }
            return;
        }

        if ((s_context.recoveryCount == 0U) &&
            (s_context.phaseElapsedTicks >= BALL_SEQUENCE_PHASE_STALL_TICKS) &&
            (fabsf(positionMM - s_context.phaseStartPositionMM) <=
             BALL_SEQUENCE_PHASE_STALL_PROGRESS_MM) &&
            (fabsf(BallBalance_GetVelocityTargetMMps()) >= 4.0f) &&
            (fabsf(speedMMps) <= BALL_SEQUENCE_PHASE_STALL_SPEED_MMPS))
        {
            if (BallSequence_ApplyMotionProfile(&s_positiveRecoveryProfile) == 0U)
            {
                s_context.recoveryCount = 1U;
                BallSequence_ResetPhaseTracking();
            }
        }
        else if ((s_context.recoveryCount != 0U) &&
                 (s_context.phaseElapsedTicks >= BALL_SEQUENCE_PHASE_STALL_TICKS) &&
                 (fabsf(positionMM - s_context.phaseStartPositionMM) <=
                  BALL_SEQUENCE_PHASE_STALL_PROGRESS_MM) &&
                 (fabsf(BallBalance_GetVelocityTargetMMps()) >= 4.0f) &&
                 (fabsf(speedMMps) <= BALL_SEQUENCE_PHASE_STALL_SPEED_MMPS))
        {
            BallSequence_Fail(BALL_SEQUENCE_ERROR_STALL);
            return;
        }
        return;
    }

    if (s_context.state == BALL_SEQUENCE_STATE_SWEEP_TO_POSITIVE)
    {
        if (s_context.phaseElapsedTicks >= BALL_SEQUENCE_POSITIVE_TIMEOUT_TICKS)
        {
            BallSequence_Fail(BALL_SEQUENCE_ERROR_TIMEOUT_POSITIVE);
            return;
        }

        if (BallSequence_IsFreshStable(
                positionMM, speedMMps, BALL_SEQUENCE_POSITIVE_TARGET_MM,
                3.0f, 3.5f) != 0U)
        {
            if (s_context.confirmTicks < UINT8_MAX)
            {
                s_context.confirmTicks++;
            }
        }
        else
        {
            s_context.confirmTicks = 0U;
        }

        if ((s_context.confirmTicks >=
             BALL_SEQUENCE_POSITIVE_CONFIRM_TICKS) &&
            (s_context.state == BALL_SEQUENCE_STATE_SWEEP_TO_POSITIVE))
        {
            s_context.state = BALL_SEQUENCE_STATE_SWEEP_HOLDING_POSITIVE;
            TaskTimer_Stop(TASK_TIMER_OWNER_BALL);
            s_context.phaseElapsedTicks = 0U;
            s_context.confirmTicks = 0U;
            return;
        }

        if ((s_context.recoveryCount == 0U) &&
            (s_context.phaseElapsedTicks >= BALL_SEQUENCE_PHASE_STALL_TICKS) &&
            (fabsf(positionMM - s_context.phaseStartPositionMM) <=
             BALL_SEQUENCE_PHASE_STALL_PROGRESS_MM) &&
            (fabsf(BallBalance_GetVelocityTargetMMps()) >= 4.0f) &&
            (fabsf(speedMMps) <= BALL_SEQUENCE_PHASE_STALL_SPEED_MMPS))
        {
            if (BallSequence_ApplyMotionProfile(&s_positiveRecoveryProfile) == 0U)
            {
                s_context.recoveryCount = 1U;
                BallSequence_ResetPhaseTracking();
            }
        }
        else if ((s_context.recoveryCount != 0U) &&
                 (s_context.phaseElapsedTicks >= BALL_SEQUENCE_PHASE_STALL_TICKS) &&
                 (fabsf(positionMM - s_context.phaseStartPositionMM) <=
                  BALL_SEQUENCE_PHASE_STALL_PROGRESS_MM) &&
                 (fabsf(BallBalance_GetVelocityTargetMMps()) >= 4.0f) &&
                 (fabsf(speedMMps) <= BALL_SEQUENCE_PHASE_STALL_SPEED_MMPS))
        {
            BallSequence_Fail(BALL_SEQUENCE_ERROR_STALL);
            return;
        }
        return;
    }
}

void BallSequence_Stop(void)
{
    BallBalance_Stop();
    TaskTimer_Stop(TASK_TIMER_OWNER_BALL);
    s_context.state = BALL_SEQUENCE_STATE_FINISHED;
    s_context.phaseElapsedTicks = 0U;
    s_context.confirmTicks = 0U;
}

BallSequence_State_t BallSequence_GetState(void)
{
    return s_context.state;
}

BallSequence_Error_t BallSequence_GetError(void)
{
    return s_context.error;
}

uint32_t BallSequence_GetElapsedTicks(void)
{
    return s_context.elapsedTicks;
}

uint32_t BallSequence_GetElapsedMilliseconds(void)
{
    return (uint32_t)BallSequence_TicksToMs(s_context.elapsedTicks);
}

float BallSequence_GetTargetMM(void)
{
    return s_context.targetMM;
}

uint8_t BallSequence_IsActive(void)
{
    return ((s_context.state == BALL_SEQUENCE_STATE_HOLDING) ||
            (s_context.state == BALL_SEQUENCE_STATE_SWEEP_TO_NEGATIVE) ||
            (s_context.state == BALL_SEQUENCE_STATE_SWEEP_TO_POSITIVE) ||
            (s_context.state ==
             BALL_SEQUENCE_STATE_SWEEP_HOLDING_POSITIVE)) ? 1U : 0U;
}

uint8_t BallSequence_IsStable(void)
{
    return ((s_context.state == BALL_SEQUENCE_STATE_HOLDING) &&
            (BallBalance_IsStable() != 0U)) ? 1U : 0U;
}

uint8_t BallSequence_GetTelemetryPhaseCode(void)
{
    if (s_context.state == BALL_SEQUENCE_STATE_SWEEP_TO_NEGATIVE)
    {
        return 1U;
    }
    if (s_context.state == BALL_SEQUENCE_STATE_SWEEP_TO_POSITIVE)
    {
        return 2U;
    }
    if (s_context.state == BALL_SEQUENCE_STATE_SWEEP_HOLDING_POSITIVE)
    {
        return 3U;
    }
    if (s_context.state == BALL_SEQUENCE_STATE_ERROR)
    {
        return 4U;
    }
    return 0U;
}

uint8_t BallSequence_GetTelemetryResultCode(void)
{
    if (s_context.state == BALL_SEQUENCE_STATE_SWEEP_HOLDING_POSITIVE)
    {
        return 1U;
    }
    if (s_context.state != BALL_SEQUENCE_STATE_ERROR)
    {
        return 0U;
    }

    switch (s_context.error)
    {
        case BALL_SEQUENCE_ERROR_VISION:
            return 2U;
        case BALL_SEQUENCE_ERROR_BALANCE:
            return 3U;
        case BALL_SEQUENCE_ERROR_TIMEOUT_NEGATIVE:
            return 4U;
        case BALL_SEQUENCE_ERROR_TIMEOUT_POSITIVE:
            return 5U;
        case BALL_SEQUENCE_ERROR_STALL:
            return 6U;
        case BALL_SEQUENCE_ERROR_OVERSHOOT:
            return 7U;
        default:
            return 0U;
    }
}
