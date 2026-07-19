#include "Accomplish/Brushless_Motor_Test.h"
#include "Application/Core/TestApp.h"
#include "Application/Mission/Mission.h"
#include "System/Interrupt.h"

int main(void)
{
    App_UpdateContext_t updateContext;

    TestApp_Init();
    Mission_Init(BrushlessMotorTest_GetMissionGraph());
    Interrupt_Enable();

    for (;;)
    {
        if (TestApp_Update(&updateContext) != 0U)
        {
            Mission_Update(&updateContext);
        }
    }
}
