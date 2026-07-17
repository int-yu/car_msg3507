#include "System/Interrupt.h"
#include "ti_msp_dl_config.h"

void Interrupt_Enable(void)
{
    __enable_irq();
}

void Interrupt_Disable(void)
{
    __disable_irq();
}
