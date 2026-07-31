#include "Application/Control/BallHold.h"
#include "Application/Control/BallBalance.h"
#include "Application/Control/MotionLine.h"
#include "tests/host/test_assert.h"

/* ---- BallHold 只依赖 BallBalance 和 MotionLine 的一个 getter ---- */
static BallBalance_State_t s_balanceState;
static BallBalance_Error_t s_balanceError;
static BallBalance_Result_t s_startResult;
static uint8_t s_stable;
static float s_targetMM;
static float s_carAccelerationMMps2;
static uint16_t s_startCount;
static uint16_t s_stopCount;
static uint16_t s_updateCount;
static uint16_t s_setTargetCount;
/* MotionLine 的规划加速度：静止测试恒为 0，接 AB 直线后才非零。 */
static float s_profileAccelerationMMps2;

void BallBalance_Init(void)
{
    s_balanceState = BALL_BALANCE_STATE_IDLE;
    s_balanceError = BALL_BALANCE_ERROR_NONE;
}

BallBalance_Result_t BallBalance_Start(float targetMM)
{
    s_startCount++;
    if (s_startResult == BALL_BALANCE_RESULT_OK)
    {
        s_balanceState = BALL_BALANCE_STATE_RUNNING;
        s_targetMM = targetMM;
    }
    return s_startResult;
}

BallBalance_Result_t BallBalance_SetTarget(float targetMM)
{
    if (s_balanceState != BALL_BALANCE_STATE_RUNNING)
    {
        return BALL_BALANCE_RESULT_NOT_RUNNING;
    }
    s_targetMM = targetMM;
    s_setTargetCount++;
    s_stable = 0U;
    return BALL_BALANCE_RESULT_OK;
}

void BallBalance_SetCarAcceleration(float accelerationMMps2)
{
    s_carAccelerationMMps2 = accelerationMMps2;
}

void BallBalance_Update(float dt)
{
    (void)dt;
    s_updateCount++;
}

void BallBalance_Stop(void)
{
    s_stopCount++;
    s_balanceState = BALL_BALANCE_STATE_IDLE;
}

BallBalance_State_t BallBalance_GetState(void) { return s_balanceState; }
BallBalance_Error_t BallBalance_GetError(void) { return s_balanceError; }
uint8_t BallBalance_IsStable(void) { return s_stable; }

float MotionLine_GetProfileAccelerationMMps2(void)
{
    return s_profileAccelerationMMps2;
}

static void reset_fakes(void)
{
    s_balanceState = BALL_BALANCE_STATE_IDLE;
    s_balanceError = BALL_BALANCE_ERROR_NONE;
    s_startResult = BALL_BALANCE_RESULT_OK;
    s_stable = 0U;
    s_targetMM = 999.0f;
    s_carAccelerationMMps2 = 0.0f;
    s_startCount = 0U;
    s_stopCount = 0U;
    s_updateCount = 0U;
    s_setTargetCount = 0U;
    s_profileAccelerationMMps2 = 0.0f;
    BallHold_Init();
}

static void run_ticks(uint16_t ticks)
{
    uint16_t tick;

    for (tick = 0U; tick < ticks; tick++)
    {
        BallHold_Update(0.01f);
    }
}

/* 要求 4 的目标恒为 O 点，不是 ±5 cm 中的任何一个。 */
static void test_start_targets_o_point(void)
{
    reset_fakes();
    CHECK(BallHold_Start() != 0U);

    CHECK(s_startCount == 1U);
    CHECK(BallHold_GetState() ==
          BALL_HOLD_STATE_CONVERGING);
    CHECK_NEAR(s_targetMM, 0.0f, 0.001f);
    CHECK(BallHold_IsActive() != 0U);
}

/* 看不到钢球时不允许起步，否则第一拍就是拿无效位置做闭环。 */
static void test_start_fails_without_vision(void)
{
    reset_fakes();
    s_startResult = BALL_BALANCE_RESULT_INVALID_ARGUMENT;

    CHECK(BallHold_Start() == 0U);
    CHECK(BallHold_GetState() ==
          BALL_HOLD_STATE_ERROR);
    CHECK(BallHold_GetError() ==
          BALL_HOLD_ERROR_VISION);
    CHECK(BallHold_IsActive() == 0U);
}

