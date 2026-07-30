#include "Accomplish/26H.h"
#include "Accomplish/26H_Ball.h"
#include "Application/Control/BeamActuator.h"
#include "Application/Core/App.h"
#include "System/Interrupt.h"

/* KEY2 单独按下启动要求 3 的摆球序列；KEY1 是要求 2 的单圈起跑，
 * 两键同时按下是物理急停，由 App 统一处理。 */
#define MAIN_BALL_START_KEY_MASK  0x02U
#define MAIN_EMERGENCY_CHORD_MASK (0x01U | 0x02U)

int main(void)
{
    App_UpdateContext_t updateContext;

    App_Init();
    Accomplish26H_Init();
    Accomplish26HBall_Init();
    Interrupt_Enable();

    for (;;)
    {
        if (App_Update(&updateContext) != 0U)
        {
            Accomplish26H_Update(&updateContext);

            /* 要求 3 与要求 2 是不同测试项，不会同时进行。只在急停
             * 组合键未按下时，把 KEY2 的按下沿当作摆球起跑。 */
            if (((updateContext.pressedKeys & MAIN_EMERGENCY_CHORD_MASK) !=
                 MAIN_EMERGENCY_CHORD_MASK) &&
                ((updateContext.pressedEdges & MAIN_BALL_START_KEY_MASK) !=
                 0U))
            {
                (void)Accomplish26HBall_Start();
            }

            Accomplish26HBall_Update(updateContext.dt);
            /* 执行层放在任务层之后：本拍算出的倾角当拍下发给步进。 */
            BeamActuator_Update(updateContext.dt);
        }
    }
}
