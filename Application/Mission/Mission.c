#include "Application/Mission/Mission.h"
#include "Application/Control/MotionManager.h"
#include <stddef.h>

static Mission_Runtime_t s_runtime;
static Mission_Status_t s_status = MISSION_STATUS_UNINITIALIZED;
static const Mission_GraphDefinition_t *s_graph;
static uint8_t s_graphValid;
static uint8_t s_stateActive;

static uint8_t Mission_GraphIsValid(
    const Mission_GraphDefinition_t *graph)
{
    uint16_t stateIndex;

    if ((graph == NULL) || (graph->states == NULL) ||
        (graph->stateCount == 0U) ||
        (graph->startState >= graph->stateCount) ||
        (graph->errorState >= graph->stateCount))
    {
        return 0U;
    }

    for (stateIndex = 0U; stateIndex < graph->stateCount; stateIndex++)
    {
        const Mission_StateDefinition_t *state =
            &graph->states[stateIndex];
        uint8_t transitionIndex;

        if ((state->transitionCount != 0U) &&
            (state->transitions == NULL))
        {
            return 0U;
        }

        for (transitionIndex = 0U;
             transitionIndex < state->transitionCount;
             transitionIndex++)
        {
            const Mission_Transition_t *transition =
                &state->transitions[transitionIndex];

            if ((transition->targetState >= graph->stateCount) ||
                ((transition->type != MISSION_TRANSITION_NORMAL) &&
                 (transition->type != MISSION_TRANSITION_INTERRUPT)))
            {
                return 0U;
            }
        }
    }

    return 1U;
}

static void Mission_CallExit(Mission_ExitReason_t reason)
{
    const Mission_StateDefinition_t *state;

    if ((s_stateActive == 0U) || (s_graph == NULL) ||
        (s_runtime.currentState >= s_graph->stateCount))
    {
        return;
    }

    state = &s_graph->states[s_runtime.currentState];
    if (state->onExit != NULL)
    {
        state->onExit(reason);
    }
    s_stateActive = 0U;
}

static uint8_t Mission_EnterState(uint16_t stateId)
{
    const Mission_StateDefinition_t *state;

    if ((s_graphValid == 0U) || (s_graph == NULL) ||
        (stateId >= s_graph->stateCount))
    {
        return 0U;
    }

    state = &s_graph->states[stateId];
    s_runtime.currentState = stateId;
    s_runtime.stateElapsedSeconds = 0.0f;
    s_runtime.actionStatus = (state->onUpdate == NULL) ?
        MISSION_ACTION_FINISHED : MISSION_ACTION_RUNNING;
    s_stateActive = 1U;

    if ((state->onEnter != NULL) &&
        (state->onEnter() != MISSION_CALLBACK_OK))
    {
        s_runtime.actionStatus = MISSION_ACTION_ERROR;
        return 0U;
    }

    s_status = (s_runtime.actionStatus == MISSION_ACTION_FINISHED) ?
        MISSION_STATUS_COMPLETED : MISSION_STATUS_RUNNING;
    return 1U;
}

static void Mission_SetError(void)
{
    const Mission_StateDefinition_t *errorState;

    Mission_CallExit(MISSION_EXIT_ERROR);
    MotionManager_Stop();

    if ((s_graphValid == 0U) || (s_graph == NULL) ||
        (s_graph->errorState >= s_graph->stateCount))
    {
        s_runtime.currentState = 0U;
        s_runtime.stateElapsedSeconds = 0.0f;
        s_runtime.actionStatus = MISSION_ACTION_ERROR;
        s_stateActive = 0U;
        s_status = MISSION_STATUS_ERROR;
        return;
    }

    s_runtime.currentState = s_graph->errorState;
    s_runtime.stateElapsedSeconds = 0.0f;
    s_runtime.actionStatus = MISSION_ACTION_ERROR;
    s_stateActive = 1U;
    s_status = MISSION_STATUS_ERROR;

    errorState = &s_graph->states[s_graph->errorState];
    if (errorState->onEnter != NULL)
    {
        (void)errorState->onEnter();
    }
}

static void Mission_ResetToStart(void)
{
    Mission_CallExit(MISSION_EXIT_STOPPED);
    MotionManager_Stop();

    if (Mission_EnterState(s_graph->startState) == 0U)
    {
        Mission_SetError();
    }
}