/* 稳住之后进入保持，并且记录收敛耗时。 */
static void test_enters_hold_after_stable(void)
{
    reset_fakes();
    CHECK(BallHold_Start() != 0U);

    run_ticks(30U);
    CHECK(BallHold_GetState() ==
          BALL_HOLD_STATE_CONVERGING);
    CHECK(BallHold_GetConvergeTicks() == 0U);

    s_stable = 1U;
    BallHold_Update(0.01f);
    CHECK(BallHold_GetState() ==
          BALL_HOLD_STATE_HOLDING);
    CHECK(BallHold_GetConvergeTicks() == 31U);
}

/*
 * 保持阶段必须一直闭环，而且不能改目标：一停控摆杆回中，球就滚走；
 * 重设目标会白白清掉 BallBalance 的稳定计数。
 */
static void test_hold_keeps_closed_loop_at_o(void)
{
    uint16_t updatesBefore;
    uint16_t setTargetBefore;

    reset_fakes();
    CHECK(BallHold_Start() != 0U);
    s_stable = 1U;
    BallHold_Update(0.01f);
    CHECK(BallHold_GetState() ==
          BALL_HOLD_STATE_HOLDING);

    updatesBefore = s_updateCount;
    setTargetBefore = s_setTargetCount;
    run_ticks(200U);

    CHECK(s_updateCount == (uint16_t)(updatesBefore + 200U));
    CHECK(s_setTargetCount == setTargetBefore);
    CHECK(s_stopCount == 0U);
    CHECK_NEAR(s_targetMM, 0.0f, 0.001f);
    CHECK(BallHold_GetState() ==
          BALL_HOLD_STATE_HOLDING);
}

/* 球被拨走后不该退出保持：BallBalance 自己会把它拉回来。 */
static void test_hold_survives_disturbance(void)
{
    reset_fakes();
    CHECK(BallHold_Start() != 0U);
    s_stable = 1U;
    BallHold_Update(0.01f);
    CHECK(BallHold_GetState() ==
          BALL_HOLD_STATE_HOLDING);

    /* 模拟手拨：稳定标志掉了，但任务状态必须留在 HOLDING。 */
    s_stable = 0U;
    run_ticks(300U);
    CHECK(BallHold_GetState() ==
          BALL_HOLD_STATE_HOLDING);
    CHECK(s_stopCount == 0U);
}

/* 静止时前馈必须是 0，否则摆杆会凭空歪一个角度。 */
static void test_feedforward_is_zero_while_car_is_still(void)
{
    reset_fakes();
    CHECK(BallHold_Start() != 0U);
    CHECK_NEAR(s_carAccelerationMMps2, 0.0f, 0.001f);

    run_ticks(20U);
    CHECK_NEAR(s_carAccelerationMMps2, 0.0f, 0.001f);
    CHECK_NEAR(BallHold_GetFeedforwardMMps2(), 0.0f, 0.001f);
}

/*
 * 本阶段只做静止 O 点保持。即便底盘模块残留了非零规划加速度，也绝不能
 * 把它送入摆杆闭环；AB 直线阶段以后再通过独立入口接入前馈。
 */
static void test_hold_ignores_motion_line_acceleration(void)
{
    reset_fakes();
    CHECK(BallHold_Start() != 0U);

    s_profileAccelerationMMps2 = 300.0f;
    BallHold_Update(0.01f);
    CHECK_NEAR(s_carAccelerationMMps2, 0.0f, 0.001f);
    CHECK_NEAR(BallHold_GetFeedforwardMMps2(), 0.0f, 0.001f);

    s_profileAccelerationMMps2 = -240.0f;
    BallHold_Update(0.01f);
    CHECK_NEAR(s_carAccelerationMMps2, 0.0f, 0.001f);
    CHECK_NEAR(BallHold_GetFeedforwardMMps2(), 0.0f, 0.001f);
}

/* 非有限的规划加速度绝不能进闭环，否则倾角立刻变成 NaN。 */
static void test_feedforward_rejects_non_finite(void)
{
    reset_fakes();
    CHECK(BallHold_Start() != 0U);

    s_profileAccelerationMMps2 = NAN;
    BallHold_Update(0.01f);
    CHECK_NEAR(s_carAccelerationMMps2, 0.0f, 0.001f);

    s_profileAccelerationMMps2 = INFINITY;
    BallHold_Update(0.01f);
    CHECK_NEAR(s_carAccelerationMMps2, 0.0f, 0.001f);
}

