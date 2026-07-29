#include "Hardware/Motor/Stepper.h"

#include "Hardware/Motor/EncoderStepper.h"
#include "ti_msp_dl_config.h"

#include <limits.h>
#include <stddef.h>

#ifndef STEPPER_DIR_POSITIVE_LEVEL_HIGH
#define STEPPER_DIR_POSITIVE_LEVEL_HIGH 1
#endif

#define STEPPER_CONTROL_HZ              100U
#define STEPPER_MAX_SEGMENT_STEPS       160U
#define STEPPER_PWM_FRAME_CLOCKS        4119U
#define STEPPER_PWM_HEADER_CLOCKS       16U
#define STEPPER_PWM_MAX_CODE            4095U
#define STEPPER_PWM_MIN_FREQUENCY_HZ    400U
#define STEPPER_PWM_MAX_FREQUENCY_HZ    1500U
#define STEPPER_PWM_STABLE_FRAMES       3U
#define STEPPER_PWM_TIMEOUT_TICKS       5U
#define STEPPER_ABS_CAPTURE_CLOCK_HZ     CPUCLK_FREQ

static volatile Stepper_State_t s_state;
static volatile bool s_enabled;
static volatile bool s_ready;
static volatile bool s_timerRunning;
static volatile bool s_stopRequested;
static volatile int32_t s_targetSteps;
static volatile int32_t s_emittedSteps;
static volatile int8_t s_direction;
static volatile uint32_t s_currentRateHz;
static volatile uint16_t s_segmentSteps;
static Stepper_Profile_t s_profile;

static int32_t s_referenceSteps;
static int32_t s_referenceEncoderCounts;

static volatile bool s_captureSynchronized;
static volatile bool s_captureSignalLost;
static volatile uint32_t s_capturePeriodTicks;
static volatile uint32_t s_captureHighTicks;
static volatile uint32_t s_captureSequence;

static uint32_t s_lastCaptureSequence;
static uint8_t s_pwmStableFrames;
static uint8_t s_pwmTimeoutTicks;
static bool s_pwmValid;
static uint16_t s_absoluteCode;
static float s_absoluteAngleDeg;

static int32_t Stepper_RoundDivideSigned(int64_t numerator, int32_t denominator)
{
    int64_t result;

    if (numerator >= 0)
    {
        result = (numerator + (denominator / 2)) / denominator;
    }
    else
    {
        result = (numerator - (denominator / 2)) / denominator;
    }

    if (result > INT32_MAX)
    {
        return INT32_MAX;
    }
    if (result < INT32_MIN)
    {
        return INT32_MIN;
    }
    return (int32_t)result;
}

static uint32_t Stepper_AbsoluteDifference(int32_t first, int32_t second)
{
    int64_t difference = (int64_t)first - (int64_t)second;

    if (difference < 0)
    {
        difference = -difference;
    }
    return (uint32_t)difference;
}

static bool Stepper_ProfileIsValid(const Stepper_Profile_t *profile)
{
    return (profile != NULL) &&
           (profile->startStepRateHz >= STEPPER_MIN_STEP_RATE_HZ) &&
           (profile->startStepRateHz <= STEPPER_MAX_STEP_RATE_HZ) &&
           (profile->maxStepRateHz >= profile->startStepRateHz) &&
           (profile->maxStepRateHz <= STEPPER_MAX_STEP_RATE_HZ) &&
           (profile->accelerationStepsPerSec2 > 0U);
}

static bool Stepper_DegreesToSteps(float degrees, int32_t *steps)
{
    float scaled;

    if (steps == NULL)
    {
        return false;
    }

    scaled = degrees *
             ((float)STEPPER_STEPS_PER_REVOLUTION / 360.0f);
    if (!(scaled >= (float)INT32_MIN) || !(scaled <= (float)INT32_MAX))
    {
        return false;
    }

    *steps = (scaled >= 0.0f) ?
        (int32_t)(scaled + 0.5f) : (int32_t)(scaled - 0.5f);
    return true;
}

static int32_t Stepper_StepsToEncoderCounts(int32_t steps)
{
    return Stepper_RoundDivideSigned(
        (int64_t)steps * STEPPER_ENCODER_COUNTS_PER_REVOLUTION,
        (int32_t)STEPPER_STEPS_PER_REVOLUTION);
}

