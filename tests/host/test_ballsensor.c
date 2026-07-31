#include "Application/Control/BallSensor.h"
#include "Application/Comms/K230Link.h"
#include "tests/host/test_assert.h"

/* ---- BallSensor only depends on K230Link_GetBallPosition(). ---- */
static uint8_t s_ballAvailable;
static K230Link_BallPosition_t s_ball;

uint8_t K230Link_GetBallPosition(K230Link_BallPosition_t *position)
{
    if ((position == NULL) || (s_ballAvailable == 0U))
    {
        return 0U;
    }
    *position = s_ball;
    return 1U;
}

static void reset_fakes(void)
{
    s_ballAvailable = 0U;
    s_ball.valid = 0U;
    s_ball.positionX100 = K230_LINK_BALL_POSITION_INVALID;
    s_ball.speedValid = 0U;
    s_ball.speedX100 = K230_LINK_BALL_SPEED_INVALID;
    s_ball.sequence = 0U;
    s_ball.ageTicks = 0U;
    BallSensor_Init();
}

static void feed_frame(int16_t positionX100)
{
    s_ballAvailable = 1U;
    s_ball.valid = 1U;
    s_ball.positionX100 = positionX100;
    s_ball.speedValid = 0U;
    s_ball.speedX100 = K230_LINK_BALL_SPEED_INVALID;
    s_ball.sequence++;
}

static void feed_frame_with_speed(int16_t positionX100, int16_t speedX100)
{
    feed_frame(positionX100);
    s_ball.speedValid = 1U;
    s_ball.speedX100 = speedX100;
}

/* 相机约 25 fps、控制环 100 Hz：一帧被读 4 拍。 */
static void run_frame_ticks(void)
{
    uint8_t tick;

    for (tick = 0U; tick < 4U; tick++)
    {
        BallSensor_Update(0.01f);
    }
}

static void test_pipe_coordinate_maps_to_millimetres(void)
{
    reset_fakes();
    /* +20.00 on a -50..+50 scale across 250 mm equals +50 mm. */
    feed_frame(2000);
    BallSensor_Update(0.01f);

    CHECK(BallSensor_IsFresh() != 0U);
    CHECK_NEAR(BallSensor_GetPositionMM(), 50.0f, 0.001f);
}

static void test_negative_position_is_signed(void)
{
    reset_fakes();
    feed_frame(-2000);
    BallSensor_Update(0.01f);

    CHECK(BallSensor_IsFresh() != 0U);
    CHECK_NEAR(BallSensor_GetPositionMM(), -50.0f, 0.001f);
}

/* 视觉失效必须让 IsFresh() 变假。钢球是双积分对象，拿冻结的位置算 PD
 * 会让倾角锁死、球一路加速滚到挡片。 */
static void test_lost_target_is_not_fresh(void)
{
    reset_fakes();
    feed_frame(200);
    BallSensor_Update(0.01f);
    CHECK(BallSensor_IsFresh() != 0U);

    s_ballAvailable = 0U;
    BallSensor_Update(0.01f);
    CHECK(BallSensor_IsFresh() == 0U);
    CHECK_NEAR(BallSensor_GetSpeedMMps(), 0.0f, 0.001f);
}

static void test_invalid_flag_is_not_fresh(void)
{
    reset_fakes();
    feed_frame(200);
    BallSensor_Update(0.01f);
    CHECK(BallSensor_IsFresh() != 0U);

    s_ball.valid = 0U;
    BallSensor_Update(0.01f);
    CHECK(BallSensor_IsFresh() == 0U);
}

/* 钢球跑不出摆杆；超出范围就是误检或认错目标，宁可报无效。 */
static void test_out_of_range_is_rejected(void)
{
    reset_fakes();
    feed_frame(6000); /* 150 mm, outside the 125 mm pipe half length. */
    BallSensor_Update(0.01f);

    CHECK(BallSensor_IsFresh() == 0U);
}

/*
 * 速度必须按帧间实际时间算。相机 25 fps、控制环 100 Hz，若错误地除以
 * 控制拍 dt(10 ms) 而不是帧间隔(40 ms)，速度会被高估约 4 倍。
 */
