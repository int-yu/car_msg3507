#include "Application/Control/BallBalance.h"
#include "Application/Control/BallSensor.h"
#include "Application/Control/BeamActuator.h"
#include "tests/host/test_assert.h"
#include <math.h>

static uint8_t s_fresh;
static float s_positionMM;
static float s_speedMMps;
static float s_lastTiltDeg;
static uint16_t s_tiltCommandCount;

uint8_t BallSensor_IsFresh(void) { return s_fresh; }
float BallSensor_GetPositionMM(void) { return s_positionMM; }
float BallSensor_GetSpeedMMps(void) { return s_speedMMps; }

void BeamActuator_SetTiltDeg(float tiltDeg)
{
    s_lastTiltDeg = tiltDeg;
    s_tiltCommandCount++;
}

static void reset_fakes(void)
{
    s_fresh = 1U;
    s_positionMM = 0.0f;
    s_speedMMps = 0.0f;
    s_lastTiltDeg = 0.0f;
    s_tiltCommandCount = 0U;
    BallBalance_Init();
}

static void test_start_requires_fresh_vision(void)
{
    reset_fakes();
    s_fresh = 0U;
    CHECK(BallBalance_Start(0.0f) != BALL_BALANCE_RESULT_OK);
    CHECK(BallBalance_GetState() == BALL_BALANCE_STATE_IDLE);

    s_fresh = 1U;
    CHECK(BallBalance_Start(0.0f) == BALL_BALANCE_RESULT_OK);
    CHECK(BallBalance_GetState() == BALL_BALANCE_STATE_RUNNING);
}

static void test_start_rejects_invalid_target(void)
{
    reset_fakes();
    CHECK(BallBalance_Start(BALL_BALANCE_TARGET_LIMIT_MM + 1.0f) !=
          BALL_BALANCE_RESULT_OK);
    CHECK(BallBalance_GetState() == BALL_BALANCE_STATE_IDLE);
}

static void test_start_sets_requested_hold_position(void)
{
    reset_fakes();
    CHECK(BallBalance_Start(35.0f) == BALL_BALANCE_RESULT_OK);
    CHECK_NEAR(BallBalance_GetTargetMM(), 35.0f, 0.001f);
    CHECK_NEAR(BallBalance_GetProfilePositionMM(), 35.0f, 0.001f);
}

static void test_control_pushes_ball_toward_target(void)
{
    reset_fakes();
    CHECK(BallBalance_Start(0.0f) == BALL_BALANCE_RESULT_OK);

    s_positionMM = -20.0f;
    s_speedMMps = 0.0f;
    BallBalance_Update(0.01f);
    CHECK(BallBalance_GetTiltCommandDeg() > 0.0f);

    s_positionMM = 20.0f;
    BallBalance_Update(0.01f);
    CHECK(BallBalance_GetTiltCommandDeg() < 0.0f);
}

static void test_derivative_damps_motion(void)
{
    float noSpeedTilt;

    reset_fakes();
    CHECK(BallBalance_Start(0.0f) == BALL_BALANCE_RESULT_OK);

    s_positionMM = -10.0f;
    s_speedMMps = 0.0f;
    BallBalance_Update(0.01f);
    noSpeedTilt = BallBalance_GetTiltCommandDeg();

    s_speedMMps = 80.0f;
    BallBalance_Update(0.01f);
    CHECK(BallBalance_GetTiltCommandDeg() < noSpeedTilt);
}

static void test_tilt_command_is_clamped(void)
{
    reset_fakes();
    CHECK(BallBalance_Start(120.0f) == BALL_BALANCE_RESULT_OK);

    s_positionMM = -120.0f;
    BallBalance_Update(0.01f);
    CHECK(BallBalance_GetTiltCommandDeg() <=
          BALL_BALANCE_MAX_TILT_DEG + 0.001f);
    CHECK(BallBalance_GetTiltCommandDeg() >=
          -BALL_BALANCE_MAX_TILT_DEG - 0.001f);
}

