#include "Application/Control/BallTargetCapture.h"
#include "Application/Control/BallSensor.h"
#include "tests/host/test_assert.h"

static uint8_t s_fresh;
static uint8_t s_sequence;
static float s_positionMM;

uint8_t BallSensor_IsFresh(void) { return s_fresh; }
uint8_t BallSensor_GetFrameSequence(void) { return s_sequence; }
float BallSensor_GetPositionMM(void) { return s_positionMM; }

static void reset_fakes(void)
{
    s_fresh = 0U;
    s_sequence = 0U;
    s_positionMM = 0.0f;
    BallTargetCapture_Init();
}

static void feed_frame(float positionMM)
{
    s_fresh = 1U;
    s_sequence++;
    s_positionMM = positionMM;
    BallTargetCapture_Update();
}

static void test_requires_unique_stable_frames(void)
{
    uint8_t frame;

    reset_fakes();
    BallTargetCapture_Start();
    feed_frame(30.0f);
    for (frame = 0U; frame < 20U; frame++)
    {
        BallTargetCapture_Update();
    }

    CHECK(BallTargetCapture_IsCapturing() != 0U);
    CHECK(BallTargetCapture_GetConfirmedFrames() == 1U);

    for (frame = 1U; frame < BALL_TARGET_CAPTURE_CONFIRM_FRAMES; frame++)
    {
        feed_frame(30.0f);
    }
    CHECK(BallTargetCapture_IsCaptured() != 0U);
    CHECK_NEAR(BallTargetCapture_GetTargetMM(), 30.0f, 0.001f);
}

static void test_averages_small_visual_jitter(void)
{
    uint8_t frame;

    reset_fakes();
    BallTargetCapture_Start();
    for (frame = 0U; frame < BALL_TARGET_CAPTURE_CONFIRM_FRAMES; frame++)
    {
        feed_frame((frame & 1U) ? 42.0f : 38.0f);
    }

    CHECK(BallTargetCapture_IsCaptured() != 0U);
    CHECK_NEAR(BallTargetCapture_GetTargetMM(), 40.0f, 0.001f);
}

static void test_large_jump_restarts_confirmation(void)
{
    uint8_t frame;

    reset_fakes();
    BallTargetCapture_Start();
    for (frame = 0U; frame < 4U; frame++)
    {
        feed_frame(20.0f);
    }
    feed_frame(40.0f);

    CHECK(BallTargetCapture_IsCapturing() != 0U);
    CHECK(BallTargetCapture_GetConfirmedFrames() == 1U);
    CHECK_NEAR(BallTargetCapture_GetTargetMM(), 0.0f, 0.001f);
}

static void test_stale_or_uncontrollable_target_does_not_lock(void)
{
    uint8_t frame;

    reset_fakes();
    BallTargetCapture_Start();
    feed_frame(20.0f);
    feed_frame(20.0f);
    CHECK(BallTargetCapture_GetConfirmedFrames() == 2U);

    s_fresh = 0U;
    BallTargetCapture_Update();
    CHECK(BallTargetCapture_GetConfirmedFrames() == 0U);

    for (frame = 0U; frame < BALL_TARGET_CAPTURE_CONFIRM_FRAMES; frame++)
    {
        feed_frame(125.0f);
    }
    CHECK(BallTargetCapture_IsCaptured() == 0U);
    CHECK(BallTargetCapture_GetConfirmedFrames() == 0U);
}

static void test_cancel_returns_to_idle(void)
{
    reset_fakes();
    BallTargetCapture_Start();
    feed_frame(-25.0f);
    BallTargetCapture_Cancel();

    CHECK(BallTargetCapture_GetState() ==
          BALL_TARGET_CAPTURE_STATE_IDLE);
    CHECK(BallTargetCapture_GetConfirmedFrames() == 0U);
}

int main(void)
{
    test_requires_unique_stable_frames();
    test_averages_small_visual_jitter();
    test_large_jump_restarts_confirmation();
    test_stale_or_uncontrollable_target_does_not_lock();
    test_cancel_returns_to_idle();

    if (s_failures == 0)
    {
        printf("test_balltargetcapture: ALL PASS\n");
        return 0;
    }
    printf("test_balltargetcapture: %d FAILURE(S)\n", s_failures);
    return 1;
}
