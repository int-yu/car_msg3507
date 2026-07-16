#include "System/Delay.h"
#include "ti_msp_dl_config.h"

void Delay_us(uint32_t us)
{
    const uint32_t cyclesPerUs = CPUCLK_FREQ / 1000000U;
    while (us-- > 0U) delay_cycles(cyclesPerUs);
}

void Delay_ms(uint32_t ms)
{
    while (ms-- > 0U) Delay_us(1000U);
}

void Delay_s(uint32_t s)
{
    while (s-- > 0U) Delay_ms(1000U);
}
