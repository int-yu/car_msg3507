#include "System/Tick.h"
#include "ti_msp_dl_config.h"

static volatile uint8_t s_tickCount;

void Tick_Init(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    s_tickCount = 0U;
    __set_PRIMASK(primask);
}

uint8_t Tick_PollCount(void)
{
    uint8_t count;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    count = s_tickCount;
    s_tickCount = 0U;
    __set_PRIMASK(primask);
    return count;
}

uint8_t Tick_Poll(void) { return (Tick_PollCount() != 0U) ? 1U : 0U; }

void SysTick_Handler(void)
{
    if (s_tickCount < 255U) s_tickCount++;
}
