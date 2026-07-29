#include "Hardware/Motor/Encoder.h"
#include "ti_msp_dl_config.h"

#define LEFT_ENCODER_SIGN  (-1)
#define RIGHT_ENCODER_SIGN (+1)

#ifndef STEPPER_ENCODER_SIGN
#define STEPPER_ENCODER_SIGN (+1)
#endif

static volatile int32_t s_leftCount;
static volatile int32_t s_rightCount;
static volatile int32_t s_stepperCount;
static volatile uint32_t s_stepperTransitionErrors;
static uint8_t s_leftState;
static uint8_t s_rightState;
static uint8_t s_stepperState;

static const int8_t s_quadratureDelta[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
};

static uint8_t Encoder_ReadState(
    GPIO_Regs *portA, uint32_t pinA, GPIO_Regs *portB, uint32_t pinB)
{
    uint32_t levelA = DL_GPIO_readPins(portA, pinA);
    uint32_t levelB = DL_GPIO_readPins(portB, pinB);

    return (uint8_t)(((levelA & pinA) != 0U ? 2U : 0U) |
                     ((levelB & pinB) != 0U ? 1U : 0U));
}

void Encoder_Init(void)
{
    const uint32_t interruptMask = ENCODER_INPUTS_LEFT_A_PIN | ENCODER_INPUTS_LEFT_B_PIN |
                                   ENCODER_INPUTS_RIGHT_A_PIN | ENCODER_INPUTS_RIGHT_B_PIN;

    s_leftCount = 0;
    s_rightCount = 0;
    s_leftState = Encoder_ReadState(
        ENCODER_INPUTS_PORT,
        ENCODER_INPUTS_LEFT_A_PIN,
        ENCODER_INPUTS_PORT,
        ENCODER_INPUTS_LEFT_B_PIN);
    s_rightState = Encoder_ReadState(
        ENCODER_INPUTS_PORT,
        ENCODER_INPUTS_RIGHT_A_PIN,
        ENCODER_INPUTS_PORT,
        ENCODER_INPUTS_RIGHT_B_PIN);
    DL_GPIO_clearInterruptStatus(ENCODER_INPUTS_PORT, interruptMask);
    DL_GPIO_enableInterrupt(ENCODER_INPUTS_PORT, interruptMask);
    NVIC_ClearPendingIRQ(GPIOA_INT_IRQn);
    NVIC_EnableIRQ(GPIOA_INT_IRQn);
    Encoder_InitStepper();
}