static void Stepper_SetDirection(int8_t direction)
{
    bool high = (direction > 0);

#if STEPPER_DIR_POSITIVE_LEVEL_HIGH == 0
    high = !high;
#endif

    if (high)
    {
        DL_GPIO_setPins(
            BOARD_OUTPUTS_STEPPER_DIR_PORT, BOARD_OUTPUTS_STEPPER_DIR_PIN);
    }
    else
    {
        DL_GPIO_clearPins(
            BOARD_OUTPUTS_STEPPER_DIR_PORT, BOARD_OUTPUTS_STEPPER_DIR_PIN);
    }
}

static void Stepper_ForcePulseLow(void)
{
    DL_TimerA_setCCPOutputDisabledAdv(STEPPER_PULSE_INST,
        DL_TIMER_CCP0_DIS_OUT_ADV_FORCE_LOW);
}

static void Stepper_ReleasePulseOutput(void)
{
    DL_TimerA_setCCPOutputDisabledAdv(STEPPER_PULSE_INST,
        DL_TIMER_CCP0_DIS_OUT_ADV_SET_BY_OCTL);
}

static void Stepper_StopTimer(void)
{
    DL_TimerA_stopCounter(STEPPER_PULSE_INST);
    DL_TimerA_disableInterrupt(
        STEPPER_PULSE_INST,
        DL_TIMERA_INTERRUPT_REPC_EVENT |
        DL_TIMERA_INTERRUPT_ZERO_EVENT);
    DL_TimerA_clearInterruptStatus(
        STEPPER_PULSE_INST,
        DL_TIMERA_INTERRUPT_REPC_EVENT |
        DL_TIMERA_INTERRUPT_ZERO_EVENT);
    Stepper_ForcePulseLow();
    s_timerRunning = false;
}

static uint32_t Stepper_RateDelta(uint32_t rate, uint16_t segmentSteps)
{
    uint64_t delta =
        ((uint64_t)s_profile.accelerationStepsPerSec2 * segmentSteps) / rate;

    if (delta == 0U)
    {
        delta = 1U;
    }
    if (delta > UINT32_MAX)
    {
        delta = UINT32_MAX;
    }
    return (uint32_t)delta;
}

static uint32_t Stepper_BrakingDistance(uint32_t rate)
{
    uint64_t rateSquared;
    uint64_t startSquared;
    uint64_t denominator;

    if (rate <= s_profile.startStepRateHz)
    {
        return 0U;
    }

    rateSquared = (uint64_t)rate * rate;
    startSquared =
        (uint64_t)s_profile.startStepRateHz * s_profile.startStepRateHz;
    denominator =
        (uint64_t)2U * s_profile.accelerationStepsPerSec2;
    return (uint32_t)(
        (rateSquared - startSquared + denominator - 1U) / denominator);
}

static void Stepper_StartSegment(void)
{
    uint32_t remaining =
        Stepper_AbsoluteDifference(s_targetSteps, s_emittedSteps);
    uint32_t segmentSteps =
        (s_currentRateHz + (STEPPER_CONTROL_HZ / 2U)) /
        STEPPER_CONTROL_HZ;
    uint32_t periodTicks;
    uint32_t loadValue;

    if (segmentSteps == 0U)
    {
        segmentSteps = 1U;
    }
    if (segmentSteps > STEPPER_MAX_SEGMENT_STEPS)
    {
        segmentSteps = STEPPER_MAX_SEGMENT_STEPS;
    }
    if (segmentSteps > remaining)
    {
        segmentSteps = remaining;
    }

    if (segmentSteps == 0U)
    {
        Stepper_StopTimer();
        s_currentRateHz = 0U;
        s_state = s_enabled ? STEPPER_STATE_READY : STEPPER_STATE_DISABLED;
        return;
    }

    periodTicks =
        (STEPPER_PULSE_INST_CLK_FREQ + (s_currentRateHz / 2U)) /
        s_currentRateHz;
    if (periodTicks < 2U)
    {
        periodTicks = 2U;
    }
    if (periodTicks > 65536U)
    {
        periodTicks = 65536U;
    }
    loadValue = periodTicks - 1U;

    Stepper_StopTimer();
    DL_TimerA_setLoadValue(STEPPER_PULSE_INST, loadValue);
    DL_TimerA_setTimerCount(STEPPER_PULSE_INST, loadValue);
    DL_TimerA_setCaptureCompareValue(
        STEPPER_PULSE_INST, loadValue / 2U, GPIO_STEPPER_PULSE_C0_IDX);
    DL_TimerA_setRepeatCounter(
        STEPPER_PULSE_INST, (uint8_t)(segmentSteps - 1U));
    DL_TimerA_clearInterruptStatus(
        STEPPER_PULSE_INST,
        DL_TIMERA_INTERRUPT_REPC_EVENT |
        DL_TIMERA_INTERRUPT_ZERO_EVENT);
    if (segmentSteps == 1U)
    {
        DL_TimerA_enableInterrupt(
            STEPPER_PULSE_INST, DL_TIMERA_INTERRUPT_ZERO_EVENT);
    }
    else
    {
        DL_TimerA_enableInterrupt(
            STEPPER_PULSE_INST, DL_TIMERA_INTERRUPT_REPC_EVENT);
    }

    s_segmentSteps = (uint16_t)segmentSteps;
    Stepper_ReleasePulseOutput();
    s_timerRunning = true;
    DL_TimerA_startCounter(STEPPER_PULSE_INST);
}

