#include "Accomplish/Test.h"
#include "Application/Control/MotionManager.h"
#include <stddef.h>

/*
 * 本文件只验证“定距减速完成后再短暂主动刹车”的效果。
 * 不直接调用 Motor_Brake()，所有电机控制仍由 MotionManager 统一调度。
 *
 * 流程：WAITING --KEY2--> FORWARD --定距完成--> BRAKE --刹车结束--> WAITING
 */

#define ACCOMPLISH_TEST_ARRAY_COUNT(array) \
    ((uint8_t)(sizeof(array) / sizeof((array)[0])))

typedef enum
{
    ACCOMPLISH_TEST_STATE_WAITING = 0,  /* 等待 KEY2，不驱动车轮。 */
    ACCOMPLISH_TEST_STATE_FORWARD,      /* 定距直行并按速度曲线降速。 */
    ACCOMPLISH_TEST_STATE_BRAKE,        /* PWM 释放后短暂主动刹车。 */
    ACCOMPLISH_TEST_STATE_ERROR,        /* 运动异常后的安全状态。 */
    ACCOMPLISH_TEST_STATE_COUNT
} AccomplishTest_StateId_t;

/* 等待状态持续运行，使 Mission 能检查 KEY2 的打断转换。 */
static Mission_ActionStatus_t AccomplishTest_WaitingUpdate(float dt)
{
    (void)dt;
    return MISSION_ACTION_RUNNING;
}

/* 启动定距直行；终点速度为零后才会报告正常完成。 */
static Mission_CallbackResult_t AccomplishTest_ForwardEnter(void)
{
    return (MotionManager_StartForward(
                ACCOMPLISH_TEST_BRAKE_DISTANCE_MM,
                ACCOMPLISH_TEST_BRAKE_SPEED_MMPS,
                ACCOMPLISH_TEST_BRAKE_END_SPEED_MMPS) ==
            MOTION_MANAGER_RESULT_OK) ?
        MISSION_CALLBACK_OK : MISSION_CALLBACK_ERROR;
}

/* 由统一运动调度层执行释放、短刹和最终释放。 */
static Mission_CallbackResult_t AccomplishTest_BrakeEnter(void)
{
    return (MotionManager_StartBrake() == MOTION_MANAGER_RESULT_OK) ?
        MISSION_CALLBACK_OK : MISSION_CALLBACK_ERROR;
}

/* App 每拍已更新 MotionManager，此处只转换其状态供 Mission 使用。 */
static Mission_ActionStatus_t AccomplishTest_MotionUpdate(float dt)
{
    (void)dt;
    return Mission_GetMotionActionStatus();
}

/* KEY2 按下沿启动；长按不会反复触发。 */
static uint8_t AccomplishTest_ConditionStart(
    const Mission_Runtime_t *runtime,
    const App_UpdateContext_t *updateContext)
{
    (void)runtime;
    if (updateContext == NULL)
    {
        return 0U;
    }

    return ((updateContext->pressedEdges &
             ACCOMPLISH_TEST_START_KEY_MASK) != 0U) ? 1U : 0U;
}

static const Mission_Transition_t s_waitingTransitions[] = {
    {
        AccomplishTest_ConditionStart,
        ACCOMPLISH_TEST_STATE_FORWARD,
        MISSION_TRANSITION_INTERRUPT,
    },
};

static const Mission_Transition_t s_forwardTransitions[] = {
    {
        NULL,
        ACCOMPLISH_TEST_STATE_BRAKE,
        MISSION_TRANSITION_NORMAL,
    },
};

static const Mission_Transition_t s_brakeTransitions[] = {
    {
        NULL,
        ACCOMPLISH_TEST_STATE_WAITING,
        MISSION_TRANSITION_NORMAL,
    },
};

static const Mission_StateDefinition_t s_stateDefinitions[
    ACCOMPLISH_TEST_STATE_COUNT] = {
    [ACCOMPLISH_TEST_STATE_WAITING] = {
        .onEnter = NULL,
        .onUpdate = AccomplishTest_WaitingUpdate,
        .onExit = NULL,
        .transitions = s_waitingTransitions,
        .transitionCount =
            ACCOMPLISH_TEST_ARRAY_COUNT(s_waitingTransitions),
        .interruptible = 1U,
    },
    [ACCOMPLISH_TEST_STATE_FORWARD] = {
        .onEnter = AccomplishTest_ForwardEnter,
        .onUpdate = AccomplishTest_MotionUpdate,
        .onExit = NULL,
        .transitions = s_forwardTransitions,
        .transitionCount =
            ACCOMPLISH_TEST_ARRAY_COUNT(s_forwardTransitions),
        .interruptible = 0U,
    },
    [ACCOMPLISH_TEST_STATE_BRAKE] = {
        .onEnter = AccomplishTest_BrakeEnter,
        .onUpdate = AccomplishTest_MotionUpdate,
        .onExit = NULL,
        .transitions = s_brakeTransitions,
        .transitionCount =
            ACCOMPLISH_TEST_ARRAY_COUNT(s_brakeTransitions),
        .interruptible = 0U,
    },
    [ACCOMPLISH_TEST_STATE_ERROR] = {
        .onEnter = NULL,
        .onUpdate = NULL,
        .onExit = NULL,
        .transitions = NULL,
        .transitionCount = 0U,
        .interruptible = 0U,
    },
};

static const Mission_GraphDefinition_t s_missionGraph = {
    .states = s_stateDefinitions,
    .stateCount = ACCOMPLISH_TEST_STATE_COUNT,
    .startState = ACCOMPLISH_TEST_STATE_WAITING,
    .errorState = ACCOMPLISH_TEST_STATE_ERROR,
};

const Mission_GraphDefinition_t *AccomplishTest_GetMissionGraph(void)
{
    return &s_missionGraph;
}