void Encoder_InitStepper(void)
{
    DL_GPIO_initDigitalInputFeatures(STEPPER_ENCODER_A_A_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(STEPPER_ENCODER_B_B_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_setLowerPinsPolarity(
        STEPPER_ENCODER_A_PORT, DL_GPIO_PIN_8_EDGE_RISE_FALL);
    DL_GPIO_setUpperPinsPolarity(
        STEPPER_ENCODER_B_PORT, DL_GPIO_PIN_25_EDGE_RISE_FALL);

    s_stepperCount = 0;
    s_stepperTransitionErrors = 0U;
    s_stepperState = Encoder_ReadState(
        STEPPER_ENCODER_A_PORT,
        STEPPER_ENCODER_A_A_PIN,
        STEPPER_ENCODER_B_PORT,
        STEPPER_ENCODER_B_B_PIN);
    DL_GPIO_clearInterruptStatus(
        STEPPER_ENCODER_A_PORT, STEPPER_ENCODER_A_A_PIN);
    DL_GPIO_clearInterruptStatus(
        STEPPER_ENCODER_B_PORT, STEPPER_ENCODER_B_B_PIN);
    DL_GPIO_enableInterrupt(
        STEPPER_ENCODER_A_PORT, STEPPER_ENCODER_A_A_PIN);
    DL_GPIO_enableInterrupt(
        STEPPER_ENCODER_B_PORT, STEPPER_ENCODER_B_B_PIN);
    NVIC_ClearPendingIRQ(GPIOA_INT_IRQn);
    NVIC_EnableIRQ(GPIOA_INT_IRQn);
}

int16_t Encoder_Get(uint8_t n)
{
    int32_t value = 0;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    if (n == 1U) { value = s_leftCount; s_leftCount = 0; }
    else if (n == 2U) { value = s_rightCount; s_rightCount = 0; }
    __set_PRIMASK(primask);
    if (value > 32767) value = 32767;
    if (value < -32768) value = -32768;
    return (int16_t)value;
}

int32_t Encoder_GetStepperCount(void)
{
    int32_t count;
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    count = s_stepperCount;
    __set_PRIMASK(primask);
    return count;
}

void Encoder_SetStepperCount(int32_t count)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    s_stepperCount = count;
    __set_PRIMASK(primask);
}

uint32_t Encoder_GetStepperTransitionErrors(void)
{
    uint32_t errors;
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    errors = s_stepperTransitionErrors;
    __set_PRIMASK(primask);
    return errors;
}

void GROUP1_IRQHandler(void)
{
    const uint32_t wheelMask =
        ENCODER_INPUTS_LEFT_A_PIN | ENCODER_INPUTS_LEFT_B_PIN |
        ENCODER_INPUTS_RIGHT_A_PIN | ENCODER_INPUTS_RIGHT_B_PIN;
    uint32_t wheelPending = DL_GPIO_getEnabledInterruptStatus(
        ENCODER_INPUTS_PORT, wheelMask);
    uint32_t stepperAPending = DL_GPIO_getEnabledInterruptStatus(
        STEPPER_ENCODER_A_PORT, STEPPER_ENCODER_A_A_PIN);
    uint32_t stepperBPending = DL_GPIO_getEnabledInterruptStatus(
        STEPPER_ENCODER_B_PORT, STEPPER_ENCODER_B_B_PIN);

    if ((wheelPending &
         (ENCODER_INPUTS_LEFT_A_PIN | ENCODER_INPUTS_LEFT_B_PIN)) != 0U)
    {
        uint8_t next = Encoder_ReadState(
            ENCODER_INPUTS_PORT,
            ENCODER_INPUTS_LEFT_A_PIN,
            ENCODER_INPUTS_PORT,
            ENCODER_INPUTS_LEFT_B_PIN);
        s_leftCount += (int32_t)(LEFT_ENCODER_SIGN * s_quadratureDelta[(s_leftState << 2) | next]);
        s_leftState = next;
    }
    if ((wheelPending &
         (ENCODER_INPUTS_RIGHT_A_PIN | ENCODER_INPUTS_RIGHT_B_PIN)) != 0U)
    {
        uint8_t next = Encoder_ReadState(
            ENCODER_INPUTS_PORT,
            ENCODER_INPUTS_RIGHT_A_PIN,
            ENCODER_INPUTS_PORT,
            ENCODER_INPUTS_RIGHT_B_PIN);
        s_rightCount += (int32_t)(
            RIGHT_ENCODER_SIGN *
            s_quadratureDelta[(s_rightState << 2) | next]);
        s_rightState = next;
    }
    if ((stepperAPending | stepperBPending) != 0U)
    {
        uint8_t next = Encoder_ReadState(
            STEPPER_ENCODER_A_PORT,
            STEPPER_ENCODER_A_A_PIN,
            STEPPER_ENCODER_B_PORT,
            STEPPER_ENCODER_B_B_PIN);
        uint8_t transition = (uint8_t)((s_stepperState << 2) | next);

        if ((uint8_t)(s_stepperState ^ next) == 3U)
        {
            s_stepperTransitionErrors++;
        }
        else
        {
            s_stepperCount +=
                (int32_t)(STEPPER_ENCODER_SIGN * s_quadratureDelta[transition]);
        }
        s_stepperState = next;
    }
    DL_GPIO_clearInterruptStatus(
        ENCODER_INPUTS_PORT, wheelPending & wheelMask);
    DL_GPIO_clearInterruptStatus(
        STEPPER_ENCODER_A_PORT,
        stepperAPending & STEPPER_ENCODER_A_A_PIN);
    DL_GPIO_clearInterruptStatus(
        STEPPER_ENCODER_B_PORT,
        stepperBPending & STEPPER_ENCODER_B_B_PIN);
}
