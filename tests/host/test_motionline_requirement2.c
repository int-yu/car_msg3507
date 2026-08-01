#include "Application/Control/MotionLineRequirement2.h"
#include "Application/Control/MotionWheel.h"
#include "Hardware/Sensors/Graydetect.h"
#include "tests/host/test_assert.h"

static uint8_t s_grayState;
static MotionWheel_Command_t s_lastCommand;

uint8_t Graydetect_GetState(void) { return s_grayState; }
MotionWheel_Result_t MotionWheel_Init(void)
{
    return MOTION_WHEEL_RESULT_OK;
}
MotionWheel_Result_t MotionWheel_Update(
    const MotionWheel_Command_t *command, float dt)
{
    (void)dt;
    s_lastCommand = *command;
    return MOTION_WHEEL_RESULT_OK;
}
void MotionWheel_Stop(void)
{
    s_lastCommand.targetSpeedLMMps = 0.0f;
    s_lastCommand.targetSpeedRMMps = 0.0f;
}

static void reset_and_start(float ratio)
{
    s_grayState = 0x20U;
    MotionLineRequirement2_TuneKpMMpsPerWeight = 10.0f;
    MotionLineRequirement2_TuneKiMMpsPerWeight = 0.0f;
    MotionLineRequirement2_TuneKdMMpsPerWeight = 0.0f;
    MotionLineRequirement2_TuneAccelerationMMps2 = 300.0f;
    MotionLineRequirement2_TuneDecelerationMMps2 = 360.0f;
    MotionLineRequirement2_TuneRightTurnRightAdjustRatio = ratio;
    CHECK(MotionLineRequirement2_Init() == MOTION_LINE_RESULT_OK);
    CHECK(MotionLineRequirement2_Start(300.0f) == MOTION_LINE_RESULT_OK);
}

static void test_clockwise_right_wheel_reduction_is_scaled(void)
{
    float leftIncrease;
    float rightReduction;
    float baseSpeed = 3.0f;

    reset_and_start(2.0f);
    MotionLineRequirement2_TuneRightTurnRightAdjustRatio = 5.0f;
    MotionLineRequirement2_Update(0.01f);

    leftIncrease = s_lastCommand.targetSpeedLMMps - baseSpeed;
    rightReduction = baseSpeed - s_lastCommand.targetSpeedRMMps;
    CHECK(leftIncrease > 0.0f);
    CHECK_NEAR(rightReduction, leftIncrease * 2.0f, 0.001f);
}

static void test_line_loss_uses_twenty_times_original_delay(void)
{
    uint16_t index;

    reset_and_start(1.0f);
    s_grayState = 0U;
    for (index = 0U;
         index < (MOTION_LINE_REQUIREMENT2_LOST_CONFIRM_TICKS - 1U);
         index++)
    {
        MotionLineRequirement2_Update(0.01f);
    }
    CHECK(MotionLineRequirement2_IsBusy() != 0U);
    MotionLineRequirement2_Update(0.01f);
    CHECK(MotionLineRequirement2_IsFinished() != 0U);
    CHECK(MOTION_LINE_REQUIREMENT2_LOST_CONFIRM_TICKS == 160U);
}

int main(void)
{
    test_clockwise_right_wheel_reduction_is_scaled();
    test_line_loss_uses_twenty_times_original_delay();

    if (s_failures == 0)
    {
        printf("test_motionline_requirement2: ALL PASS\n");
        return 0;
    }
    printf("test_motionline_requirement2: %d FAILURE(S)\n", s_failures);
    return 1;
}