static void test_integral_is_limited_and_reset_on_retarget(void)
{
    uint16_t tick;

    reset_fakes();
    BallBalance_TuneKi = 0.1f;
    CHECK(BallBalance_Start(100.0f) == BALL_BALANCE_RESULT_OK);
    for (tick = 0U; tick < 300U; tick++)
    {
        BallBalance_Update(0.01f);
    }
    CHECK(BallBalance_GetIntegralMMs() <=
          BALL_BALANCE_INTEGRAL_LIMIT_MM_S + 0.001f);

    CHECK(BallBalance_SetTarget(0.0f) == BALL_BALANCE_RESULT_OK);
    CHECK_NEAR(BallBalance_GetIntegralMMs(), 0.0f, 0.001f);
}

static void test_stable_requires_sustained_tolerance(void)
{
    uint16_t tick;

    reset_fakes();
    CHECK(BallBalance_Start(0.0f) == BALL_BALANCE_RESULT_OK);
    s_positionMM = 0.0f;

    for (tick = 0U; tick < BALL_BALANCE_SETTLE_CONFIRM_TICKS - 1U; tick++)
    {
        BallBalance_Update(0.01f);
    }
    CHECK(BallBalance_IsStable() == 0U);

    BallBalance_Update(0.01f);
    CHECK(BallBalance_IsStable() != 0U);

    s_positionMM = 20.0f;
    BallBalance_Update(0.01f);
    CHECK(BallBalance_IsStable() == 0U);
}

static void test_vision_loss_recenters_and_errors(void)
{
    uint16_t tick;

    reset_fakes();
    CHECK(BallBalance_Start(0.0f) == BALL_BALANCE_RESULT_OK);
    s_positionMM = 20.0f;
    BallBalance_Update(0.01f);

    s_fresh = 0U;
    for (tick = 0U; tick < BALL_BALANCE_VISION_LOST_TICKS; tick++)
    {
        BallBalance_Update(0.01f);
    }

    CHECK(BallBalance_GetState() == BALL_BALANCE_STATE_ERROR);
    CHECK(BallBalance_GetError() == BALL_BALANCE_ERROR_VISION_LOST);
    CHECK_NEAR(s_lastTiltDeg, 0.0f, 0.001f);
}

static void test_brief_vision_gap_is_tolerated(void)
{
    reset_fakes();
    CHECK(BallBalance_Start(0.0f) == BALL_BALANCE_RESULT_OK);

    s_fresh = 0U;
    BallBalance_Update(0.01f);
    BallBalance_Update(0.01f);
    CHECK(BallBalance_GetState() == BALL_BALANCE_STATE_RUNNING);

    s_fresh = 1U;
    BallBalance_Update(0.01f);
    CHECK(BallBalance_GetState() == BALL_BALANCE_STATE_RUNNING);
    CHECK(BallBalance_GetError() == BALL_BALANCE_ERROR_NONE);
}

static void test_stop_returns_to_idle_and_centers(void)
{
    reset_fakes();
    CHECK(BallBalance_Start(0.0f) == BALL_BALANCE_RESULT_OK);
    s_positionMM = 20.0f;
    BallBalance_Update(0.01f);

    BallBalance_Stop();
    CHECK(BallBalance_GetState() == BALL_BALANCE_STATE_IDLE);
    CHECK_NEAR(s_lastTiltDeg, 0.0f, 0.001f);
}

static void test_set_target_requires_running(void)
{
    reset_fakes();
    CHECK(BallBalance_SetTarget(50.0f) ==
          BALL_BALANCE_RESULT_NOT_RUNNING);
}

int main(void)
{
    test_start_requires_fresh_vision();
    test_start_rejects_invalid_target();
    test_start_sets_requested_hold_position();
    test_control_pushes_ball_toward_target();
    test_derivative_damps_motion();
    test_tilt_command_is_clamped();
    test_integral_is_limited_and_reset_on_retarget();
    test_stable_requires_sustained_tolerance();
    test_vision_loss_recenters_and_errors();
    test_brief_vision_gap_is_tolerated();
    test_stop_returns_to_idle_and_centers();
    test_set_target_requires_running();

    if (s_failures == 0)
    {
        printf("test_ballbalance: ALL PASS\n");
        return 0;
    }
    printf("test_ballbalance: %d FAILURE(S)\n", s_failures);
    return 1;
}
