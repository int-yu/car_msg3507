#include "Accomplish/26H.h"
#include "Application/Core/App.h"
#include "System/Interrupt.h"

int main(void)
{
    App_UpdateContext_t updateContext;

    App_Init();
    Accomplish26H_Init();
    Interrupt_Enable();

    for (;;)
    {
        if (App_Update(&updateContext) != 0U)
        {
            Accomplish26H_Update(&updateContext);
        }
    }
}
