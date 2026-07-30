#include "Application/Control/MotionLine.h"
#include "Application/Control/MotionWheel.h"
#include "Hardware/Sensors/Graydetect.h"
#include "tests/host/test_assert.h"

static uint8_t s_grayState;
static uint8_t s_headingReady;
static float s_yawDeg;
static uint16_t s_wheelInitCount;
static uint16_t s_wheelStopCount;
static uint16_t s_wheelUpdateCount;
static MotionWheel_Command_t s_lastCommand;

/* ---- MotionLine 依赖的最小宿主桩 ---- */
uint8_t Graydetect_GetState(void)
{
    return s_grayState;
}

uint8_t Heading_IsReady(void)
{
    return s_headingReady;
}

float Heading_GetYaw(void)
{
    return s_yawDeg;
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
    s_headingReady = 1U;
    s_yawDeg = 0.0f;
    s_wheelInitCount = 0U;
    s_wheelStopCount = 0U;
    s_wheelUpdateCount = 0U;
    s_lastCommand.targetSpeedLMMps = 0.0f;
    s_lastCommand.targetSpeedRMMps = 0.0f;
    s_lastCommand.trimLPWM = 0.0f;
    s_lastCommand.trimRPWM = 0.0f;
    MotionLine_TuneMaxAdjustRatio = MOTION_LINE_MAX_ADJUST_RATIO;
    MotionLine_TuneWeightKd = 0.0f;
    MotionLine_TuneCurveMaxAdjustRatio = 0.48f;
    MotionLine_TuneCurveWeightKd = 0.0f;
    MotionLine_TuneAccelerationMMps2 = MOTION_LINE_ACCELERATION_MMPS2;
    MotionLine_TuneDecelerationMMps2 = MOTION_LINE_DECELERATION_MMPS2;
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

/* 摆杆平衡要用它做车体加速度前馈（θ_ff = a_car / k）。必须是规划量：
 * 加速段为正、匀速段归零、减速段为负，且不含编码器差分的噪声。 */
static void test_profile_acceleration_tracks_the_ramp(void)
{
    reset_fakes();
    init_and_start(450.0f);

    MotionLine_Update(0.01f);
    CHECK_NEAR(MotionLine_GetProfileAccelerationMMps2(),
               MOTION_LINE_ACCELERATION_MMPS2, 0.001f);

    /* 斜坡到达请求速度后进入匀速，前馈必须归零，否则摆杆会一直偏着。 */
    while (MotionLine_GetProfileSpeedMMps() < 449.999f)
    {
        MotionLine_Update(0.01f);
    }
    MotionLine_Update(0.01f);
    CHECK_NEAR(MotionLine_GetProfileAccelerationMMps2(), 0.0f, 0.001f);

    /* 减速段为负，且大小按减速度常数而不是加速度常数。 */
    CHECK(MotionLine_SetSpeed(100.0f) == MOTION_LINE_RESULT_OK);
    MotionLine_Update(0.01f);
    CHECK_NEAR(MotionLine_GetProfileAccelerationMMps2(),
               -MOTION_LINE_DECELERATION_MMPS2, 0.001f);
}

static void test_profile_tunings_are_snapshotted_at_start(void)
{
    reset_fakes();
    MotionLine_TuneAccelerationMMps2 = 125.0f;
    MotionLine_TuneDecelerationMMps2 = 240.0f;
    init_and_start(300.0f);

    MotionLine_TuneAccelerationMMps2 = 900.0f;
    MotionLine_TuneDecelerationMMps2 = 950.0f;
    MotionLine_Update(0.01f);
    CHECK_NEAR(MotionLine_GetProfileAccelerationMMps2(), 125.0f, 0.001f);

    while (MotionLine_GetProfileSpeedMMps() < 299.999f)
    {
        MotionLine_Update(0.01f);
    }
    CHECK(MotionLine_SetSpeed(100.0f) == MOTION_LINE_RESULT_OK);
    MotionLine_Update(0.01f);
    CHECK_NEAR(MotionLine_GetProfileAccelerationMMps2(), -240.0f, 0.001f);

    MotionLine_Stop();
    CHECK(MotionLine_Start(300.0f) == MOTION_LINE_RESULT_OK);
    MotionLine_Update(0.01f);
    CHECK_NEAR(MotionLine_GetProfileAccelerationMMps2(), 900.0f, 0.001f);
}

static void test_ir_side_controls_the_matching_wheel(void)
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
    s_grayState = 0x01U; /* CH1 在左侧：左轮减速、右轮加速。 */
    MotionLine_Update(0.01f);
    CHECK(MotionLine_GetProfileSpeedMMps() <=
          speedBefore + MOTION_LINE_ACCELERATION_MMPS2 * 0.01f + 0.001f);
    CHECK(s_lastCommand.targetSpeedLMMps < s_lastCommand.targetSpeedRMMps);
    CHECK((s_lastCommand.targetSpeedRMMps -
           s_lastCommand.targetSpeedLMMps) <=
          (2.0f * MOTION_LINE_MAX_ADJUST_RATE_MMPS2 * 0.01f + 0.001f));

    for (index = 0U; index < 100U; index++)
    {
        MotionLine_Update(0.01f);
    }
    CHECK(MotionLine_GetProfileSpeedMMps() <=
          450.0f * MOTION_LINE_CURVE_MIN_SPEED_RATIO + 0.1f);

    reset_fakes();
    init_and_start(450.0f);
    for (index = 0U; index < 100U; index++)
    {
        MotionLine_Update(0.01f);
    }

    s_grayState = 0x20U; /* CH6 在右侧：右轮减速、左轮加速。 */
    MotionLine_Update(0.01f);
    CHECK(s_lastCommand.targetSpeedLMMps > s_lastCommand.targetSpeedRMMps);
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

static void test_curve_mode_uses_ch2_ch5_and_yaw_angle_exit(void)
{
    uint8_t index;

    reset_fakes();
    MotionLine_TuneMaxAdjustRatio = 0.10f;
    init_and_start(450.0f);

    /* CH2 连续压线只能产生弧线候选；未见转动时仍保持直线 PID。 */
    s_grayState = 0x02U;
    for (index = 0U;
         index < (MOTION_LINE_CURVE_ENTRY_CONFIRM_TICKS - 1U); index++)
    {
        MotionLine_Update(0.01f);
    }
    CHECK(MotionLine_GetPathState() == MOTION_LINE_PATH_STRAIGHT);

    /* 角速度连续越过进入阈值才真正切到弧线 PID。 */
    MotionLine_Update(0.01f);
    CHECK(MotionLine_GetPathState() == MOTION_LINE_PATH_CURVE);

    /* 在两道角速度阈值之间必须锁住弧线，不允许来回切换。 */
    s_grayState = 0x0CU;
    s_yawDeg = 0.0f;
    for (index = 0U; index < 20U; index++)
    {
        MotionLine_Update(0.01f);
    }
    CHECK(MotionLine_GetPathState() == MOTION_LINE_PATH_CURVE);

    /* 退出只由低角速度连续确认，不能依据 CH2/CH5 是否释放。 */
    s_yawDeg = 0.0f;
    for (index = 0U; index < 20U; index++)
    {
        MotionLine_Update(0.01f);
    }
    CHECK(MotionLine_GetPathState() == MOTION_LINE_PATH_CURVE);
    s_yawDeg = MOTION_LINE_CURVE_EXIT_ANGLE_DEG;
    MotionLine_Update(0.01f);
    CHECK(MotionLine_GetPathState() == MOTION_LINE_PATH_STRAIGHT);
}

static void test_curve_speed_is_limited_after_curve_confirmation(void)
{
    uint8_t index;

    reset_fakes();
    MotionLine_TuneCurveSpeedMMps = 400.0f;
    init_and_start(450.0f);
    for (index = 0U; index < 200U; index++)
    {
        MotionLine_Update(0.01f);
    }
    CHECK(MotionLine_GetProfileSpeedMMps() > 400.0f);

    s_grayState = 0x10U; /* CH5：弧线入口。 */
    for (index = 0U; index < MOTION_LINE_CURVE_ENTRY_CONFIRM_TICKS; index++)
    {
        MotionLine_Update(0.01f);
    }
    CHECK(MotionLine_GetPathState() == MOTION_LINE_PATH_CURVE);

    for (index = 0U; index < 100U; index++)
    {
        MotionLine_Update(0.01f);
    }
    CHECK_NEAR(MotionLine_GetProfileSpeedMMps(),
               MotionLine_TuneCurveSpeedMMps, 0.1f);
}

static void test_curve_exit_uses_relative_yaw_angle(void)
{
    uint8_t index;

    reset_fakes();
    init_and_start(450.0f);

    s_grayState = 0x02U;
    for (index = 0U; index < MOTION_LINE_CURVE_ENTRY_CONFIRM_TICKS; index++)
    {
        MotionLine_Update(0.01f);
    }
    CHECK(MotionLine_GetPathState() == MOTION_LINE_PATH_CURVE);

    s_yawDeg = MOTION_LINE_CURVE_EXIT_ANGLE_DEG - 0.1f;
    MotionLine_Update(0.01f);
    CHECK(MotionLine_GetPathState() == MOTION_LINE_PATH_CURVE);

    s_yawDeg = MOTION_LINE_CURVE_EXIT_ANGLE_DEG;
    MotionLine_Update(0.01f);
    CHECK(MotionLine_GetPathState() == MOTION_LINE_PATH_STRAIGHT);
}

int main(void)
{
    test_start_speed_is_ramped();
    test_profile_acceleration_tracks_the_ramp();
    test_profile_tunings_are_snapshotted_at_start();
    test_ir_side_controls_the_matching_wheel();
    test_finish_speed_and_stop_remain_continuous();
    test_line_loss_is_confirmed_quickly();
    test_curve_mode_uses_ch2_ch5_and_yaw_angle_exit();
    test_curve_speed_is_limited_after_curve_confirmation();
    test_curve_exit_uses_relative_yaw_angle();

    if (s_failures == 0)
    {
        printf("test_motionline: ALL PASS\n");
        return 0;
    }
    printf("test_motionline: %d FAILURE(S)\n", s_failures);
    return 1;
}
