#include "Accomplish/25H.h"
#include "Application/Comms/BluetoothDebug.h"
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
            /* 蓝牙命令只在 Mission 起始等待状态有效；任务运行中只解析遥测，不接管车轮。 */
            BluetoothDebug_ControlUpdate(Mission_IsAtStartState());
            Mission_Update(&updateContext);
        }
    }
}
