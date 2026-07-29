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
    uint8_t keyPressed;

    if (context == NULL)
    {
        return;
    }

    keyPressed = ((context->pressedEdges &
                   ACCOMPLISH_26H_START_STOP_KEY_MASK) != 0U) ?
        1U : 0U;

    if (s_timing == 0U)
    {
        if (keyPressed != 0U)
        {
            s_elapsedTicks = 0U;
            s_timing = 1U;
        }
        return;
    }

    if ((UINT32_MAX - s_elapsedTicks) < context->elapsedTicks)
    {
        s_elapsedTicks = UINT32_MAX;
    }
    else
    {
        s_elapsedTicks += context->elapsedTicks;
    }

    if (keyPressed != 0U)
    {
        s_timing = 0U;
    }
}

uint8_t Accomplish26H_IsTiming(void)
{
    return s_timing;
}

uint32_t Accomplish26H_GetElapsedTicks(void)
{
    return s_elapsedTicks;
}