/* 中途丢视觉要停控回中并报错。 */
static void test_vision_loss_aborts(void)
{
    reset_fakes();
    CHECK(BallHold_Start() != 0U);
    run_ticks(10U);

    s_balanceState = BALL_BALANCE_STATE_ERROR;
    s_balanceError = BALL_BALANCE_ERROR_VISION_LOST;
    BallHold_Update(0.01f);

    CHECK(BallHold_GetState() ==
          BALL_HOLD_STATE_ERROR);
    CHECK(BallHold_GetError() ==
          BALL_HOLD_ERROR_VISION);
    CHECK(s_stopCount == 1U);
    CHECK_NEAR(BallHold_GetFeedforwardMMps2(), 0.0f, 0.001f);
}

/* 保持阶段丢视觉同样要回中：这时倾角锁死最危险。 */
static void test_vision_loss_during_hold_aborts(void)
{
    reset_fakes();
    CHECK(BallHold_Start() != 0U);
    s_stable = 1U;
    BallHold_Update(0.01f);
    CHECK(BallHold_GetState() ==
          BALL_HOLD_STATE_HOLDING);

    s_balanceState = BALL_BALANCE_STATE_ERROR;
    s_balanceError = BALL_BALANCE_ERROR_VISION_LOST;
    BallHold_Update(0.01f);

    CHECK(BallHold_GetState() ==
          BALL_HOLD_STATE_ERROR);
    CHECK(s_stopCount == 1U);
}

/* 一直收敛不了要回中报错，不能让摆杆无限期顶着一个倾角。 */
static void test_converge_timeout_aborts(void)
{
    reset_fakes();
    CHECK(BallHold_Start() != 0U);

    /* 默认 8 s，100 Hz 下 800 拍；多跑几拍确保跨过阈值。 */
    run_ticks(810U);

    CHECK(BallHold_GetState() ==
          BALL_HOLD_STATE_ERROR);
    CHECK(BallHold_GetError() ==
          BALL_HOLD_ERROR_CONVERGE_TIMEOUT);
    CHECK(s_stopCount == 1U);
}

/* 已经稳住之后就不该再被收敛超时打断——长期保持才是要求 4 的目标。 */
static void test_hold_never_times_out(void)
{
    reset_fakes();
    CHECK(BallHold_Start() != 0U);
    s_stable = 1U;
    BallHold_Update(0.01f);
    CHECK(BallHold_GetState() ==
          BALL_HOLD_STATE_HOLDING);

    run_ticks(3000U);
    CHECK(BallHold_GetState() ==
          BALL_HOLD_STATE_HOLDING);
    CHECK(BallHold_GetError() ==
          BALL_HOLD_ERROR_NONE);
    CHECK(s_stopCount == 0U);
}

static void test_stop_recenters_and_finishes(void)
{
    reset_fakes();
    CHECK(BallHold_Start() != 0U);
    run_ticks(10U);

    BallHold_Stop();
    CHECK(s_stopCount == 1U);
    CHECK(BallHold_GetState() ==
          BALL_HOLD_STATE_FINISHED);
    CHECK(BallHold_IsActive() == 0U);

    /* 停止后继续调 Update 不能把闭环又跑起来。 */
    run_ticks(10U);
    CHECK(BallHold_GetState() ==
          BALL_HOLD_STATE_FINISHED);
}

static void test_update_before_start_does_nothing(void)
{
    reset_fakes();
    run_ticks(10U);

    CHECK(s_updateCount == 0U);
    CHECK(BallHold_GetState() ==
          BALL_HOLD_STATE_READY);
    CHECK(BallHold_IsActive() == 0U);
}

int main(void)
{
    test_start_targets_o_point();
    test_start_fails_without_vision();
    test_enters_hold_after_stable();
    test_hold_keeps_closed_loop_at_o();
    test_hold_survives_disturbance();
    test_feedforward_is_zero_while_car_is_still();
    test_hold_ignores_motion_line_acceleration();
    test_feedforward_rejects_non_finite();
    test_vision_loss_aborts();
    test_vision_loss_during_hold_aborts();
    test_converge_timeout_aborts();
    test_hold_never_times_out();
    test_stop_recenters_and_finishes();
    test_update_before_start_does_nothing();

    if (s_failures == 0)
    {
        printf("test_26h_ball4: ALL PASS\n");
        return 0;
    }
    printf("test_26h_ball4: %d FAILURE(S)\n", s_failures);
    return 1;
}
