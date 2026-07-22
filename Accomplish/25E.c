#include "Accomplish/25E.h"
#include "Application/Control/MotionManager.h"
#include "Application/State/Heading.h"
#include "Hardware/Sensors/Graydetect.h"
#include <stddef.h>

/*
 * 25E 状态图全部保存在本文件，不向其他模块公开状态编号或回调。
 * Mission 负责执行状态图，MotionManager 负责保证只有一种运动占用双轮。
 *
 * 状态流程：
 * WAITING --KEY1 按下--> STRAIGHT --检测到黑线--> LINE
 * STRAIGHT --走满 2000 mm 未找到线--> WAITING
 * LINE --连续丢线 50 拍--> TURN --到达启动航向 + 180°--> STRAIGHT
 */

/* 根据静态数组自动计算转换数量，避免手工填写后与数组长度不一致。 */
#define ACCOMPLISH_25E_ARRAY_COUNT(array) \
    ((uint8_t)(sizeof(array) / sizeof((array)[0])))

/* 状态编号只在 25E 内部使用，最后的 COUNT 用于定义和校验状态表长度。 */
typedef enum
{
    ACCOMPLISH_25E_STATE_WAITING = 0, /* 等待 KEY1 按下，不驱动车轮。 */
    ACCOMPLISH_25E_STATE_STRAIGHT,    /* 直行并在途中检查灰度黑线。 */
    ACCOMPLISH_25E_STATE_LINE,        /* 使用五路灰度持续巡线。 */
    ACCOMPLISH_25E_STATE_TURN,        /* 指向启动航向加 180° 的绝对目标。 */
    ACCOMPLISH_25E_STATE_ERROR,       /* 运动启动或运行异常后的安全状态。 */
    ACCOMPLISH_25E_STATE_COUNT
} Accomplish25E_StateId_t;

/*
 * 直线阶段的黑线连续检测计数。
 * 进入每一轮直线状态时清零，防止上一轮残留值直接触发巡线。
 */
static uint8_t s_lineDetectedTicks;

/*
 * KEY1 启动本轮任务时的连续航向角。
 * Heading 的角度不做正负 180° 限幅，因此这里保存的也是连续绝对角。
 */
static float s_startYawDeg;
static uint8_t s_startYawCaptured;

/*
 * 每次回到等待状态都清除上一轮任务的航向基准。
 * 下一次按 KEY1 进入直线状态时会重新记录当前航向。
 */
static Mission_CallbackResult_t Accomplish25E_WaitingEnter(void)
{
    s_startYawDeg = 0.0f;
    s_startYawCaptured = 0U;
    return MISSION_CALLBACK_OK;
}

/*
 * 等待状态必须持续返回 RUNNING。
 * 如果 onUpdate 为空或返回 FINISHED，Mission 将不会检查 KEY1 打断转换。
 */
static Mission_ActionStatus_t Accomplish25E_WaitingUpdate(float dt)
{
    (void)dt;
    return MISSION_ACTION_RUNNING;
}

/*
 * 每次进入直线状态只调用一次。
 * 先清除入线确认计数，再启动最大距离 2000 mm 的直线运动。
 * MotionManager 启动失败时返回 ERROR，由 Mission 停车并进入错误状态。
 */
static Mission_CallbackResult_t Accomplish25E_StraightEnter(void)
{
    s_lineDetectedTicks = 0U;

    if (s_startYawCaptured == 0U)
    {
        /* 首次直行开始前记录 KEY1 启动时的航向，后续循环不再覆盖。 */
        s_startYawDeg = Heading_GetYaw();
        s_startYawCaptured = 1U;
    }

    return (MotionManager_StartForward(
                ACCOMPLISH_25E_STRAIGHT_DISTANCE_MM,
                ACCOMPLISH_25E_STRAIGHT_SPEED_MMPS,
                ACCOMPLISH_25E_STRAIGHT_END_SPEED_MMPS) ==
            MOTION_MANAGER_RESULT_OK) ?
        MISSION_CALLBACK_OK : MISSION_CALLBACK_ERROR;
}

/*
 * 检测到黑线后进入本状态，只调用一次 MotionManager_StartLine()。
 * MotionLine 内部完成五路权重差速，并在连续全白 50 拍后返回 FINISHED。
 */
static Mission_CallbackResult_t Accomplish25E_LineEnter(void)
{
    return (MotionManager_StartLine(
                ACCOMPLISH_25E_LINE_SPEED_MMPS) ==
            MOTION_MANAGER_RESULT_OK) ?
        MISSION_CALLBACK_OK : MISSION_CALLBACK_ERROR;
}