static void test_speed_uses_frame_interval_not_tick(void)
{
    uint8_t frame;

    reset_fakes();
    /* 从 -100 mm 起步，每帧 +10 mm，20 帧走到 +100 mm；全程留在摆杆内，
     * 否则会被范围检查判无效而测不到速度。帧间隔 40 ms，真值 250 mm/s。 */
    feed_frame(-4000);
    run_frame_ticks();
    for (frame = 1U; frame <= 20U; frame++)
    {
        feed_frame((int16_t)(-4000 + (int16_t)(frame * 400)));
        run_frame_ticks();
    }

    /* 低通有稳态收敛，取一个能区分 250 与 1000 的宽容差。 */
    CHECK(BallSensor_GetSpeedMMps() > 200.0f);
    CHECK(BallSensor_GetSpeedMMps() < 300.0f);
}

/* 同一帧被重复读时不能做差分，否则估计值被一路拉向零、废掉阻尼项。 */
static void test_repeated_frame_does_not_drag_speed_to_zero(void)
{
    uint8_t frame;
    uint8_t tick;
    float speedAfterMotion;

    reset_fakes();
    feed_frame(-4000);
    run_frame_ticks();
    for (frame = 1U; frame <= 20U; frame++)
    {
        feed_frame((int16_t)(-4000 + (int16_t)(frame * 400)));
        run_frame_ticks();
    }
    speedAfterMotion = BallSensor_GetSpeedMMps();
    CHECK(speedAfterMotion > 200.0f);

    /* 序号不变地再读 8 拍，速度不应被重复帧稀释。 */
    for (tick = 0U; tick < 8U; tick++)
    {
        BallSensor_Update(0.01f);
    }
    CHECK_NEAR(BallSensor_GetSpeedMMps(), speedAfterMotion, 0.001f);
}

static void test_stationary_ball_has_zero_speed(void)
{
    uint8_t frame;

    reset_fakes();
    for (frame = 0U; frame < 20U; frame++)
    {
        feed_frame(800); /* Fixed 20 mm. */
        run_frame_ticks();
    }

    CHECK_NEAR(BallSensor_GetPositionMM(), 20.0f, 0.001f);
    CHECK_NEAR(BallSensor_GetSpeedMMps(), 0.0f, 0.001f);
}

static void test_k230_speed_is_preferred(void)
{
    reset_fakes();
    feed_frame(-2000);
    run_frame_ticks();
    feed_frame_with_speed(2000, -4000);
    BallSensor_Update(0.01f);

    CHECK_NEAR(BallSensor_GetSpeedMMps(), -100.0f, 0.001f);
    CHECK(BallSensor_GetSpeedSource() == BALL_SENSOR_SPEED_SOURCE_K230);
}

static void test_k230_speed_is_used_on_sequence_zero_first_frame(void)
{
    reset_fakes();
    s_ballAvailable = 1U;
    s_ball.valid = 1U;
    s_ball.positionX100 = 0;
    s_ball.speedValid = 1U;
    s_ball.speedX100 = 2000;
    s_ball.sequence = 0U;
    BallSensor_Update(0.01f);

    CHECK_NEAR(BallSensor_GetSpeedMMps(), 50.0f, 0.001f);
    CHECK(BallSensor_GetSpeedSource() == BALL_SENSOR_SPEED_SOURCE_K230);
}

static void test_missing_k230_speed_falls_back_to_ti_difference(void)
{
    uint8_t frame;

    reset_fakes();
    feed_frame(-4000);
    run_frame_ticks();
    for (frame = 1U; frame <= 20U; frame++)
    {
        feed_frame((int16_t)(-4000 + (int16_t)(frame * 400)));
        run_frame_ticks();
    }

    CHECK(BallSensor_GetSpeedSource() == BALL_SENSOR_SPEED_SOURCE_TI);
    CHECK(BallSensor_GetSpeedMMps() > 200.0f);
    CHECK(BallSensor_GetSpeedMMps() < 300.0f);
}

int main(void)
{
    test_pipe_coordinate_maps_to_millimetres();
    test_negative_position_is_signed();
    test_lost_target_is_not_fresh();
    test_invalid_flag_is_not_fresh();
    test_out_of_range_is_rejected();
    test_speed_uses_frame_interval_not_tick();
    test_repeated_frame_does_not_drag_speed_to_zero();
    test_stationary_ball_has_zero_speed();
    test_k230_speed_is_preferred();
    test_k230_speed_is_used_on_sequence_zero_first_frame();
    test_missing_k230_speed_falls_back_to_ti_difference();

    if (s_failures == 0)
    {
        printf("test_ballsensor: ALL PASS\n");
        return 0;
    }
    printf("test_ballsensor: %d FAILURE(S)\n", s_failures);
    return 1;
}
