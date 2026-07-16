#include "Hardware/Sensors/Graydetect.h"
#include "ti_msp_dl_config.h"

#define GRAY_MASK_ALL   0x1FU
#define GRAY_MASK_LEFT  0x07U
#define GRAY_MASK_RIGHT 0x1CU

static const float s_weight[5] = {-2.0f, -1.0f, 0.0f, 1.0f, 2.0f};

static uint8_t Graydetect_GetMask(uint8_t side)
{
    if (side == GRAY_SIDE_LEFT) return GRAY_MASK_LEFT;
    if (side == GRAY_SIDE_RIGHT) return GRAY_MASK_RIGHT;
    return GRAY_MASK_ALL;
}

void Graydetect_Init(void)
{
    /* 上拉输入(原 STM32 版为 GPIO_Mode_IPU)：灰度板开漏输出/线未接时防浮空误读 */
    DL_GPIO_initDigitalInputFeatures(GRAY_INPUTS_CH0_IOMUX, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_PULL_UP, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(GRAY_INPUTS_CH1_IOMUX, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_PULL_UP, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(GRAY_INPUTS_CH2_IOMUX, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_PULL_UP, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(GRAY_INPUTS_CH3_IOMUX, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_PULL_UP, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(GRAY_INPUTS_CH4_IOMUX, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_PULL_UP, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
}

uint8_t Graydetect_GetState(void)
{
    uint8_t state = 0U;
    if (DL_GPIO_readPins(GRAY_INPUTS_CH0_PORT, GRAY_INPUTS_CH0_PIN) != 0U) state |= 0x01U;
    if (DL_GPIO_readPins(GRAY_INPUTS_CH1_PORT, GRAY_INPUTS_CH1_PIN) != 0U) state |= 0x02U;
    if (DL_GPIO_readPins(GRAY_INPUTS_CH2_PORT, GRAY_INPUTS_CH2_PIN) != 0U) state |= 0x04U;
    if (DL_GPIO_readPins(GRAY_INPUTS_CH3_PORT, GRAY_INPUTS_CH3_PIN) != 0U) state |= 0x08U;
    if (DL_GPIO_readPins(GRAY_INPUTS_CH4_PORT, GRAY_INPUTS_CH4_PIN) != 0U) state |= 0x10U;
    return state;
}

uint8_t Graydetect_GetBit(uint8_t index)
{
    return (index < 5U) ? (uint8_t)((Graydetect_GetState() >> index) & 1U) : 0U;
}

float Graydetect_GetError(uint8_t side)
{
    uint8_t state = Graydetect_GetState() & Graydetect_GetMask(side);
    float sum = 0.0f;
    float count = 0.0f;
    uint8_t i;
    for (i = 0U; i < 5U; i++)
    {
        if ((state & (1U << i)) != 0U) { sum += s_weight[i]; count += 1.0f; }
    }
    return (count > 0.5f) ? (sum / count) : 0.0f;
}

uint8_t Graydetect_OnLine(uint8_t side)
{
    return ((Graydetect_GetState() & Graydetect_GetMask(side)) != 0U) ? 1U : 0U;
}