static void Stepper_CompleteSegment(void)
{
    uint32_t remaining;
    uint32_t rateDelta;
    uint32_t brakingDistance;

    Stepper_StopTimer();
    s_emittedSteps += (int32_t)s_direction * (int32_t)s_segmentSteps;
    remaining = Stepper_AbsoluteDifference(s_targetSteps, s_emittedSteps);

    if (remaining == 0U)
    {
        s_currentRateHz = 0U;
        s_stopRequested = false;
        s_state = s_enabled ? STEPPER_STATE_READY : STEPPER_STATE_DISABLED;
        return;
    }

    rateDelta = Stepper_RateDelta(s_currentRateHz, s_segmentSteps);
    if (s_stopRequested)
    {
        s_state = STEPPER_STATE_STOPPING;
        if ((s_currentRateHz <= s_profile.startStepRateHz) ||
            ((s_currentRateHz - s_profile.startStepRateHz) <= rateDelta))
        {
            s_targetSteps = s_emittedSteps;
            s_currentRateHz = 0U;
            s_stopRequested = false;
            s_state = s_enabled ? STEPPER_STATE_READY : STEPPER_STATE_DISABLED;
            return;
        }
        s_currentRateHz -= rateDelta;
    }
    else
    {
        brakingDistance = Stepper_BrakingDistance(s_currentRateHz);
        if (remaining <= brakingDistance + s_segmentSteps)
        {
            s_state = STEPPER_STATE_STOPPING;
            if ((s_currentRateHz > s_profile.startStepRateHz) &&
                ((s_currentRateHz - s_profile.startStepRateHz) > rateDelta))
            {
                s_currentRateHz -= rateDelta;
            }
            else
            {
                s_currentRateHz = s_profile.startStepRateHz;
            }
        }
        else
        {
            s_state = STEPPER_STATE_MOVING;
            if (s_currentRateHz < s_profile.maxStepRateHz)
            {
                uint32_t nextRate = s_currentRateHz + rateDelta;

                if ((nextRate < s_currentRateHz) ||
                    (nextRate > s_profile.maxStepRateHz))
                {
                    nextRate = s_profile.maxStepRateHz;
                }
                s_currentRateHz = nextRate;
            }
        }
    }

    Stepper_StartSegment();
}

static uint16_t Stepper_GetPartialSegmentSteps(void)
{
    uint32_t completed;
    uint8_t repeatCounter;
    uint32_t timerCount;
    uint32_t compareValue;

    if ((!s_timerRunning) || (s_segmentSteps == 0U))
    {
        return 0U;
    }

    repeatCounter = DL_TimerA_getRepeatCounter(STEPPER_PULSE_INST);
    timerCount = DL_TimerA_getTimerCount(STEPPER_PULSE_INST);
    compareValue = DL_TimerA_getCaptureCompareValue(
        STEPPER_PULSE_INST, GPIO_STEPPER_PULSE_C0_IDX);

    completed =
        ((uint32_t)s_segmentSteps - 1U) - (uint32_t)repeatCounter;
    if (timerCount <= compareValue)
    {
        completed++;
    }
    if (completed > s_segmentSteps)
    {
        completed = s_segmentSteps;
    }
    return (uint16_t)completed;
}

