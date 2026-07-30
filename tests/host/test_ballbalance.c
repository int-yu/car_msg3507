#include "Application/Control/BallBalance.h"
#include "Application/Control/BallSensor.h"
#include "Application/Control/BeamActuator.h"
#include "tests/host/test_assert.h"
#include <math.h>

/* ---- BallBalance 只依赖 BallSensor 的观测和 BeamActuator 的输出 ---- */
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

/*
 * 一个粗糙的钢球被控对象：x'' = k * theta。只用来验证闭环方向和收敛
 * 趋势，不追求物理精度——真正的 k 要实车标定。
 */
static void simulate(uint16_t ticks)
{
    uint16_t tick;
    const float dt = 0.01f;

    for (tick = 0U; tick < ticks; tick++)
    {
        float accelerationMMps2 =
            BALL_BALANCE_GRAVITY_COUPLING_MMPS2_PER_DEG * s_lastTiltDeg;

        BallBalance_Update(dt);
        s_speedMMps += accelerationMMps2 * dt;
        s_positionMM += s_speedMMps * dt;
    }
}

static void test_start_requires_fresh_vision(void)
{
    reset_fakes();
    s_fresh = 0U;
    CHECK(BallBalance_Start() != BALL_BALANCE_RESULT_OK);
    CHECK(BallBalance_GetState() == BALL_BALANCE_STATE_IDLE);

    s_fresh = 1U;
    CHECK(BallBalance_Start() == BALL_BALANCE_RESULT_OK);
    CHECK(BallBalance_GetState() == BALL_BALANCE_STATE_RUNNING);
}

/* 起步要从钢球当前位置开始，否则第一拍就是一个大阶跃。 */
static void test_start_anchors_profile_to_current_ball(void)
{
    reset_fakes();
    s_positionMM = 30.0f;
    CHECK(BallBalance_Start() == BALL_BALANCE_RESULT_OK);

    CHECK_NEAR(BallBalance_GetProfilePositionMM(), 30.0f, 0.001f);
    CHECK_NEAR(BallBalance_GetTargetMM(), 30.0f, 0.001f);
}

/*
 * 轨迹发生器的存在意义：目标阶跃 100 mm 时，PD 若直接跟阶跃会命令
 * 100*0.24 = 24 度，远超摆杆行程，球会被打到挡片。
 */
static void test_target_step_does_not_saturate_tilt(void)
{
    reset_fakes();
    CHECK(BallBalance_Start() == BALL_BALANCE_RESULT_OK);
    CHECK(BallBalance_SetTarget(50.0f) == BALL_BALANCE_RESULT_OK);

    BallBalance_Update(0.01f);
    /* 第一拍参考位置只移动了几分之一毫米，倾角必须远小于限幅值。 */
    CHECK(fabsf(BallBalance_GetTiltCommandDeg()) <
          BALL_BALANCE_MAX_TILT_DEG);
    CHECK(fabsf(BallBalance_GetTiltCommandDeg()) < 1.0f);
}

static void test_tilt_command_is_clamped(void)
{
    reset_fakes();
    CHECK(BallBalance_Start() == BALL_BALANCE_RESULT_OK);

    /* 人为制造一个极大偏差，倾角必须被限幅在机械行程内。 */
    s_positionMM = -120.0f;
    BallBalance_Update(0.01f);
    CHECK(BallBalance_GetTiltCommandDeg() <=
          BALL_BALANCE_MAX_TILT_DEG + 0.001f);
    CHECK(BallBalance_GetTiltCommandDeg() >=
          -BALL_BALANCE_MAX_TILT_DEG - 0.001f);
}

/* 闭环方向必须正确：球偏负侧时倾角应为正，把球推回来。 */
static void test_control_pushes_ball_toward_target(void)
{
    reset_fakes();
    CHECK(BallBalance_Start() == BALL_BALANCE_RESULT_OK);

    s_positionMM = -20.0f;
    s_speedMMps = 0.0f;
    BallBalance_Update(0.01f);
    CHECK(BallBalance_GetTiltCommandDeg() > 0.0f);

    s_positionMM = 20.0f;
    BallBalance_Update(0.01f);
    CHECK(BallBalance_GetTiltCommandDeg() < 0.0f);
}

