#include "Accomplish/25H.h"
#include "Application/Control/MotionManager.h"
#include "Application/State/Heading.h"
#include "Hardware/Sensors/Graydetect.h"
#include <stddef.h>

/*
 * 25H 只描述题目流程，不直接更新底层运动控制器。
 * App 每拍更新 MotionManager，Mission 再根据本状态图决定是否切换动作。
 *
 * 状态流程：
 * WAITING --KEY1--> LINE --左侧两路同时为 1--> FORWARD
 * FORWARD --完成 150 mm--> TURN --到达绝对目标--> LINE
 * LINE --连续丢线并停止--> WAITING
 */

#define ACCOMPLISH_25H_ARRAY_COUNT(array) \
    ((uint8_t)(sizeof(array) / sizeof((array)[0])))

typedef enum
{
    ACCOMPLISH_25H_STATE_WAITING = 0, /* 等待 KEY1，不驱动车轮。 */
    ACCOMPLISH_25H_STATE_LINE,        /* 使用五路灰度巡线。 */
    ACCOMPLISH_25H_STATE_FORWARD,     /* 离开标志线并直行 150 mm。 */
    ACCOMPLISH_25H_STATE_TURN,        /* 转到下一绝对左转目标。 */
    ACCOMPLISH_25H_STATE_ERROR,       /* 运动异常后的安全停车状态。 */
    ACCOMPLISH_25H_STATE_COUNT
} Accomplish25H_StateId_t;

/*
 * s_nextTurnTargetDeg 保存下一次 TurnTo() 使用的连续绝对角。
 * KEY1 启动时以当前航向作为 0°基准；每进入一次 TURN 就减去 90°。
 */
static float s_nextTurnTargetDeg;
static uint8_t s_headingReferenceCaptured;

/* 返回等待状态时清除上一轮任务的航向基准。 */
static Mission_CallbackResult_t Accomplish25H_WaitingEnter(void)
{
    s_nextTurnTargetDeg = 0.0f;
    s_headingReferenceCaptured = 0U;
    return MISSION_CALLBACK_OK;
}

/* 等待状态保持 RUNNING，才能检查 KEY1 的打断转换。 */
static Mission_ActionStatus_t Accomplish25H_WaitingUpdate(float dt)
{
    (void)dt;
    return MISSION_ACTION_RUNNING;
}

/*
 * 首次进入巡线时记录 KEY1 启动方向；后续循环不再覆盖该基准。
 * MotionLine 连续丢线达到公共配置值后会正常结束并回到等待。
 */
static Mission_CallbackResult_t Accomplish25H_LineEnter(void)
{
    if (s_headingReferenceCaptured == 0U)
    {
        if (Heading_IsReady() == 0U)
        {
            /* 本题后续必须使用绝对角转向，MPU 离线时禁止启动车辆。 */
            return MISSION_CALLBACK_ERROR;
        }
        s_nextTurnTargetDeg = Heading_GetYaw();
        s_headingReferenceCaptured = 1U;
    }

    return (MotionManager_StartLine(
                ACCOMPLISH_25H_LINE_SPEED_MMPS) ==
            MOTION_MANAGER_RESULT_OK) ?
        MISSION_CALLBACK_OK : MISSION_CALLBACK_ERROR;
}

/* 检测到左侧标志后，使用航向闭环向前直行 150 mm 并平滑停车。 */
static Mission_CallbackResult_t Accomplish25H_ForwardEnter(void)
{
    return (MotionManager_StartForward(
                ACCOMPLISH_25H_FORWARD_DISTANCE_MM,
                ACCOMPLISH_25H_FORWARD_SPEED_MMPS,
                ACCOMPLISH_25H_FORWARD_END_SPEED_MMPS) ==
            MOTION_MANAGER_RESULT_OK) ?
        MISSION_CALLBACK_OK : MISSION_CALLBACK_ERROR;
}

/*
 * 每次进入 TURN 都先生成新的绝对目标，再调用 TurnTo()。
 * 例如启动航向为 12°，目标依次为 -78°、-168°、-258°……
 */
static Mission_CallbackResult_t Accomplish25H_TurnEnter(void)
{
    if (s_headingReferenceCaptured == 0U)
    {
        return MISSION_CALLBACK_ERROR;
    }

    s_nextTurnTargetDeg += ACCOMPLISH_25H_TURN_STEP_DEG;
    return (MotionManager_TurnTo(
                s_nextTurnTargetDeg,
                ACCOMPLISH_25H_TURN_SPEED_MMPS) ==
            MOTION_MANAGER_RESULT_OK) ?
        MISSION_CALLBACK_OK : MISSION_CALLBACK_ERROR;
}

/* MotionManager 已由 App 更新，本回调只读取当前动作状态。 */
static Mission_ActionStatus_t Accomplish25H_MotionUpdate(float dt)
{
    (void)dt;
    return Mission_GetMotionActionStatus();
}

