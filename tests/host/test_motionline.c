#include "Application/Control/MotionLine.h"
#include "Application/Control/MotionWheel.h"
#include "Hardware/Sensors/Graydetect.h"
#include "tests/host/test_assert.h"

static uint8_t s_grayState;
static uint16_t s_wheelInitCount;
static uint16_t s_wheelStopCount;
static uint16_t s_wheelUpdateCount;
static MotionWheel_Command_t s_lastCommand;

/* ---- MotionLine 依赖的最小宿主桩 ---- */
uint8_t Graydetect_GetState(void)
{
    return s_grayState;
}

MotionWheel_Result_t MotionWheel_Init(void)
{
    s_wheelInitCount++;
    return MOTION_WHEEL_RESULT_OK;
}

MotionWheel_Result_t MotionWheel_Update(
    const MotionWheel_Command_t *command, float dt)
{
    (void)dt;
    if (command == NULL)
    {
        return MOTION_WHEEL_RESULT_INVALID_ARGUMENT;
    }
    s_lastCommand = *command;
    s_wheelUpdateCount++;
    return MOTION_WHEEL_RESULT_OK;
}

void MotionWheel_Stop(void)
{
    s_wheelStopCount++;
    s_lastCommand.targetSpeedLMMps = 0.0f;
    s_lastCommand.targetSpeedRMMps = 0.0f;
}

static void reset_fakes(void)
{
    s_grayState = 0x0CU; /* CH3 + CH4，六路几何中心。 */
    s_wheelInitCount = 0U;
    s_wheelStopCount = 0U;
    s_wheelUpdateCount = 0U;
    s_lastCommand.targetSpeedLMMps = 0.0f;
    s_lastCommand.targetSpeedRMMps = 0.0f;
    s_lastCommand.trimLPWM = 0.0f;
    s_lastCommand.trimRPWM = 0.0f;
    MotionLine_TuneMaxAdjustRatio = MOTION_LINE_MAX_ADJUST_RATIO;
    MotionLine_TuneWeightKd = 0.0f;
}

static void init_and_start(float speedMMps)
{
    CHECK(MotionLine_Init() == MOTION_LINE_RESULT_OK);
    CHECK(s_wheelInitCount == 1U);
    CHECK(MotionLine_Start(speedMMps) == MOTION_LINE_RESULT_OK);
}

static void test_start_speed_is_ramped(void)
{
    reset_fakes();
    init_and_start(450.0f);

    MotionLine_Update(0.01f);
    CHECK_NEAR(MotionLine_GetProfileSpeedMMps(),
               MOTION_LINE_ACCELERATION_MMPS2 * 0.01f, 0.001f);
    CHECK_NEAR(s_lastCommand.targetSpeedLMMps,
               MotionLine_GetProfileSpeedMMps(), 0.001f);
    CHECK_NEAR(s_lastCommand.targetSpeedRMMps,
               MotionLine_GetProfileSpeedMMps(), 0.001f);

    MotionLine_Update(0.01f);
    CHECK_NEAR(MotionLine_GetProfileSpeedMMps(),
               MOTION_LINE_ACCELERATION_MMPS2 * 0.02f, 0.001f);
}

static void test_ir_error_slows_and_turns_without_speed_step(void)
{
    float speedBefore;
    uint8_t index;

    reset_fakes();
    init_and_start(450.0f);
    for (index = 0U; index < 100U; index++)
    {
        MotionLine_Update(0.01f);
    }

    speedBefore = MotionLine_GetProfileSpeedMMps();
    s_grayState = 0x01U; /* CH1 在右侧：左轮应更快，车向右修正。 */
    MotionLine_Update(0.01f);
    CHECK(MotionLine_GetProfileSpeedMMps() <=
          speedBefore + MOTION_LINE_ACCELERATION_MMPS2 * 0.01f + 0.001f);
    CHECK(s_lastCommand.targetSpeedLMMps > s_lastCommand.targetSpeedRMMps);
    CHECK((s_lastCommand.targetSpeedLMMps -
           s_lastCommand.targetSpeedRMMps) <=
          (2.0f * MOTION_LINE_MAX_ADJUST_RATE_MMPS2 * 0.01f + 0.001f));

    for (index = 0U; index < 100U; index++)
    {
        MotionLine_Update(0.01f);
    }
    CHECK(MotionLine_GetProfileSpeedMMps() <=
          450.0f * MOTION_LINE_CURVE_MIN_SPEED_RATIO + 0.1f);
}

static void test_finish_speed_and_stop_remain_continuous(void)
{
    float speedBefore;
    uint16_t index;

    reset_fakes();
    init_and_start(450.0f);
    for (index = 0U; index < 100U; index++)
    {
        MotionLine_Update(0.01f);
    }

    speedBefore = MotionLine_GetProfileSpeedMMps();
    CHECK(MotionLine_SetSpeed(160.0f) == MOTION_LINE_RESULT_OK);
    MotionLine_Update(0.01f);
    CHECK(MotionLine_GetProfileSpeedMMps() >=
          speedBefore - MOTION_LINE_DECELERATION_MMPS2 * 0.01f - 0.001f);
    CHECK(s_wheelInitCount == 1U);

    CHECK(MotionLine_RequestStop() == MOTION_LINE_RESULT_OK);
    for (index = 0U; (index < 200U) && (MotionLine_IsBusy() != 0U); index++)
    {
        MotionLine_Update(0.01f);
    }
    CHECK(MotionLine_IsFinished() != 0U);
    CHECK(MotionLine_IsBusy() == 0U);
    CHECK(s_wheelStopCount >= 1U);
}

static void test_line_loss_is_confirmed_quickly(void)
{
    uint8_t index;

    reset_fakes();
    init_and_start(300.0f);
    s_grayState = 0U;
    for (index = 0U; index < (MOTION_LINE_LOST_CONFIRM_TICKS - 1U); index++)
    {
        MotionLine_Update(0.01f);
    }
    CHECK(MotionLine_IsBusy() != 0U);

    MotionLine_Update(0.01f);
    CHECK(MotionLine_IsFinished() != 0U);
    CHECK(s_wheelStopCount >= 1U);
}

int main(void)
{
    test_start_speed_is_ramped();
    test_ir_error_slows_and_turns_without_speed_step();
    test_finish_speed_and_stop_remain_continuous();
    test_line_loss_is_confirmed_quickly();

    if (s_failures == 0)
    {
        printf("test_motionline: ALL PASS\n");
        return 0;
    }
    printf("test_motionline: %d FAILURE(S)\n", s_failures);
    return 1;
}