static uint8_t Mission_TryTransition(
    Mission_TransitionType_t transitionType,
    const App_UpdateContext_t *updateContext)
{
    const Mission_StateDefinition_t *state =
        &s_graph->states[s_runtime.currentState];
    uint8_t transitionIndex;

    for (transitionIndex = 0U;
         transitionIndex < state->transitionCount;
         transitionIndex++)
    {
        const Mission_Transition_t *transition =
            &state->transitions[transitionIndex];
        Mission_ExitReason_t exitReason;

        if (transition->type != transitionType)
        {
            continue;
        }
        if ((transition->condition != NULL) &&
            (transition->condition(&s_runtime, updateContext) == 0U))
        {
            continue;
        }

        exitReason = (transitionType == MISSION_TRANSITION_INTERRUPT) ?
            MISSION_EXIT_INTERRUPTED : MISSION_EXIT_COMPLETED;
        Mission_CallExit(exitReason);
        MotionManager_Stop();
        if (Mission_EnterState(transition->targetState) == 0U)
        {
            Mission_SetError();
        }
        return 1U;
    }

    return 0U;
}

void Mission_Init(const Mission_GraphDefinition_t *graph)
{
    s_graph = graph;
    s_graphValid = Mission_GraphIsValid(graph);
    s_runtime.currentState = 0U;
    s_runtime.stateElapsedSeconds = 0.0f;
    s_runtime.actionStatus = MISSION_ACTION_ERROR;
    s_status = MISSION_STATUS_UNINITIALIZED;
    s_stateActive = 0U;

    if (s_graphValid == 0U)
    {
        MotionManager_Stop();
        s_status = MISSION_STATUS_ERROR;
        return;
    }

    if (Mission_EnterState(s_graph->startState) == 0U)
    {
        Mission_SetError();
    }
}

void Mission_Update(const App_UpdateContext_t *updateContext)
{
    const Mission_StateDefinition_t *state;

    if ((updateContext == NULL) || (s_graphValid == 0U) ||
        (s_status == MISSION_STATUS_UNINITIALIZED))
    {
        return;
    }

    if (Mission_ContextHasBluetoothSignal(updateContext, 0U) != 0U)
    {
        Mission_ResetToStart();
        return;
    }

    if (s_status == MISSION_STATUS_ERROR)
    {
        return;
    }

    state = &s_graph->states[s_runtime.currentState];
    s_runtime.stateElapsedSeconds += updateContext->dt;

    if ((s_runtime.actionStatus == MISSION_ACTION_RUNNING) &&
        (state->onUpdate != NULL))
    {
        s_runtime.actionStatus = state->onUpdate(updateContext->dt);
    }

    if (((s_runtime.actionStatus != MISSION_ACTION_RUNNING) &&
         (s_runtime.actionStatus != MISSION_ACTION_FINISHED) &&
         (s_runtime.actionStatus != MISSION_ACTION_ERROR)) ||
        (s_runtime.actionStatus == MISSION_ACTION_ERROR) ||
        (MotionManager_GetError() != MOTION_MANAGER_ERROR_NONE))
    {
        Mission_SetError();
        return;
    }

    if (s_runtime.actionStatus == MISSION_ACTION_RUNNING)
    {
        s_status = MISSION_STATUS_RUNNING;
        if (state->interruptible != 0U)
        {
            (void)Mission_TryTransition(
                MISSION_TRANSITION_INTERRUPT, updateContext);
        }
        return;
    }

    s_status = MISSION_STATUS_COMPLETED;
    (void)Mission_TryTransition(MISSION_TRANSITION_NORMAL, updateContext);
}

void Mission_Stop(void)
{
    if ((s_graphValid == 0U) ||
        (s_status == MISSION_STATUS_UNINITIALIZED))
    {
        MotionManager_Stop();
        return;
    }

    Mission_ResetToStart();
}

Mission_Status_t Mission_GetStatus(void)
{
    return s_status;
}

const Mission_Runtime_t *Mission_GetRuntime(void)
{
    return &s_runtime;
}

Mission_ActionStatus_t Mission_GetMotionActionStatus(void)
{
    if (MotionManager_GetError() != MOTION_MANAGER_ERROR_NONE)
    {
        return MISSION_ACTION_ERROR;
    }
    if (MotionManager_IsFinished() != 0U)
    {
        return MISSION_ACTION_FINISHED;
    }
    if (MotionManager_IsBusy() != 0U)
    {
        return MISSION_ACTION_RUNNING;
    }
    return MISSION_ACTION_FINISHED;
}

uint8_t Mission_ContextHasBluetoothSignal(
    const App_UpdateContext_t *updateContext, uint8_t signal)
{
    if ((updateContext == NULL) ||
        (updateContext->hasBluetoothSignal == 0U))
    {
        return 0U;
    }

    return (updateContext->bluetoothSignal == signal) ? 1U : 0U;
}