/*
 * 巡线完成后进入本状态，在上一绝对目标上增加 180°。
 * 使用 TurnTo 依次指向启动航向 +180°、+360°、+540°……
 */
static Mission_CallbackResult_t Accomplish25E_TurnEnter(void)
{
    if (s_startYawCaptured == 0U)
    {
        /* 未保存启动航向说明状态流程异常，禁止使用错误基准启动转向。 */
        return MISSION_CALLBACK_ERROR;
    }

    return (MotionManager_TurnTo(
                s_startYawDeg += ACCOMPLISH_25E_TURN_TARGET_OFFSET_DEG,
                ACCOMPLISH_25E_TURN_SPEED_MMPS) ==
            MOTION_MANAGER_RESULT_OK) ?
        MISSION_CALLBACK_OK : MISSION_CALLBACK_ERROR;
}

/*
 * 直线、巡线和转向共用的状态更新回调。
 * 底层运动已经由 App_Update() 调用 MotionManager_Update(dt) 更新，
 * 此处只查询动作是运行、完成还是错误，不能再次更新底层控制器。
 */
static Mission_ActionStatus_t Accomplish25E_MotionUpdate(float dt)
{
    (void)dt;
    return Mission_GetMotionActionStatus();
}

/*
 * 等待状态的启动条件。
 * App 只在 KEY1 从松开变为按下的那一拍设置 pressedEdges bit0。
 * 使用按下沿而不是 pressedKeys，可以避免长按 KEY1 时反复启动流程。
 */
static uint8_t Accomplish25E_ConditionStart(
    const Mission_Runtime_t *runtime,
    const App_UpdateContext_t *updateContext)
{
    (void)runtime;
    if (updateContext == NULL)
    {
        return 0U;
    }

    return ((updateContext->pressedEdges &
             ACCOMPLISH_25E_START_KEY_MASK) != 0U) ? 1U : 0U;
}

/*
 * 直线阶段的入线条件：五路灰度任意一路为 1 都表示检测到黑线。
 * 为避免单拍毛刺误触发，必须连续达到头文件设定的确认节拍数。
 * 使用 updateContext->elapsedTicks 累加真实经过的系统拍数，主循环偶尔延迟时
 * 仍能保持正确的确认时间，而不是固定按一次函数调用等于 10 ms 计算。
 */
static uint8_t Accomplish25E_ConditionLineDetected(
    const Mission_Runtime_t *runtime,
    const App_UpdateContext_t *updateContext)
{
    uint16_t detectedTicks;

    (void)runtime;
    if (updateContext == NULL)
    {
        return 0U;
    }

    if (Graydetect_OnLine(GRAY_SIDE_ALL) == 0U)
    {
        /* 任意一拍恢复全白都重新开始连续检测。 */
        s_lineDetectedTicks = 0U;
        return 0U;
    }

    detectedTicks = (uint16_t)s_lineDetectedTicks +
                    updateContext->elapsedTicks;
    if (detectedTicks >= ACCOMPLISH_25E_LINE_DETECT_CONFIRM_TICKS)
    {
        /* 达到阈值后进行饱和，避免计数继续累加溢出。 */
        s_lineDetectedTicks =
            ACCOMPLISH_25E_LINE_DETECT_CONFIRM_TICKS;
        return 1U;
    }

    s_lineDetectedTicks = (uint8_t)detectedTicks;
    return 0U;
}

/* WAITING 持续运行，因此 KEY1 按下沿使用打断转换进入第一轮直线状态。 */
static const Mission_Transition_t s_waitingTransitions[] = {
    {
        Accomplish25E_ConditionStart,
        ACCOMPLISH_25E_STATE_STRAIGHT,
        MISSION_TRANSITION_INTERRUPT,
    },
};

/*
 * 直线状态共有三条转换：
 * 1. 运动未完成时检测到线，立即打断直线；
 * 2. 黑线与距离完成出现在同一拍时，仍优先进入巡线；
 * 3. 走满距离仍无黑线时，无条件回到等待状态。
 * Mission 按数组声明顺序检查，因此入线条件必须写在兜底转换之前。
 */
static const Mission_Transition_t s_straightTransitions[] = {
    /* 2000 mm 内确认检测到黑线时，立即打断直线并开始巡线。 */
    {
        Accomplish25E_ConditionLineDetected,
        ACCOMPLISH_25E_STATE_LINE,
        MISSION_TRANSITION_INTERRUPT,
    },
    /* 黑线恰好在直线完成的同一拍出现时，仍优先进入巡线。 */
    {
        Accomplish25E_ConditionLineDetected,
        ACCOMPLISH_25E_STATE_LINE,
        MISSION_TRANSITION_NORMAL,
    },
    /* 走满 2000 mm 仍未检测到线，停车并回到等待状态。 */
    {
        NULL,
        ACCOMPLISH_25E_STATE_WAITING,
        MISSION_TRANSITION_NORMAL,
    },
};

