#include "Accomplish/Brushless_Motor_Test.h"
#include "Application/Gimbal/Gimbal.h"
#include <stddef.h>

/*
 * 测试流程：
 * 上电 -> ZERO -> Y_TO_180 <-> Y_TO_0 --KEY2/C0--> STOPPED
 *
 * ZERO 先把两个协议目标设为 0°。之后每完成一段运动，X 轴目标继续
 * 沿右转方向累加 180°，Y 轴则在 0° 和 180° 之间往返。
 * 本测试不会调用“清除多圈角度”或“保存参数”。ZERO 状态直接命令电机
 * 转到协议坐标的 0°，不会把当前物理位置错误地当作 0°。
 */

#define BRUSHLESS_MOTOR_TEST_ARRAY_COUNT(array) \
    ((uint8_t)(sizeof(array) / sizeof((array)[0])))

typedef enum
{
    BRUSHLESS_MOTOR_TEST_STATE_ZERO = 0,
    BRUSHLESS_MOTOR_TEST_STATE_Y_TO_180,
    BRUSHLESS_MOTOR_TEST_STATE_Y_TO_0,
    BRUSHLESS_MOTOR_TEST_STATE_STOPPED,
    BRUSHLESS_MOTOR_TEST_STATE_ERROR,
    BRUSHLESS_MOTOR_TEST_STATE_COUNT
} BrushlessMotorTest_StateId_t;

static float s_xTargetDeg;
static uint8_t s_hasAutoStarted;
static uint8_t s_autoStartPending;

static Mission_CallbackResult_t BrushlessMotorTest_StoppedEnter(void)
{
    s_xTargetDeg = 0.0f;
    /* 只允许上电后的第一次 STOPPED 自动进入测试，C0/KEY2 停止后不重启。 */
    if (s_hasAutoStarted == 0U)
    {
        s_hasAutoStarted = 1U;
        s_autoStartPending = 1U;
    }
    else
    {
        s_autoStartPending = 0U;
    }
    return MISSION_CALLBACK_OK;
}

static Mission_ActionStatus_t BrushlessMotorTest_StoppedUpdate(float dt)
{
    (void)dt;
    if (s_autoStartPending != 0U)
    {
        s_autoStartPending = 0U;
        return MISSION_ACTION_FINISHED;
    }
    return MISSION_ACTION_RUNNING;
}

/* 使能两轴、选择带 T 型规划的多圈模式，并命令两轴回到协议 0°。 */
static Mission_CallbackResult_t BrushlessMotorTest_ZeroEnter(void)
{
    s_xTargetDeg = 0.0f;
    if ((Gimbal_Enable() != GIMBAL_RESULT_OK) ||
        (Gimbal_SetTargetAngles(
             s_xTargetDeg, BRUSHLESS_MOTOR_TEST_Y_LOW_DEG) !=
         GIMBAL_RESULT_OK))
    {
        return MISSION_CALLBACK_ERROR;
    }
    return MISSION_CALLBACK_OK;
}

static Mission_CallbackResult_t BrushlessMotorTest_YTo180Enter(void)
{
    s_xTargetDeg += BRUSHLESS_MOTOR_TEST_X_RIGHT_SIGN *
                    BRUSHLESS_MOTOR_TEST_X_STEP_DEG;
    return (Gimbal_SetTargetAngles(
                s_xTargetDeg,
                BRUSHLESS_MOTOR_TEST_Y_HIGH_DEG) ==
            GIMBAL_RESULT_OK) ?
        MISSION_CALLBACK_OK : MISSION_CALLBACK_ERROR;
}

static Mission_CallbackResult_t BrushlessMotorTest_YTo0Enter(void)
{
    s_xTargetDeg += BRUSHLESS_MOTOR_TEST_X_RIGHT_SIGN *
                    BRUSHLESS_MOTOR_TEST_X_STEP_DEG;
    return (Gimbal_SetTargetAngles(
                s_xTargetDeg,
                BRUSHLESS_MOTOR_TEST_Y_LOW_DEG) ==
            GIMBAL_RESULT_OK) ?
        MISSION_CALLBACK_OK : MISSION_CALLBACK_ERROR;
}

/* 两个地址都连续反馈到达目标后，本段运动才算完成。 */
static Mission_ActionStatus_t BrushlessMotorTest_MoveUpdate(float dt)
{
    (void)dt;
    if (Gimbal_GetState() == GIMBAL_STATE_ERROR)
    {
        return MISSION_ACTION_ERROR;
    }
    return (Gimbal_AreTargetsReached() != 0U) ?
        MISSION_ACTION_FINISHED : MISSION_ACTION_RUNNING;
}

/* 正常切换到下一段时保持使能；被停止、打断或报错时立即失能。 */
static void BrushlessMotorTest_ActiveExit(Mission_ExitReason_t reason)
{
    if (reason != MISSION_EXIT_COMPLETED)
    {
        (void)Gimbal_Disable();
    }
}