static bool Stepper_DecodePwm(
    uint32_t highTicks, uint32_t periodTicks, uint16_t *code)
{
    uint32_t minimumPeriod;
    uint32_t maximumPeriod;
    uint64_t frameHighClocks;
    int32_t decoded;

    if ((code == NULL) || (periodTicks == 0U) ||
        (highTicks > periodTicks))
    {
        return false;
    }

    minimumPeriod =
        STEPPER_ABS_CAPTURE_CLOCK_HZ / STEPPER_PWM_MAX_FREQUENCY_HZ;
    maximumPeriod =
        (STEPPER_ABS_CAPTURE_CLOCK_HZ +
         STEPPER_PWM_MIN_FREQUENCY_HZ - 1U) /
        STEPPER_PWM_MIN_FREQUENCY_HZ;
    if ((periodTicks < minimumPeriod) || (periodTicks > maximumPeriod))
    {
        return false;
    }

    frameHighClocks =
        ((uint64_t)highTicks * STEPPER_PWM_FRAME_CLOCKS +
         (periodTicks / 2U)) /
        periodTicks;
    decoded = (int32_t)frameHighClocks - STEPPER_PWM_HEADER_CLOCKS;
    if (decoded < 0)
    {
        decoded = 0;
    }
    if (decoded > (int32_t)STEPPER_PWM_MAX_CODE)
    {
        decoded = STEPPER_PWM_MAX_CODE;
    }
    *code = (uint16_t)decoded;
    return true;
}

static void Stepper_SetInitialReference(uint16_t code)
{
    int32_t initialSteps = Stepper_RoundDivideSigned(
        (int64_t)code * STEPPER_STEPS_PER_REVOLUTION,
        (int32_t)STEPPER_ENCODER_COUNTS_PER_REVOLUTION);

    Encoder_SetStepperCount((int32_t)code);
    s_referenceEncoderCounts = (int32_t)code;
    s_referenceSteps = initialSteps;
    s_emittedSteps = initialSteps;
    s_targetSteps = initialSteps;
    s_ready = true;
    if (s_enabled)
    {
        s_state = STEPPER_STATE_READY;
    }
}

