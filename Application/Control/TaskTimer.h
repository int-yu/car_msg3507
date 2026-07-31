#ifndef APPLICATION_CONTROL_TASK_TIMER_H
#define APPLICATION_CONTROL_TASK_TIMER_H

#include <stdint.h>

typedef enum
{
    TASK_TIMER_OWNER_NONE = 0,
    TASK_TIMER_OWNER_LINE,
    TASK_TIMER_OWNER_BALL
} TaskTimer_Owner_t;

void TaskTimer_Init(void);
void TaskTimer_Update(uint8_t elapsedTicks);

/* Starting a task always clears the previous displayed duration. */
void TaskTimer_Start(TaskTimer_Owner_t owner);
/* A task may stop only the timer it owns. */
void TaskTimer_Stop(TaskTimer_Owner_t owner);

uint32_t TaskTimer_GetElapsedTicks(void);
uint8_t TaskTimer_IsRunning(void);
TaskTimer_Owner_t TaskTimer_GetOwner(void);

#endif
