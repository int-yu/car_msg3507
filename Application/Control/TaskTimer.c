#include "Application/Control/TaskTimer.h"
#include <limits.h>

typedef struct
{
    TaskTimer_Owner_t owner;
    uint32_t elapsedTicks;
    uint8_t running;
} TaskTimer_Context_t;

static TaskTimer_Context_t s_context;

void TaskTimer_Init(void)
{
    s_context.owner = TASK_TIMER_OWNER_NONE;
    s_context.elapsedTicks = 0U;
    s_context.running = 0U;
}

void TaskTimer_Update(uint8_t elapsedTicks)
{
    if ((s_context.running == 0U) || (elapsedTicks == 0U))
    {
        return;
    }

    if ((UINT32_MAX - s_context.elapsedTicks) < elapsedTicks)
    {
        s_context.elapsedTicks = UINT32_MAX;
    }
    else
    {
        s_context.elapsedTicks += elapsedTicks;
    }
}

void TaskTimer_Start(TaskTimer_Owner_t owner)
{
    if (owner == TASK_TIMER_OWNER_NONE)
    {
        return;
    }

    s_context.owner = owner;
    s_context.elapsedTicks = 0U;
    s_context.running = 1U;
}

void TaskTimer_Stop(TaskTimer_Owner_t owner)
{
    if ((owner != TASK_TIMER_OWNER_NONE) &&
        (s_context.owner == owner))
    {
        s_context.running = 0U;
    }
}

uint32_t TaskTimer_GetElapsedTicks(void)
{
    return s_context.elapsedTicks;
}

uint8_t TaskTimer_IsRunning(void)
{
    return s_context.running;
}

TaskTimer_Owner_t TaskTimer_GetOwner(void)
{
    return s_context.owner;
}
