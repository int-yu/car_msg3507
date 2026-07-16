#include "Hardware/Board/LED.h"
#include "ti_msp_dl_config.h"

void LED_Init(void)
{
    LED1_OFF();
    LED2_OFF();
}

void LED1_ON(void)  { DL_GPIO_setPins(BOARD_OUTPUTS_LED1_PORT, BOARD_OUTPUTS_LED1_PIN); }
void LED1_OFF(void) { DL_GPIO_clearPins(BOARD_OUTPUTS_LED1_PORT, BOARD_OUTPUTS_LED1_PIN); }
void LED1_Turn(void) { DL_GPIO_togglePins(BOARD_OUTPUTS_LED1_PORT, BOARD_OUTPUTS_LED1_PIN); }
void LED2_ON(void)  { DL_GPIO_setPins(BOARD_OUTPUTS_LED2_PORT, BOARD_OUTPUTS_LED2_PIN); }
void LED2_OFF(void) { DL_GPIO_clearPins(BOARD_OUTPUTS_LED2_PORT, BOARD_OUTPUTS_LED2_PIN); }
void LED2_Turn(void) { DL_GPIO_togglePins(BOARD_OUTPUTS_LED2_PORT, BOARD_OUTPUTS_LED2_PIN); }

/* 兼容旧接口：本板只有两个独立状态 LED，RGB 接口映射到 LED2。 */
void LED_RGB_ON(void)  { LED2_ON(); }
void LED_RGB_OFF(void) { LED2_OFF(); }
