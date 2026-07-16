#include "Hardware/Board/Key.h"
#include "ti_msp_dl_config.h"

static uint8_t Key_ReadPin(uint8_t index)
{
    switch (index)
    {
        case 1U:
            return (DL_GPIO_readPins(BUTTON_INPUTS_KEY1_PORT,
                                     BUTTON_INPUTS_KEY1_PIN) == 0U) ? 1U : 0U;
        case 2U:
            return (DL_GPIO_readPins(BUTTON_INPUTS_KEY2_PORT,
                                     BUTTON_INPUTS_KEY2_PIN) == 0U) ? 1U : 0U;
        case 3U:
            return (DL_GPIO_readPins(BUTTON_INPUTS_KEY3_PORT,
                                     BUTTON_INPUTS_KEY3_PIN) == 0U) ? 1U : 0U;
        case 4U:
            return (DL_GPIO_readPins(BUTTON_INPUTS_KEY4_PORT,
                                     BUTTON_INPUTS_KEY4_PIN) == 0U) ? 1U : 0U;
        default:
            return 0U;
    }
}

void Key_Init(void)
{
    DL_GPIO_initDigitalInputFeatures(
        BUTTON_INPUTS_KEY1_IOMUX, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_PULL_UP, DL_GPIO_HYSTERESIS_DISABLE,
        DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(
        BUTTON_INPUTS_KEY2_IOMUX, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_PULL_UP, DL_GPIO_HYSTERESIS_DISABLE,
        DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(
        BUTTON_INPUTS_KEY3_IOMUX, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_PULL_UP, DL_GPIO_HYSTERESIS_DISABLE,
        DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(
        BUTTON_INPUTS_KEY4_IOMUX, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_PULL_UP, DL_GPIO_HYSTERESIS_DISABLE,
        DL_GPIO_WAKEUP_DISABLE);
}

uint8_t Key_GetPressedMask(void)
{
    uint8_t index;
    uint8_t pressedMask = 0U;

    for (index = 1U; index <= 4U; index++)
    {
        if (Key_ReadPin(index) != 0U)
        {
            pressedMask |= (uint8_t)(1U << (index - 1U));
        }
    }
    return pressedMask;
}

uint8_t Key_GetNum(void)
{
    uint8_t index;
    uint8_t pressedMask = Key_GetPressedMask();

    for (index = 1U; index <= 4U; index++)
    {
        if ((pressedMask & (1U << (index - 1U))) != 0U)
        {
            return index;
        }
    }
    return 0U;
}
