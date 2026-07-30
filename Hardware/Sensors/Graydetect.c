#include "Hardware/Sensors/Graydetect.h"
#include "ti_msp_dl_config.h"

#if GRAYDETECT_ENABLED

#define GRAY_MASK_ALL ((uint8_t)((1U << GRAY_CHANNEL_COUNT) - 1U))

#if CAR_IS_MASTER
#define GRAY_MASK_LEFT  0x0FU
#define GRAY_MASK_RIGHT 0xF0U
#else
#define GRAY_MASK_LEFT  0x07U
#define GRAY_MASK_RIGHT 0x1CU
#endif

typedef struct
{
    GPIO_Regs *port;
    uint32_t pin;
    uint32_t iomux;
} Graydetect_Channel_t;

static const Graydetect_Channel_t s_channels[GRAY_CHANNEL_COUNT] = {
    {GRAY_INPUTS_CH0_PORT, GRAY_INPUTS_CH0_PIN, GRAY_INPUTS_CH0_IOMUX},
    {GRAY_INPUTS_CH1_PORT, GRAY_INPUTS_CH1_PIN, GRAY_INPUTS_CH1_IOMUX},
    {GRAY_INPUTS_CH2_PORT, GRAY_INPUTS_CH2_PIN, GRAY_INPUTS_CH2_IOMUX},
    {GRAY_INPUTS_CH3_PORT, GRAY_INPUTS_CH3_PIN, GRAY_INPUTS_CH3_IOMUX},
    {GRAY_INPUTS_CH4_PORT, GRAY_INPUTS_CH4_PIN, GRAY_INPUTS_CH4_IOMUX},
#if CAR_IS_MASTER
    {GRAY_INPUTS_CH5_PORT, GRAY_INPUTS_CH5_PIN, GRAY_INPUTS_CH5_IOMUX},
    {GRAY_INPUTS_CH6_PORT, GRAY_INPUTS_CH6_PIN, GRAY_INPUTS_CH6_IOMUX},
    {GRAY_INPUTS_CH7_PORT, GRAY_INPUTS_CH7_PIN, GRAY_INPUTS_CH7_IOMUX},
#endif
};

static uint8_t Graydetect_GetMask(uint8_t side)
{
    if (side == GRAY_SIDE_LEFT) return GRAY_MASK_LEFT;
    if (side == GRAY_SIDE_RIGHT) return GRAY_MASK_RIGHT;
    return GRAY_MASK_ALL;
}

void Graydetect_Init(void)
{
    uint8_t index;

    /* 上拉输入(原 STM32 版为 GPIO_Mode_IPU)：灰度板开漏输出/线未接时防浮空误读 */
    for (index = 0U; index < GRAY_CHANNEL_COUNT; index++)
    {
        DL_GPIO_initDigitalInputFeatures(s_channels[index].iomux,
            DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
            DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    }
}

uint8_t Graydetect_GetState(void)
{
    uint8_t state = 0U;
    uint8_t index;

    for (index = 0U; index < GRAY_CHANNEL_COUNT; index++)
    {
        if (DL_GPIO_readPins(s_channels[index].port,
                             s_channels[index].pin) != 0U)
        {
            state |= (uint8_t)(1U << index);
        }
    }
    return state;
}

uint8_t Graydetect_GetBit(uint8_t index)
{
    return (index < GRAY_CHANNEL_COUNT) ?
        (uint8_t)((Graydetect_GetState() >> index) & 1U) : 0U;
}

float Graydetect_GetError(uint8_t side)
{
    uint8_t state = Graydetect_GetState() & Graydetect_GetMask(side);
    float center = (float)(GRAY_CHANNEL_COUNT - 1U) / 2.0f;
    float sum = 0.0f;
    float count = 0.0f;
    uint8_t i;

    for (i = 0U; i < GRAY_CHANNEL_COUNT; i++)
    {
        if ((state & (uint8_t)(1U << i)) != 0U)
        {
            sum += (float)i - center;
            count += 1.0f;
        }
    }
    return (count > 0.5f) ? (sum / count) : 0.0f;
}

uint8_t Graydetect_OnLine(uint8_t side)
{
    return ((Graydetect_GetState() & Graydetect_GetMask(side)) != 0U) ? 1U : 0U;
}

#else

void Graydetect_Init(void)
{
}

uint8_t Graydetect_GetState(void)
{
    return 0U;
}

uint8_t Graydetect_GetBit(uint8_t index)
{
    (void)index;
    return 0U;
}

float Graydetect_GetError(uint8_t side)
{
    (void)side;
    return 0.0f;
}

uint8_t Graydetect_OnLine(uint8_t side)
{
    (void)side;
    return 0U;
}

#endif
