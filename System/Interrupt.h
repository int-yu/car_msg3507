#ifndef SYSTEM_INTERRUPT_H
#define SYSTEM_INTERRUPT_H

/* 所有驱动和 Mission 初始化完成后，统一开启全局硬件中断。 */
void Interrupt_Enable(void);
void Interrupt_Disable(void);

#endif
