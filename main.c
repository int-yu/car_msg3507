#include "Accomplish/25H.h"
#include "Application/Core/App.h"
#include "Application/Mission/Mission.h"
#include "System/Interrupt.h"

int main(void)
{
    App_UpdateContext_t updateContext;

    App_Init();
    Mission_Init(Accomplish25H_GetMissionGraph());
    Interrupt_Enable();

    for (;;)
    {
        if (App_Update(&updateContext) != 0U)
        {
            Mission_Update(&updateContext);
        }
    }
}
