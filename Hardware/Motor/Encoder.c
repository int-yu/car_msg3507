#include "Hardware/Motor/Encoder.h"
#include "ti_msp_dl_config.h"

#define LEFT_ENCODER_SIGN  (-1)
#define RIGHT_ENCODER_SIGN (+1)

static volatile int32_t s_leftCount;
static volatile int32_t s_rightCount;
static uint8_t s_leftState;
static uint8_t s_rightState;

static const int8_t s_quadratureDelta[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
};

static uint8_t Encoder_ReadState(uint32_t pinA, uint32_t pinB)
{
    uint32_t pins = DL_GPIO_readPins(ENCODER_INPUTS_PORT, pinA | pinB);
    return (uint8_t)(((pins & pinA) != 0U ? 2U : 0U) | ((pins & pinB) != 0U ? 1U : 0U));
}

void Encoder_Init(void)
{
    const uint32_t interruptMask = ENCODER_INPUTS_LEFT_A_PIN | ENCODER_INPUTS_LEFT_B_PIN |
                                   ENCODER_INPUTS_RIGHT_A_PIN | ENCODER_INPUTS_RIGHT_B_PIN;

    s_leftCount = 0;
    s_rightCount = 0;
    s_leftState = Encoder_ReadState(ENCODER_INPUTS_LEFT_A_PIN, ENCODER_INPUTS_LEFT_B_PIN);
    s_rightState = Encoder_ReadState(ENCODER_INPUTS_RIGHT_A_PIN, ENCODER_INPUTS_RIGHT_B_PIN);
    DL_GPIO_clearInterruptStatus(ENCODER_INPUTS_PORT, interruptMask);
    DL_GPIO_enableInterrupt(ENCODER_INPUTS_PORT, interruptMask);
    NVIC_ClearPendingIRQ(ENCODER_INPUTS_INT_IRQN);
    NVIC_EnableIRQ(ENCODER_INPUTS_INT_IRQN);
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

void GROUP1_IRQHandler(void)
{
    const uint32_t mask = ENCODER_INPUTS_LEFT_A_PIN | ENCODER_INPUTS_LEFT_B_PIN |
                          ENCODER_INPUTS_RIGHT_A_PIN | ENCODER_INPUTS_RIGHT_B_PIN;
    uint32_t pending = DL_GPIO_getEnabledInterruptStatus(ENCODER_INPUTS_PORT, mask);

    if ((pending & (ENCODER_INPUTS_LEFT_A_PIN | ENCODER_INPUTS_LEFT_B_PIN)) != 0U)
    {
        uint8_t next = Encoder_ReadState(ENCODER_INPUTS_LEFT_A_PIN, ENCODER_INPUTS_LEFT_B_PIN);
        s_leftCount += (int32_t)(LEFT_ENCODER_SIGN * s_quadratureDelta[(s_leftState << 2) | next]);
        s_leftState = next;
    }
    if ((pending & (ENCODER_INPUTS_RIGHT_A_PIN | ENCODER_INPUTS_RIGHT_B_PIN)) != 0U)
    {
        uint8_t next = Encoder_ReadState(ENCODER_INPUTS_RIGHT_A_PIN, ENCODER_INPUTS_RIGHT_B_PIN);
        s_rightCount += (int32_t)(
            RIGHT_ENCODER_SIGN *
            s_quadratureDelta[(s_rightState << 2) | next]);
        s_rightState = next;
    }
    DL_GPIO_clearInterruptStatus(ENCODER_INPUTS_PORT, pending & mask);
}
