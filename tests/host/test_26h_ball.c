#include "Application/Control/BallSequence.h"
#include "Application/Control/BallBalance.h"
#include "tests/host/test_assert.h"

/* ---- BallSequence 只依赖 BallBalance，这里给一个可控的桩 ---- */
static BallBalance_State_t s_balanceState;
static BallBalance_Error_t s_balanceError;
static BallBalance_Result_t s_startResult;
static uint8_t s_stable;
static float s_targetMM;
static float s_carAccelerationMMps2;
static uint16_t s_startCount;
static uint16_t s_stopCount;
static uint16_t s_updateCount;

void BallBalance_Init(void)
{
    s_balanceState = BALL_BALANCE_STATE_IDLE;
    s_balanceError = BALL_BALANCE_ERROR_NONE;
}

BallBalance_Result_t BallBalance_Start(void)
{
    s_startCount++;
    if (s_startResult == BALL_BALANCE_RESULT_OK)
    {
        s_balanceState = BALL_BALANCE_STATE_RUNNING;
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
    /* 换目标就重新开始计稳定，和真实实现一致。 */
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

static void reset_fakes(void)
{
    s_balanceState = BALL_BALANCE_STATE_IDLE;
    s_balanceError = BALL_BALANCE_ERROR_NONE;
    s_startResult = BALL_BALANCE_RESULT_OK;
    s_stable = 0U;
    s_targetMM = 0.0f;
    s_carAccelerationMMps2 = -1.0f;
    s_startCount = 0U;
    s_stopCount = 0U;
    s_updateCount = 0U;
    BallSequence_Init();
}

static void run_ticks(uint16_t ticks)
{
    uint16_t tick;

    for (tick = 0U; tick < ticks; tick++)
    {
        BallSequence_Update(0.01f);
    }
}

static void test_start_sets_plus_target(void)
{
    reset_fakes();
    CHECK(BallSequence_Start() != 0U);

    CHECK(s_startCount == 1U);
    CHECK(BallSequence_GetState() ==
          BALL_SEQUENCE_STATE_TO_PLUS);
    CHECK_NEAR(s_targetMM, BALL_SEQUENCE_TARGET_MM, 0.001f);
    /* 要求 3 是静止测试，前馈必须显式置零。 */
    CHECK_NEAR(s_carAccelerationMMps2, 0.0f, 0.001f);
}

/* 看不到钢球时不允许起步，否则第一拍就是拿无效位置做闭环。 */
static void test_start_fails_without_vision(void)
{
    reset_fakes();
    s_startResult = BALL_BALANCE_RESULT_INVALID_ARGUMENT;

    CHECK(BallSequence_Start() == 0U);
    CHECK(BallSequence_GetState() ==
          BALL_SEQUENCE_STATE_ERROR);
    CHECK(BallSequence_GetError() ==
          BALL_SEQUENCE_ERROR_VISION);
}

/* 到达 +5 cm 并稳定后才折返，不能一擦边就换目标。 */
static void test_reverses_only_after_stable_at_plus(void)
{
    reset_fakes();
    CHECK(BallSequence_Start() != 0U);

    run_ticks(50U);
    CHECK(BallSequence_GetState() ==
          BALL_SEQUENCE_STATE_TO_PLUS);
    CHECK_NEAR(s_targetMM, BALL_SEQUENCE_TARGET_MM, 0.001f);

    s_stable = 1U;
    BallSequence_Update(0.01f);
    CHECK(BallSequence_GetState() ==
          BALL_SEQUENCE_STATE_TO_MINUS);
    CHECK_NEAR(s_targetMM, -BALL_SEQUENCE_TARGET_MM, 0.001f);
}

/* 到 -5 cm 后要继续保持闭环：停控摆杆回中，球就滚走了。 */
static void test_holds_closed_loop_at_minus(void)
{
    uint16_t updatesBefore;

    reset_fakes();
    CHECK(BallSequence_Start() != 0U);
    s_stable = 1U;
    BallSequence_Update(0.01f);
    CHECK(BallSequence_GetState() ==
          BALL_SEQUENCE_STATE_TO_MINUS);

    s_stable = 1U;
    BallSequence_Update(0.01f);
    CHECK(BallSequence_GetState() ==
          BALL_SEQUENCE_STATE_HOLD_MINUS);

    updatesBefore = s_updateCount;
    run_ticks(20U);
    CHECK(s_updateCount > updatesBefore);
    CHECK(s_stopCount == 0U);
    CHECK_NEAR(s_targetMM, -BALL_SEQUENCE_TARGET_MM, 0.001f);
}

/* 中途丢视觉要停控回中并报错。 */
static void test_vision_loss_aborts(void)
{
    reset_fakes();
    CHECK(BallSequence_Start() != 0U);
    run_ticks(10U);

    s_balanceState = BALL_BALANCE_STATE_ERROR;
    s_balanceError = BALL_BALANCE_ERROR_VISION_LOST;
    BallSequence_Update(0.01f);

    CHECK(BallSequence_GetState() ==
          BALL_SEQUENCE_STATE_ERROR);
    CHECK(BallSequence_GetError() ==
          BALL_SEQUENCE_ERROR_VISION);
    CHECK(s_stopCount == 1U);
}

static void test_timeout_aborts_and_recenters(void)
{
    reset_fakes();
    CHECK(BallSequence_Start() != 0U);

    run_ticks(BALL_SEQUENCE_TIMEOUT_TICKS);

    CHECK(BallSequence_GetState() ==
          BALL_SEQUENCE_STATE_ERROR);
    CHECK(BallSequence_GetError() ==
          BALL_SEQUENCE_ERROR_TIMEOUT);
    CHECK(s_stopCount == 1U);
}

/* 正常序列必须在 5 s 要求内完成，这里核对计时口径。 */
static void test_elapsed_ticks_track_the_sequence(void)
{
    reset_fakes();
    CHECK(BallSequence_Start() != 0U);
    CHECK(BallSequence_GetElapsedTicks() == 0U);

    run_ticks(120U);
    CHECK(BallSequence_GetElapsedTicks() == 120U);

    /* 500 拍即题目的 5 s 上限，正常序列应远早于此完成。 */
    CHECK(BallSequence_GetElapsedTicks() < 500U);
}

static void test_update_before_start_does_nothing(void)
{
    reset_fakes();
    run_ticks(10U);

    CHECK(s_updateCount == 0U);
    CHECK(BallSequence_GetState() ==
          BALL_SEQUENCE_STATE_READY);
}

int main(void)
{
    test_start_sets_plus_target();
    test_start_fails_without_vision();
    test_reverses_only_after_stable_at_plus();
    test_holds_closed_loop_at_minus();
    test_vision_loss_aborts();
    test_timeout_aborts_and_recenters();
    test_elapsed_ticks_track_the_sequence();
    test_update_before_start_does_nothing();

    if (s_failures == 0)
    {
        printf("test_26h_ball: ALL PASS\n");
        return 0;
    }
    printf("test_26h_ball: %d FAILURE(S)\n", s_failures);
    return 1;
}