static Mission_CallbackResult_t BrushlessMotorTest_ErrorEnter(void)
{
    /* Gimbal 自身已报错时硬件已经失能，保留错误码供 OLED 诊断。 */
    if (Gimbal_GetState() != GIMBAL_STATE_ERROR)
    {
        (void)Gimbal_Disable();
    }
    return MISSION_CALLBACK_OK;
}

static uint8_t BrushlessMotorTest_ConditionStop(
    const Mission_Runtime_t *runtime,
    const App_UpdateContext_t *updateContext)
{
    (void)runtime;
    if (updateContext == NULL)
    {
        return 0U;
    }
    return ((updateContext->pressedEdges &
             BRUSHLESS_MOTOR_TEST_STOP_KEY_MASK) != 0U) ? 1U : 0U;
}

static const Mission_Transition_t s_zeroTransitions[] = {
    {
        BrushlessMotorTest_ConditionStop,
        BRUSHLESS_MOTOR_TEST_STATE_STOPPED,
        MISSION_TRANSITION_INTERRUPT,
    },
    {
        NULL,
        BRUSHLESS_MOTOR_TEST_STATE_Y_TO_180,
        MISSION_TRANSITION_NORMAL,
    },
};

static const Mission_Transition_t s_yTo180Transitions[] = {
    {
        BrushlessMotorTest_ConditionStop,
        BRUSHLESS_MOTOR_TEST_STATE_STOPPED,
        MISSION_TRANSITION_INTERRUPT,
    },
    {
        NULL,
        BRUSHLESS_MOTOR_TEST_STATE_Y_TO_0,
        MISSION_TRANSITION_NORMAL,
    },
};

static const Mission_Transition_t s_yTo0Transitions[] = {
    {
        BrushlessMotorTest_ConditionStop,
        BRUSHLESS_MOTOR_TEST_STATE_STOPPED,
        MISSION_TRANSITION_INTERRUPT,
    },
    {
        NULL,
        BRUSHLESS_MOTOR_TEST_STATE_Y_TO_180,
        MISSION_TRANSITION_NORMAL,
    },
};

static const Mission_Transition_t s_stoppedTransitions[] = {
    {
        NULL,
        BRUSHLESS_MOTOR_TEST_STATE_ZERO,
        MISSION_TRANSITION_NORMAL,
    },
};

static const Mission_StateDefinition_t s_states[
    BRUSHLESS_MOTOR_TEST_STATE_COUNT] = {
    [BRUSHLESS_MOTOR_TEST_STATE_ZERO] = {
        .onEnter = BrushlessMotorTest_ZeroEnter,
        .onUpdate = BrushlessMotorTest_MoveUpdate,
        .onExit = BrushlessMotorTest_ActiveExit,
        .transitions = s_zeroTransitions,
        .transitionCount =
            BRUSHLESS_MOTOR_TEST_ARRAY_COUNT(s_zeroTransitions),
        .interruptible = 1U,
    },
    [BRUSHLESS_MOTOR_TEST_STATE_Y_TO_180] = {
        .onEnter = BrushlessMotorTest_YTo180Enter,
        .onUpdate = BrushlessMotorTest_MoveUpdate,
        .onExit = BrushlessMotorTest_ActiveExit,
        .transitions = s_yTo180Transitions,
        .transitionCount =
            BRUSHLESS_MOTOR_TEST_ARRAY_COUNT(s_yTo180Transitions),
        .interruptible = 1U,
    },
    [BRUSHLESS_MOTOR_TEST_STATE_Y_TO_0] = {
        .onEnter = BrushlessMotorTest_YTo0Enter,
        .onUpdate = BrushlessMotorTest_MoveUpdate,
        .onExit = BrushlessMotorTest_ActiveExit,
        .transitions = s_yTo0Transitions,
        .transitionCount =
            BRUSHLESS_MOTOR_TEST_ARRAY_COUNT(s_yTo0Transitions),
        .interruptible = 1U,
    },
    [BRUSHLESS_MOTOR_TEST_STATE_STOPPED] = {
        .onEnter = BrushlessMotorTest_StoppedEnter,
        .onUpdate = BrushlessMotorTest_StoppedUpdate,
        .onExit = NULL,
        .transitions = s_stoppedTransitions,
        .transitionCount =
            BRUSHLESS_MOTOR_TEST_ARRAY_COUNT(s_stoppedTransitions),
        .interruptible = 0U,
    },
    [BRUSHLESS_MOTOR_TEST_STATE_ERROR] = {
        .onEnter = BrushlessMotorTest_ErrorEnter,
        .onUpdate = NULL,
        .onExit = NULL,
        .transitions = NULL,
        .transitionCount = 0U,
        .interruptible = 0U,
    },
};

static const Mission_GraphDefinition_t s_graph = {
    .states = s_states,
    .stateCount = BRUSHLESS_MOTOR_TEST_STATE_COUNT,
    .startState = BRUSHLESS_MOTOR_TEST_STATE_STOPPED,
    .errorState = BRUSHLESS_MOTOR_TEST_STATE_ERROR,
};

const Mission_GraphDefinition_t *
BrushlessMotorTest_GetMissionGraph(void)
{
    return &s_graph;
}
