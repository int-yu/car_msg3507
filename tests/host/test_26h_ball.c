#include "Application/Control/BallSequence.h"
#include "Application/Control/BallBalance.h"
#include "Application/Control/TaskTimer.h"
#include "tests/host/test_assert.h"

static BallBalance_State_t s_balanceState;
static BallBalance_Error_t s_balanceError;
static BallBalance_Result_t s_startResult;
static BallBalance_Result_t s_setTargetResult;
static float s_targetMM;
static uint16_t s_startCount;
static uint16_t s_setTargetCount;
static uint16_t s_stopCount;
static uint16_t s_updateCount;
static float s_positionMM;
static uint8_t s_stable;

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
    s_setTargetCount++;
    if (s_setTargetResult == BALL_BALANCE_RESULT_OK)
    {
        s_targetMM = targetMM;
    }
    return s_setTargetResult;
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
float BallBalance_GetPositionMM(void) { return s_positionMM; }
uint8_t BallBalance_IsStable(void) { return s_stable; }

static void reset_fakes(void)
{
    TaskTimer_Init();
    s_balanceState = BALL_BALANCE_STATE_IDLE;
    s_balanceError = BALL_BALANCE_ERROR_NONE;
    s_startResult = BALL_BALANCE_RESULT_OK;
    s_setTargetResult = BALL_BALANCE_RESULT_OK;
    s_targetMM = 0.0f;
    s_startCount = 0U;
    s_setTargetCount = 0U;
    s_stopCount = 0U;
    s_updateCount = 0U;
    s_positionMM = 0.0f;
    s_stable = 0U;
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

static void test_start_passes_requested_target(void)
{
    reset_fakes();
    CHECK(BallSequence_Start(25.0f) != 0U);

    CHECK(s_startCount == 1U);
    CHECK(BallSequence_GetState() == BALL_SEQUENCE_STATE_HOLDING);
    CHECK(BallSequence_IsActive() != 0U);
    CHECK_NEAR(s_targetMM, 25.0f, 0.001f);
    CHECK_NEAR(BallSequence_GetTargetMM(), 25.0f, 0.001f);
}

static void test_start_fails_without_vision_or_invalid_target(void)
{
    reset_fakes();
    s_startResult = BALL_BALANCE_RESULT_INVALID_ARGUMENT;

    CHECK(BallSequence_Start(0.0f) == 0U);
    CHECK(BallSequence_GetState() == BALL_SEQUENCE_STATE_ERROR);
    CHECK(BallSequence_GetError() == BALL_SEQUENCE_ERROR_VISION);
}

static void test_holds_closed_loop_until_stopped(void)
{
    uint16_t updatesBefore;

    reset_fakes();
    CHECK(BallSequence_Start(-30.0f) != 0U);
    updatesBefore = s_updateCount;

    run_ticks(700U);

    CHECK(BallSequence_GetState() == BALL_SEQUENCE_STATE_HOLDING);
    CHECK(BallSequence_IsActive() != 0U);
    CHECK(s_updateCount > updatesBefore);
    CHECK(s_stopCount == 0U);
    CHECK_NEAR(s_targetMM, -30.0f, 0.001f);
}

static void test_hold_reports_existing_balance_stability(void)
{
    reset_fakes();
    CHECK(BallSequence_Start(35.0f) != 0U);
    CHECK(BallSequence_IsStable() == 0U);

    s_stable = 1U;
    CHECK(BallSequence_IsStable() != 0U);

    CHECK(BallSequence_StartSweep() != 0U);
    CHECK(BallSequence_IsStable() == 0U);
}

static void test_active_hold_retargets_without_restart(void)
{
    reset_fakes();
    CHECK(BallSequence_Start(10.0f) != 0U);

    CHECK(BallSequence_SetTarget(-20.0f) != 0U);

    CHECK(s_startCount == 1U);
    CHECK(s_setTargetCount == 1U);
    CHECK(BallSequence_GetState() == BALL_SEQUENCE_STATE_HOLDING);
    CHECK_NEAR(s_targetMM, -20.0f, 0.001f);
    CHECK_NEAR(BallSequence_GetTargetMM(), -20.0f, 0.001f);
}

static void test_sweep_reverses_without_waiting_for_stability(void)
{
    reset_fakes();
    CHECK(BallSequence_StartSweep() != 0U);
    TaskTimer_Start(TASK_TIMER_OWNER_BALL);
    CHECK(BallSequence_GetState() ==
          BALL_SEQUENCE_STATE_SWEEP_TO_NEGATIVE);
    CHECK_NEAR(s_targetMM, -50.0f, 0.001f);

    s_positionMM = -44.0f;
    BallSequence_Update(0.01f);
    CHECK(s_setTargetCount == 0U);

    s_positionMM = -45.0f;
    s_stable = 0U;
    BallSequence_Update(0.01f);
    CHECK(s_setTargetCount == 1U);
    CHECK_NEAR(s_targetMM, 50.0f, 0.001f);
    CHECK(BallSequence_GetState() ==
          BALL_SEQUENCE_STATE_SWEEP_TO_POSITIVE);

    BallSequence_Update(0.01f);
    CHECK(BallSequence_GetState() ==
          BALL_SEQUENCE_STATE_SWEEP_TO_POSITIVE);
    s_stable = 1U;
    BallSequence_Update(0.01f);
    CHECK(BallSequence_GetState() ==
          BALL_SEQUENCE_STATE_SWEEP_HOLDING_POSITIVE);
    CHECK(BallSequence_IsActive() != 0U);
    CHECK(s_stopCount == 0U);
    CHECK(TaskTimer_IsRunning() == 0U);
}

static void test_retarget_cancels_active_sweep(void)
{
    reset_fakes();
    CHECK(BallSequence_StartSweep() != 0U);
    TaskTimer_Start(TASK_TIMER_OWNER_BALL);
    CHECK(BallSequence_SetTarget(0.0f) != 0U);

    CHECK(BallSequence_GetState() == BALL_SEQUENCE_STATE_HOLDING);
    CHECK_NEAR(s_targetMM, 0.0f, 0.001f);
    CHECK(TaskTimer_IsRunning() == 0U);
}

static void test_idle_hold_rejects_retarget(void)
{
    reset_fakes();

    CHECK(BallSequence_SetTarget(15.0f) == 0U);

    CHECK(s_setTargetCount == 0U);
    CHECK(BallSequence_GetState() == BALL_SEQUENCE_STATE_READY);
}

static void test_balance_error_aborts(void)
{
    reset_fakes();
    CHECK(BallSequence_Start(0.0f) != 0U);
    run_ticks(10U);

    s_balanceState = BALL_BALANCE_STATE_ERROR;
    s_balanceError = BALL_BALANCE_ERROR_VISION_LOST;
    BallSequence_Update(0.01f);

    CHECK(BallSequence_GetState() == BALL_SEQUENCE_STATE_ERROR);
    CHECK(BallSequence_GetError() == BALL_SEQUENCE_ERROR_VISION);
    CHECK(s_stopCount == 1U);
}

static void test_elapsed_ticks_track_holding_time(void)
{
    reset_fakes();
    CHECK(BallSequence_Start(0.0f) != 0U);
    CHECK(BallSequence_GetElapsedTicks() == 0U);

    run_ticks(120U);
    CHECK(BallSequence_GetElapsedTicks() == 120U);
}

static void test_stop_finishes_and_recenters(void)
{
    reset_fakes();
    CHECK(BallSequence_Start(0.0f) != 0U);

    BallSequence_Stop();

    CHECK(BallSequence_GetState() == BALL_SEQUENCE_STATE_FINISHED);
    CHECK(BallSequence_IsActive() == 0U);
    CHECK(s_stopCount == 1U);
}

static void test_update_before_start_does_nothing(void)
{
    reset_fakes();
    run_ticks(10U);

    CHECK(s_updateCount == 0U);
    CHECK(BallSequence_GetState() == BALL_SEQUENCE_STATE_READY);
}

int main(void)
{
    test_start_passes_requested_target();
    test_start_fails_without_vision_or_invalid_target();
    test_holds_closed_loop_until_stopped();
    test_hold_reports_existing_balance_stability();
    test_active_hold_retargets_without_restart();
    test_sweep_reverses_without_waiting_for_stability();
    test_retarget_cancels_active_sweep();
    test_idle_hold_rejects_retarget();
    test_balance_error_aborts();
    test_elapsed_ticks_track_holding_time();
    test_stop_finishes_and_recenters();
    test_update_before_start_does_nothing();

    if (s_failures == 0)
    {
        printf("test_26h_ball: ALL PASS\n");
        return 0;
    }
    printf("test_26h_ball: %d FAILURE(S)\n", s_failures);
    return 1;
}