/* KEY1 按下沿启动；长按不会重复进入巡线。 */
static uint8_t Accomplish25H_ConditionStart(
    const Mission_Runtime_t *runtime,
    const App_UpdateContext_t *updateContext)
{
    (void)runtime;
    if (updateContext == NULL)
    {
        return 0U;
    }

    return ((updateContext->pressedEdges &
             ACCOMPLISH_25H_START_KEY_MASK) != 0U) ? 1U : 0U;
}

/*
 * 左侧标志条件必须同时满足 bit0=1 和 bit1=1。
 * 条件在同一拍立即生效，避免 MotionLine 继续把双黑线当作普通左偏处理。
 */
static uint8_t Accomplish25H_ConditionLeftMarker(
    const Mission_Runtime_t *runtime,
    const App_UpdateContext_t *updateContext)
{
    uint8_t grayState;

    (void)runtime;
    (void)updateContext;
    grayState = Graydetect_GetState();
    return ((grayState & ACCOMPLISH_25H_LEFT_MARKER_MASK) ==
            ACCOMPLISH_25H_LEFT_MARKER_MASK) ? 1U : 0U;
}

static const Mission_Transition_t s_waitingTransitions[] = {
    {
        Accomplish25H_ConditionStart,
        ACCOMPLISH_25H_STATE_LINE,
        MISSION_TRANSITION_INTERRUPT,
    },
};

static const Mission_Transition_t s_lineTransitions[] = {
    /* 巡线尚在运行时，左侧双黑线立即打断并进入定距直行。 */
    {
        Accomplish25H_ConditionLeftMarker,
        ACCOMPLISH_25H_STATE_FORWARD,
        MISSION_TRANSITION_INTERRUPT,
    },
    /* 非标志原因导致 MotionLine 丢线完成时，停车并重新等待 KEY1。 */
    {
        NULL,
        ACCOMPLISH_25H_STATE_WAITING,
        MISSION_TRANSITION_NORMAL,
    },
};

static const Mission_Transition_t s_forwardTransitions[] = {
    /* 零速目标固定保持结束后，直接进入绝对角转向。 */
    {
        NULL,
        ACCOMPLISH_25H_STATE_TURN,
        MISSION_TRANSITION_NORMAL,
    },
};

static const Mission_Transition_t s_turnTransitions[] = {
    /* Nav 到达本轮绝对目标并稳定后，继续下一轮巡线。 */
    {
        NULL,
        ACCOMPLISH_25H_STATE_LINE,
        MISSION_TRANSITION_NORMAL,
    },
};

static const Mission_StateDefinition_t s_stateDefinitions[
    ACCOMPLISH_25H_STATE_COUNT] = {
    [ACCOMPLISH_25H_STATE_WAITING] = {
        .onEnter = Accomplish25H_WaitingEnter,
        .onUpdate = Accomplish25H_WaitingUpdate,
        .onExit = NULL,
        .transitions = s_waitingTransitions,
        .transitionCount =
            ACCOMPLISH_25H_ARRAY_COUNT(s_waitingTransitions),
        .interruptible = 1U,
    },
    [ACCOMPLISH_25H_STATE_LINE] = {
        .onEnter = Accomplish25H_LineEnter,
        .onUpdate = Accomplish25H_MotionUpdate,
        .onExit = NULL,
        .transitions = s_lineTransitions,
        .transitionCount =
            ACCOMPLISH_25H_ARRAY_COUNT(s_lineTransitions),
        .interruptible = 1U,
    },
    [ACCOMPLISH_25H_STATE_FORWARD] = {
        .onEnter = Accomplish25H_ForwardEnter,
        .onUpdate = Accomplish25H_MotionUpdate,
        .onExit = NULL,
        .transitions = s_forwardTransitions,
        .transitionCount =
            ACCOMPLISH_25H_ARRAY_COUNT(s_forwardTransitions),
        .interruptible = 0U,
    },
    [ACCOMPLISH_25H_STATE_TURN] = {
        .onEnter = Accomplish25H_TurnEnter,
        .onUpdate = Accomplish25H_MotionUpdate,
        .onExit = NULL,
        .transitions = s_turnTransitions,
        .transitionCount =
            ACCOMPLISH_25H_ARRAY_COUNT(s_turnTransitions),
        .interruptible = 0U,
    },
    [ACCOMPLISH_25H_STATE_ERROR] = {
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
    .stateCount = ACCOMPLISH_25H_STATE_COUNT,
    .startState = ACCOMPLISH_25H_STATE_WAITING,
    .errorState = ACCOMPLISH_25H_STATE_ERROR,
};

const Mission_GraphDefinition_t *Accomplish25H_GetMissionGraph(void)
{
    return &s_missionGraph;
}