/* 闭环应当把球带到 +50 mm 附近并稳定，这是题目的 +5 cm 点。 */
static void test_closed_loop_converges_to_target(void)
{
    reset_fakes();
    CHECK(BallBalance_Start() == BALL_BALANCE_RESULT_OK);
    CHECK(BallBalance_SetTarget(50.0f) == BALL_BALANCE_RESULT_OK);

    simulate(400U); /* 4 秒。 */

    CHECK(fabsf(s_positionMM - 50.0f) <= 10.0f);
    CHECK(BallBalance_IsStable() != 0U);
}

/* 视觉失效必须回中并报错，不能拿冻结位置继续闭环。 */
static void test_vision_loss_recenters_and_errors(void)
{
    uint16_t tick;

    reset_fakes();
    CHECK(BallBalance_Start() == BALL_BALANCE_RESULT_OK);
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

/* 短暂丢帧不应立刻判错：相机偶尔丢一两帧是正常的。 */
static void test_brief_vision_gap_is_tolerated(void)
{
    reset_fakes();
    CHECK(BallBalance_Start() == BALL_BALANCE_RESULT_OK);

    s_fresh = 0U;
    BallBalance_Update(0.01f);
    BallBalance_Update(0.01f);
    CHECK(BallBalance_GetState() == BALL_BALANCE_STATE_RUNNING);

    s_fresh = 1U;
    BallBalance_Update(0.01f);
    CHECK(BallBalance_GetState() == BALL_BALANCE_STATE_RUNNING);
    CHECK(BallBalance_GetError() == BALL_BALANCE_ERROR_NONE);
}

/* 车体加速度前馈：静止时为零，加速时应产生抵消倾角。 */
static void test_car_acceleration_feedforward(void)
{
    float tiltWithoutFeedforward;
    float tiltWithFeedforward;

    reset_fakes();
    CHECK(BallBalance_Start() == BALL_BALANCE_RESULT_OK);
    BallBalance_SetCarAcceleration(0.0f);
    BallBalance_Update(0.01f);
    tiltWithoutFeedforward = BallBalance_GetTiltCommandDeg();

    /* 300 mm/s^2 需要 300 / 102.8 约 2.9 度补偿。 */
    BallBalance_SetCarAcceleration(300.0f);
    BallBalance_Update(0.01f);
    tiltWithFeedforward = BallBalance_GetTiltCommandDeg();

    CHECK((tiltWithFeedforward - tiltWithoutFeedforward) > 2.0f);
    CHECK((tiltWithFeedforward - tiltWithoutFeedforward) < 4.0f);
}

/* 到位判据要求持续满足，不能一擦边就宣布稳定。 */
static void test_stable_requires_sustained_tolerance(void)
{
    reset_fakes();
    CHECK(BallBalance_Start() == BALL_BALANCE_RESULT_OK);
    CHECK(BallBalance_SetTarget(0.0f) == BALL_BALANCE_RESULT_OK);

    s_positionMM = 0.0f;
    BallBalance_Update(0.01f);
    CHECK(BallBalance_IsStable() == 0U);

    /* 中途跑出容差要清零计数。 */
    simulate(5U);
    s_positionMM = 40.0f;
    BallBalance_Update(0.01f);
    CHECK(BallBalance_IsStable() == 0U);
}

static void test_stop_returns_to_idle_and_centers(void)
{
    reset_fakes();
    CHECK(BallBalance_Start() == BALL_BALANCE_RESULT_OK);
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
    test_start_anchors_profile_to_current_ball();
    test_target_step_does_not_saturate_tilt();
    test_tilt_command_is_clamped();
    test_control_pushes_ball_toward_target();
    test_closed_loop_converges_to_target();
    test_vision_loss_recenters_and_errors();
    test_brief_vision_gap_is_tolerated();
    test_car_acceleration_feedforward();
    test_stable_requires_sustained_tolerance();
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
