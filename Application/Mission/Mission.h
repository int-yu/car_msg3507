#ifndef APPLICATION_MISSION_MISSION_H
#define APPLICATION_MISSION_MISSION_H

#include "Application/Core/App.h"
#include <stdint.h>

typedef enum
{
    MISSION_CALLBACK_OK = 0,
    MISSION_CALLBACK_ERROR
} Mission_CallbackResult_t;

typedef enum
{
    MISSION_ACTION_RUNNING = 0,
    MISSION_ACTION_FINISHED,
    MISSION_ACTION_ERROR
} Mission_ActionStatus_t;

typedef enum
{
    MISSION_EXIT_COMPLETED = 0,
    MISSION_EXIT_INTERRUPTED,
    MISSION_EXIT_STOPPED,
    MISSION_EXIT_ERROR
} Mission_ExitReason_t;

typedef enum
{
    MISSION_TRANSITION_NORMAL = 0,
    MISSION_TRANSITION_INTERRUPT
} Mission_TransitionType_t;

typedef enum
{
    MISSION_STATUS_UNINITIALIZED = 0,
    MISSION_STATUS_RUNNING,
    MISSION_STATUS_COMPLETED,
    MISSION_STATUS_ERROR
} Mission_Status_t;

typedef struct
{
    uint16_t currentState;
    float stateElapsedSeconds;
    Mission_ActionStatus_t actionStatus;
} Mission_Runtime_t;

typedef Mission_CallbackResult_t (*Mission_EnterCallback_t)(void);
typedef Mission_ActionStatus_t (*Mission_UpdateCallback_t)(float dt);
typedef void (*Mission_ExitCallback_t)(Mission_ExitReason_t reason);
typedef uint8_t (*Mission_ConditionCallback_t)(
    const Mission_Runtime_t *runtime,
    const App_UpdateContext_t *updateContext);

typedef struct
{
    Mission_ConditionCallback_t condition;
    uint16_t targetState;
    Mission_TransitionType_t type;
} Mission_Transition_t;

typedef struct
{
    Mission_EnterCallback_t onEnter;
    Mission_UpdateCallback_t onUpdate;
    Mission_ExitCallback_t onExit;
    const Mission_Transition_t *transitions;
    uint8_t transitionCount;
    uint8_t interruptible;
} Mission_StateDefinition_t;

/* 题目层提供静态状态表，Mission 只负责校验和执行。 */
typedef struct
{
    const Mission_StateDefinition_t *states;
    uint16_t stateCount;
    uint16_t startState;
    uint16_t errorState;
} Mission_GraphDefinition_t;

void Mission_Init(const Mission_GraphDefinition_t *graph);
void Mission_Update(const App_UpdateContext_t *updateContext);
void Mission_Stop(void);

Mission_Status_t Mission_GetStatus(void);
const Mission_Runtime_t *Mission_GetRuntime(void);
Mission_ActionStatus_t Mission_GetMotionActionStatus(void);
uint8_t Mission_ContextHasBluetoothSignal(
    const App_UpdateContext_t *updateContext, uint8_t signal);

#endif