static Stepper_Result_t Stepper_StartMove(
    int32_t target, const Stepper_Profile_t *profile)
{
    uint32_t primask;

    if (Stepper_IsBusy())
    {
        return STEPPER_RESULT_BUSY;
    }
    if (!Stepper_ProfileIsValid(profile))
    {
        return STEPPER_RESULT_INVALID_ARGUMENT;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    if ((s_state == STEPPER_STATE_MOVING) ||
        (s_state == STEPPER_STATE_STOPPING))
    {
        __set_PRIMASK(primask);
        return STEPPER_RESULT_BUSY;
    }
    if (!s_enabled)
    {
        __set_PRIMASK(primask);
        return STEPPER_RESULT_DISABLED;
    }
    if (!s_ready)
    {
        __set_PRIMASK(primask);
        return STEPPER_RESULT_NOT_READY;
    }
    if (target == s_emittedSteps)
    {
        s_targetSteps = target;
        __set_PRIMASK(primask);
        return STEPPER_RESULT_OK;
    }

    s_profile = *profile;
    s_targetSteps = target;
    s_direction = (target > s_emittedSteps) ? 1 : -1;
    s_currentRateHz = profile->startStepRateHz;
    s_stopRequested = false;
    s_state = STEPPER_STATE_MOVING;
    Stepper_SetDirection(s_direction);
    Stepper_StartSegment();
    __set_PRIMASK(primask);
    return STEPPER_RESULT_OK;
}

void Stepper_Init(void)
{
    const uint32_t primask = __get_PRIMASK();

    __disable_irq();
    Encoder_InitStepper();
    s_state = STEPPER_STATE_DISABLED;
    s_enabled = false;
    s_ready = false;
    s_timerRunning = false;
    s_stopRequested = false;
    s_targetSteps = 0;
    s_emittedSteps = 0;
    s_direction = 1;
    s_currentRateHz = 0U;
    s_segmentSteps = 0U;
    s_referenceSteps = 0;
    s_referenceEncoderCounts = 0;

    s_captureSynchronized = false;
    s_captureSignalLost = false;
    s_capturePeriodTicks = 0U;
    s_captureHighTicks = 0U;
    s_captureSequence = 0U;
    s_lastCaptureSequence = 0U;
    s_pwmStableFrames = 0U;
    s_pwmTimeoutTicks = 0U;
    s_pwmValid = false;
    s_absoluteCode = 0U;
    s_absoluteAngleDeg = 0.0f;

    DL_GPIO_clearPins(
        BOARD_OUTPUTS_STEPPER_EN_PORT, BOARD_OUTPUTS_STEPPER_EN_PIN);
    Stepper_SetDirection(1);

    Stepper_StopTimer();
    DL_TimerA_setCaptureCompareAction(STEPPER_PULSE_INST,
        DL_TIMER_CC_LACT_CCP_LOW |
        DL_TIMER_CC_CDACT_CCP_HIGH |
        DL_TIMER_CC_ZACT_CCP_LOW,
        GPIO_STEPPER_PULSE_C0_IDX);
    DL_TimerA_clearInterruptStatus(
        STEPPER_PULSE_INST,
        DL_TIMERA_INTERRUPT_REPC_EVENT |
        DL_TIMERA_INTERRUPT_ZERO_EVENT);
    NVIC_ClearPendingIRQ(STEPPER_PULSE_INST_INT_IRQN);
    NVIC_EnableIRQ(STEPPER_PULSE_INST_INT_IRQN);

    DL_TimerG_setTimerCount(
        STEPPER_ABS_CAPTURE_INST, STEPPER_ABS_CAPTURE_INST_LOAD_VALUE);
    DL_TimerG_clearInterruptStatus(STEPPER_ABS_CAPTURE_INST,
        DL_TIMERG_INTERRUPT_CC1_DN_EVENT |
        DL_TIMERG_INTERRUPT_ZERO_EVENT);
    NVIC_ClearPendingIRQ(STEPPER_ABS_CAPTURE_INST_INT_IRQN);
    NVIC_EnableIRQ(STEPPER_ABS_CAPTURE_INST_INT_IRQN);
    DL_TimerG_startCounter(STEPPER_ABS_CAPTURE_INST);
    __set_PRIMASK(primask);
}

void Stepper_Update(uint8_t elapsedTicks)
{
    uint32_t sequence;
    uint32_t periodTicks;
    uint32_t highTicks;
    bool signalLost;
    uint32_t primask;

    primask = __get_PRIMASK();
    __disable_irq();
    sequence = s_captureSequence;
    periodTicks = s_capturePeriodTicks;
    highTicks = s_captureHighTicks;
    signalLost = s_captureSignalLost;
    s_captureSignalLost = false;
    __set_PRIMASK(primask);

    if (signalLost)
    {
        s_pwmValid = false;
        s_pwmStableFrames = 0U;
    }

    if (sequence != s_lastCaptureSequence)
    {
        uint32_t frameCount = sequence - s_lastCaptureSequence;
        uint16_t code;

        s_lastCaptureSequence = sequence;
        s_pwmTimeoutTicks = 0U;
        if (Stepper_DecodePwm(highTicks, periodTicks, &code))
        {
            uint32_t stableFrames = s_pwmStableFrames + frameCount;

            s_pwmStableFrames = (stableFrames >= STEPPER_PWM_STABLE_FRAMES) ?
                STEPPER_PWM_STABLE_FRAMES : (uint8_t)stableFrames;
            s_pwmValid = true;
            s_absoluteCode = code;
            s_absoluteAngleDeg =
                (float)code * (360.0f / 4096.0f);
            if ((!s_ready) &&
                (s_pwmStableFrames >= STEPPER_PWM_STABLE_FRAMES))
            {
                Stepper_SetInitialReference(code);
            }
        }
        else
        {
            s_pwmValid = false;
            s_pwmStableFrames = 0U;
        }
    }
    else if (elapsedTicks > 0U)
    {
        uint16_t timeout = (uint16_t)s_pwmTimeoutTicks + elapsedTicks;

        s_pwmTimeoutTicks = (timeout > UINT8_MAX) ?
            UINT8_MAX : (uint8_t)timeout;
        if (s_pwmTimeoutTicks >= STEPPER_PWM_TIMEOUT_TICKS)
        {
            s_pwmValid = false;
            s_pwmStableFrames = 0U;
        }
    }
}

Stepper_Result_t Stepper_Enable(bool enable)
{
    if (!enable)
    {
        Stepper_EmergencyStop();
        DL_GPIO_clearPins(
            BOARD_OUTPUTS_STEPPER_EN_PORT, BOARD_OUTPUTS_STEPPER_EN_PIN);
        s_enabled = false;
        s_state = STEPPER_STATE_DISABLED;
        return STEPPER_RESULT_OK;
    }

    DL_GPIO_setPins(
        BOARD_OUTPUTS_STEPPER_EN_PORT, BOARD_OUTPUTS_STEPPER_EN_PIN);
    s_enabled = true;
    s_state = s_ready ? STEPPER_STATE_READY : STEPPER_STATE_DISABLED;
    return STEPPER_RESULT_OK;
}

Stepper_Result_t Stepper_MoveBySteps(
    int32_t steps, const Stepper_Profile_t *profile)
{
    int64_t target = (int64_t)s_emittedSteps + steps;

    if (Stepper_IsBusy())
    {
        return STEPPER_RESULT_BUSY;
    }
    if ((target < INT32_MIN) || (target > INT32_MAX))
    {
        return STEPPER_RESULT_INVALID_ARGUMENT;
    }
    return Stepper_StartMove((int32_t)target, profile);
}

Stepper_Result_t Stepper_MoveToSteps(
    int32_t target, const Stepper_Profile_t *profile)
{
    return Stepper_StartMove(target, profile);
}

Stepper_Result_t Stepper_MoveByAngle(
    float degrees, const Stepper_Profile_t *profile)
{
    int32_t steps;

    if (Stepper_IsBusy())
    {
        return STEPPER_RESULT_BUSY;
    }
    if (!Stepper_DegreesToSteps(degrees, &steps))
    {
        return STEPPER_RESULT_INVALID_ARGUMENT;
    }
    return Stepper_MoveBySteps(steps, profile);
}

Stepper_Result_t Stepper_MoveToAngle(
    float degrees, const Stepper_Profile_t *profile)
{
    int32_t steps;

    if (Stepper_IsBusy())
    {
        return STEPPER_RESULT_BUSY;
    }
    if (!Stepper_DegreesToSteps(degrees, &steps))
    {
        return STEPPER_RESULT_INVALID_ARGUMENT;
    }
    return Stepper_MoveToSteps(steps, profile);
}

void Stepper_Stop(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    if ((s_state == STEPPER_STATE_MOVING) ||
        (s_state == STEPPER_STATE_STOPPING))
    {
        s_stopRequested = true;
        s_state = STEPPER_STATE_STOPPING;
    }
    __set_PRIMASK(primask);
}

void Stepper_EmergencyStop(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    if (s_timerRunning)
    {
        uint16_t partialSteps;

        DL_TimerA_stopCounter(STEPPER_PULSE_INST);
        partialSteps = Stepper_GetPartialSegmentSteps();
        Stepper_StopTimer();
        s_emittedSteps +=
            (int32_t)s_direction * (int32_t)partialSteps;
    }
    else
    {
        Stepper_StopTimer();
    }

    s_targetSteps = s_emittedSteps;
    s_currentRateHz = 0U;
    s_segmentSteps = 0U;
    s_stopRequested = false;
    s_state = s_enabled ? STEPPER_STATE_READY : STEPPER_STATE_DISABLED;
    __set_PRIMASK(primask);
}

Stepper_Result_t Stepper_SetCurrentPosition(float degrees)
{
    int32_t steps;
    int32_t encoderCounts;
    uint32_t primask;

    if (!Stepper_DegreesToSteps(degrees, &steps))
    {
        return STEPPER_RESULT_INVALID_ARGUMENT;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    if (!s_ready)
    {
        __set_PRIMASK(primask);
        return STEPPER_RESULT_NOT_READY;
    }
    if ((s_state == STEPPER_STATE_MOVING) ||
        (s_state == STEPPER_STATE_STOPPING))
    {
        __set_PRIMASK(primask);
        return STEPPER_RESULT_BUSY;
    }

    encoderCounts = Stepper_StepsToEncoderCounts(steps);
    Encoder_SetStepperCount(encoderCounts);
    s_referenceSteps = steps;
    s_referenceEncoderCounts = encoderCounts;
    s_emittedSteps = steps;
    s_targetSteps = steps;
    __set_PRIMASK(primask);
    return STEPPER_RESULT_OK;
}

bool Stepper_IsBusy(void)
{
    return (s_state == STEPPER_STATE_MOVING) ||
           (s_state == STEPPER_STATE_STOPPING);
}

void Stepper_GetStatus(Stepper_Status_t *status)
{
    int32_t emittedSteps;
    int32_t targetSteps;
    Stepper_State_t state;
    bool enabled;
    bool ready;
    uint32_t primask;
    int32_t encoderCounts;
    int32_t expectedCounts;

    if (status == NULL)
    {
        return;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    emittedSteps = s_emittedSteps;
    targetSteps = s_targetSteps;
    state = s_state;
    enabled = s_enabled;
    ready = s_ready;
    __set_PRIMASK(primask);

    encoderCounts = Encoder_GetStepperCount();
    expectedCounts = Stepper_RoundDivideSigned(
        (int64_t)s_referenceEncoderCounts +
            Stepper_RoundDivideSigned(
                ((int64_t)emittedSteps - s_referenceSteps) *
                    STEPPER_ENCODER_COUNTS_PER_REVOLUTION,
                (int32_t)STEPPER_STEPS_PER_REVOLUTION),
        1);

    status->enabled = enabled;
    status->ready = ready;
    status->busy =
        (state == STEPPER_STATE_MOVING) ||
        (state == STEPPER_STATE_STOPPING);
    status->pwmValid = s_pwmValid;
    status->targetSteps = targetSteps;
    status->emittedSteps = emittedSteps;
    status->encoderCounts = encoderCounts;
    status->trackingErrorCounts = Stepper_RoundDivideSigned(
        (int64_t)encoderCounts - expectedCounts, 1);
    status->absoluteCode = s_absoluteCode;
    status->absoluteAngleDeg = s_absoluteAngleDeg;
    status->multiTurnAngleDeg =
        (float)encoderCounts * (360.0f / 4096.0f);
    status->encoderTransitionErrors =
        Encoder_GetStepperTransitionErrors();
    status->state = state;
}

void STEPPER_PULSE_INST_IRQHandler(void)
{
    switch (DL_TimerA_getPendingInterrupt(STEPPER_PULSE_INST))
    {
        case DL_TIMER_IIDX_REPEAT_COUNT:
            if (s_segmentSteps > 1U)
            {
                Stepper_CompleteSegment();
            }
            break;

        case DL_TIMER_IIDX_ZERO:
            if (s_segmentSteps == 1U)
            {
                Stepper_CompleteSegment();
            }
            break;

        default:
            break;
    }
}

void STEPPER_ABS_CAPTURE_INST_IRQHandler(void)
{
    switch (DL_TimerG_getPendingInterrupt(STEPPER_ABS_CAPTURE_INST))
    {
        case DL_TIMER_IIDX_CC1_DN:
            if (s_captureSynchronized)
            {
                uint32_t periodCapture = DL_TimerG_getCaptureCompareValue(
                    STEPPER_ABS_CAPTURE_INST, DL_TIMER_CC_1_INDEX);
                uint32_t highCapture = DL_TimerG_getCaptureCompareValue(
                    STEPPER_ABS_CAPTURE_INST, DL_TIMER_CC_0_INDEX);

                s_capturePeriodTicks =
                    STEPPER_ABS_CAPTURE_INST_LOAD_VALUE - periodCapture;
                s_captureHighTicks =
                    STEPPER_ABS_CAPTURE_INST_LOAD_VALUE - highCapture;
                s_captureSequence++;
            }
            else
            {
                s_captureSynchronized = true;
            }

            /* TIMER_ERR_01: reload manually after each combined capture. */
            DL_TimerG_setTimerCount(STEPPER_ABS_CAPTURE_INST,
                STEPPER_ABS_CAPTURE_INST_LOAD_VALUE);
            break;

        case DL_TIMER_IIDX_ZERO:
            s_captureSynchronized = false;
            s_captureSignalLost = true;
            break;

        default:
            break;
    }
}
