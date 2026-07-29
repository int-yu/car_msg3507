#include "Accomplish/26H.h"
#include <stddef.h>

static uint32_t s_elapsedTicks;
static uint8_t s_timing;

void Accomplish26H_Init(void)
{
    s_elapsedTicks = 0U;
    s_timing = 0U;
}

void Accomplish26H_Update(const App_UpdateContext_t *context)
{
    if (context == NULL)
    {
        return;
    }

    if (s_timing == 0U)
    {
        if ((context->pressedEdges &
             ACCOMPLISH_26H_START_STOP_KEY_MASK) != 0U)
        {
            s_elapsedTicks = 0U;
            s_timing = 1U;
        }
        return;
    }

    s_elapsedTicks += context->elapsedTicks;
}

uint8_t Accomplish26H_IsTiming(void)
{
    return s_timing;
}

uint32_t Accomplish26H_GetElapsedTicks(void)
{
    return s_elapsedTicks;
}