/*
 * 丢线是“巡线直到丢线”动作的预期结束条件，不属于系统故障。
 * MotionLine 返回 FINISHED 后，Mission 检查本正常转换并进入转向状态。
 */
static const Mission_Transition_t s_lineTransitions[] = {
    /* MotionLine 连续丢线 50 拍后完成，随后进入绝对目标转向。 */
    {
        NULL,
        ACCOMPLISH_25E_STATE_TURN,
        MISSION_TRANSITION_NORMAL,
    },
};

/* Nav 到达本轮绝对目标并稳定后返回 FINISHED，无条件开始下一轮直线。 */
static const Mission_Transition_t s_turnTransitions[] = {
    /* 到达目标后重新直行；下一轮 TURN 会再增加 180°。 */
    {
        NULL,
        ACCOMPLISH_25E_STATE_STRAIGHT,
        MISSION_TRANSITION_NORMAL,
    },
};

/*
 * 25E 状态定义表：
 * - onEnter 只负责启动一次动作；
 * - onUpdate 只返回动作状态；
 * - onExit 为空，因为 Mission 转换时会统一调用 MotionManager_Stop()；
 * - interruptible 只控制状态图中的普通打断转换；蓝牙命令只在起始等待状态执行。
 */
static const Mission_StateDefinition_t s_stateDefinitions[
    ACCOMPLISH_25E_STATE_COUNT] = {
    [ACCOMPLISH_25E_STATE_WAITING] = {
        /* 清除旧航向基准，并保持 RUNNING 以接收 KEY1 按下沿。 */
        .onEnter = Accomplish25E_WaitingEnter,
        .onUpdate = Accomplish25E_WaitingUpdate,
        .onExit = NULL,
        .transitions = s_waitingTransitions,
        .transitionCount =
            ACCOMPLISH_25E_ARRAY_COUNT(s_waitingTransitions),
        .interruptible = 1U,
    },
    [ACCOMPLISH_25E_STATE_STRAIGHT] = {
        /* 允许灰度入线条件在直线完成前打断当前动作。 */
        .onEnter = Accomplish25E_StraightEnter,
        .onUpdate = Accomplish25E_MotionUpdate,
        .onExit = NULL,
        .transitions = s_straightTransitions,
        .transitionCount =
            ACCOMPLISH_25E_ARRAY_COUNT(s_straightTransitions),
        .interruptible = 1U,
    },
    [ACCOMPLISH_25E_STATE_LINE] = {
        /* 巡线通过正常完成进入转向，不设置普通打断转换。 */
        .onEnter = Accomplish25E_LineEnter,
        .onUpdate = Accomplish25E_MotionUpdate,
        .onExit = NULL,
        .transitions = s_lineTransitions,
        .transitionCount =
            ACCOMPLISH_25E_ARRAY_COUNT(s_lineTransitions),
        .interruptible = 0U,
    },
    [ACCOMPLISH_25E_STATE_TURN] = {
        /* 转向完成后使用正常转换进入下一轮直线。 */
        .onEnter = Accomplish25E_TurnEnter,
        .onUpdate = Accomplish25E_MotionUpdate,
        .onExit = NULL,
        .transitions = s_turnTransitions,
        .transitionCount =
            ACCOMPLISH_25E_ARRAY_COUNT(s_turnTransitions),
        .interruptible = 0U,
    },
    [ACCOMPLISH_25E_STATE_ERROR] = {
        /* 错误状态不启动运动；可通过 Mission_Stop() 复位到 WAITING。 */
        .onEnter = NULL,
        .onUpdate = NULL,
        .onExit = NULL,
        .transitions = NULL,
        .transitionCount = 0U,
        .interruptible = 0U,
    },
};

/*
 * 提供给通用 Mission 的图描述。
 * Mission_Init() 会检查状态数量、起始/错误状态和所有转换目标是否合法。
 */
static const Mission_GraphDefinition_t s_missionGraph = {
    .states = s_stateDefinitions,
    .stateCount = ACCOMPLISH_25E_STATE_COUNT,
    .startState = ACCOMPLISH_25E_STATE_WAITING,
    .errorState = ACCOMPLISH_25E_STATE_ERROR,
};

/* 唯一跨模块接口：返回静态只读图，不能返回函数内部的临时变量。 */
const Mission_GraphDefinition_t *Accomplish25E_GetMissionGraph(void)
{
    return &s_missionGraph;
}
