#include "Accomplish/26H.h"
#include "Application/Control/MotionManager.h"
#include "Application/Control/TaskTimer.h"
#include "Hardware/Sensors/Graydetect.h"
#include "tests/host/test_assert.h"

static uint8_t s_lineOnline;
static uint8_t s_lineState;
static float s_distanceMM;
static float s_speedLMMps;
static float s_speedRMMps;
static MotionManager_Mode_t s_motionMode;
static MotionManager_Error_t s_motionError;
static uint8_t s_motionBusy;
static uint8_t s_motionFinished;
static MotionManager_Result_t s_startResult;
static uint16_t s_startLineCount;
static uint16_t s_setSpeedCount;
static uint16_t s_requestStopCount;
static uint16_t s_startBrakeCount;
static uint16_t s_stopCount;
static float s_startLineSpeedMMps;
static float s_lastLineSpeedMMps;

/* ---- 26H 状态机使用的最小宿主桩 ---- */
uint8_t Graydetect_IsOnline(void) { return s_lineOnline; }
uint8_t Graydetect_GetState(void) { return s_lineState; }

float Odometry_GetDistanceMM(void) { return s_distanceMM; }
float Odometry_GetSpeedL(void) { return s_speedLMMps; }
float Odometry_GetSpeedR(void) { return s_speedRMMps; }

MotionManager_Result_t MotionManager_StartRequirement2Line(float speedMMps)
{
    s_startLineCount++;
    s_startLineSpeedMMps = speedMMps;
    if (s_startResult == MOTION_MANAGER_RESULT_OK)
    {
        s_motionMode = MOTION_MANAGER_MODE_REQUIREMENT2_LINE;
        s_motionBusy = 1U;
        s_motionFinished = 0U;
        s_motionError = MOTION_MANAGER_ERROR_NONE;
    }
    return s_startResult;
}

MotionManager_Result_t MotionManager_SetRequirement2LineSpeed(float speedMMps)
{
    if (s_motionMode != MOTION_MANAGER_MODE_REQUIREMENT2_LINE)
    {
        return MOTION_MANAGER_RESULT_START_FAILED;
    }
    s_setSpeedCount++;
    s_lastLineSpeedMMps = speedMMps;
    return MOTION_MANAGER_RESULT_OK;
}

MotionManager_Result_t MotionManager_RequestRequirement2LineStop(void)
{
    if (s_motionMode != MOTION_MANAGER_MODE_REQUIREMENT2_LINE)
    {
        return MOTION_MANAGER_RESULT_START_FAILED;
    }
    s_requestStopCount++;
    return MOTION_MANAGER_RESULT_OK;
}

MotionManager_Result_t MotionManager_StartBrake(void)
{
    s_startBrakeCount++;
    MotionManager_Stop();
    s_motionMode = MOTION_MANAGER_MODE_BRAKE;
    s_motionBusy = 1U;
    s_motionFinished = 0U;
    s_motionError = MOTION_MANAGER_ERROR_NONE;
    return MOTION_MANAGER_RESULT_OK;
}

void MotionManager_Stop(void)
{
    s_stopCount++;
    s_motionMode = MOTION_MANAGER_MODE_IDLE;
    s_motionBusy = 0U;
    s_motionFinished = 0U;
    s_motionError = MOTION_MANAGER_ERROR_NONE;
}

uint8_t MotionManager_IsBusy(void) { return s_motionBusy; }
uint8_t MotionManager_IsFinished(void) { return s_motionFinished; }
MotionManager_Mode_t MotionManager_GetMode(void) { return s_motionMode; }
MotionManager_Error_t MotionManager_GetError(void) { return s_motionError; }

static void reset_fakes(void)
{
    TaskTimer_Init();
    s_lineOnline = 1U;
    s_lineState = 0x03U;
    s_distanceMM = 0.0f;
    s_speedLMMps = 0.0f;
    s_speedRMMps = 0.0f;
    s_motionMode = MOTION_MANAGER_MODE_IDLE;
    s_motionError = MOTION_MANAGER_ERROR_NONE;
    s_motionBusy = 0U;
    s_motionFinished = 0U;
    s_startResult = MOTION_MANAGER_RESULT_OK;
    s_startLineCount = 0U;
    s_setSpeedCount = 0U;
    s_requestStopCount = 0U;
    s_startBrakeCount = 0U;
    s_stopCount = 0U;
    s_startLineSpeedMMps = 0.0f;
    s_lastLineSpeedMMps = 0.0f;
    Accomplish26H_TuneCruiseSpeedMMps =
        ACCOMPLISH_26H_CRUISE_SPEED_MMPS;
    Accomplish26H_TuneFinishCrawlSpeedMMps =
        ACCOMPLISH_26H_FINISH_CRAWL_SPEED_MMPS;
    Accomplish26H_TuneStartClearDistanceMM =
        ACCOMPLISH_26H_START_CLEAR_DISTANCE_MM;
    Accomplish26H_TuneNominalLapDistanceMM =
        ACCOMPLISH_26H_NOMINAL_LAP_DISTANCE_MM;
    Accomplish26H_TuneFinishApproachDistanceMM =
        ACCOMPLISH_26H_FINISH_APPROACH_DISTANCE_MM;
    Accomplish26H_TuneFinishMarkerArmDistanceMM =
        ACCOMPLISH_26H_FINISH_MARKER_ARM_DISTANCE_MM;
    Accomplish26H_TuneMaxLapDistanceMM =
        ACCOMPLISH_26H_MAX_LAP_DISTANCE_MM;
    Accomplish26H_TuneFinishRolloutMM =
        ACCOMPLISH_26H_FINISH_ROLLOUT_MM;
}

static App_UpdateContext_t make_context(
    uint8_t elapsedTicks, uint8_t pressedEdges)
{
    App_UpdateContext_t context = {0};

    context.elapsedTicks = elapsedTicks;
    context.pressedEdges = pressedEdges;
    return context;
}

static void start_one_lap(void)
{
    App_UpdateContext_t context = make_context(
        1U, ACCOMPLISH_26H_START_STOP_KEY_MASK);

    Accomplish26H_Update(&context);
}

static void leave_start_line(void)
{
    App_UpdateContext_t context = make_context(3U, 0U);

    s_distanceMM = ACCOMPLISH_26H_START_CLEAR_DISTANCE_MM;
    s_lineState = 0x03U;
    Accomplish26H_Update(&context);
}

static void test_init_is_ready_at_zero(void)
{
    reset_fakes();
    Accomplish26H_Init();
    CHECK(Accomplish26H_GetState() == ACCOMPLISH_26H_STATE_READY);
    CHECK(Accomplish26H_GetError() == ACCOMPLISH_26H_ERROR_NONE);
    CHECK(Accomplish26H_IsTiming() == 0U);
    CHECK(Accomplish26H_GetElapsedTicks() == 0U);
}

static void test_parameters_are_snapshotted_at_key1_start(void)
{
    App_UpdateContext_t context = make_context(3U, 0U);

    reset_fakes();
    Accomplish26H_TuneCruiseSpeedMMps = 520.0f;
    Accomplish26H_TuneFinishCrawlSpeedMMps = 180.0f;
    Accomplish26H_TuneStartClearDistanceMM = 120.0f;
    Accomplish26H_TuneNominalLapDistanceMM = 6200.0f;
    Accomplish26H_TuneFinishApproachDistanceMM = 500.0f;
    Accomplish26H_TuneFinishMarkerArmDistanceMM = 1700.0f;
    Accomplish26H_TuneMaxLapDistanceMM = 6800.0f;
    Accomplish26H_TuneFinishRolloutMM = 40.0f;
    Accomplish26H_Init();
    start_one_lap();

    CHECK_NEAR(s_startLineSpeedMMps, 520.0f, 0.001f);
    CHECK(TaskTimer_IsRunning() != 0U);
    CHECK(TaskTimer_GetOwner() == TASK_TIMER_OWNER_LINE);

    /* 网页运行中继续写 K 参数，只能影响下一次 KEY1，不能改本圈阶段。 */
    Accomplish26H_TuneCruiseSpeedMMps = 700.0f;
    Accomplish26H_TuneFinishCrawlSpeedMMps = 90.0f;
    Accomplish26H_TuneStartClearDistanceMM = 10.0f;
    Accomplish26H_TuneNominalLapDistanceMM = 3000.0f;
    Accomplish26H_TuneFinishApproachDistanceMM = 1000.0f;
    Accomplish26H_TuneFinishMarkerArmDistanceMM = 2000.0f;
    Accomplish26H_TuneMaxLapDistanceMM = 3200.0f;
    Accomplish26H_TuneFinishRolloutMM = 0.0f;

    s_distanceMM = 10.0f;
    Accomplish26H_Update(&context);
    CHECK(Accomplish26H_GetState() ==
          ACCOMPLISH_26H_STATE_LEAVING_START);

    s_distanceMM = 120.0f;
    Accomplish26H_Update(&context);
    CHECK(Accomplish26H_GetState() == ACCOMPLISH_26H_STATE_RUNNING);

    context = make_context(1U, 0U);
    s_distanceMM = 5700.0f; /* 6200 - 500，本圈快照的减速起点。 */
    Accomplish26H_Update(&context);
    CHECK(s_setSpeedCount == 1U);
    CHECK_NEAR(s_lastLineSpeedMMps, 180.0f, 0.001f);

    s_distanceMM = 1701.0f;
    s_lineState = 0x3FU;
    Accomplish26H_Update(&context);
    Accomplish26H_Update(&context);
    CHECK(s_startBrakeCount == 1U);
    CHECK(Accomplish26H_GetState() == ACCOMPLISH_26H_STATE_SOFT_STOP);
}

static void test_next_key1_uses_new_parameters(void)
{
    reset_fakes();
    Accomplish26H_TuneCruiseSpeedMMps = 710.0f;
    Accomplish26H_Init();
    start_one_lap();

    CHECK_NEAR(s_startLineSpeedMMps, 710.0f, 0.001f);
}

static void test_invalid_stage_configuration_is_rejected_before_motion(void)
{
    reset_fakes();
    Accomplish26H_TuneCruiseSpeedMMps = 200.0f;
    Accomplish26H_TuneFinishCrawlSpeedMMps = 300.0f;
    Accomplish26H_Init();
    start_one_lap();

    CHECK(s_startLineCount == 0U);
    CHECK(Accomplish26H_GetState() == ACCOMPLISH_26H_STATE_ERROR);
    CHECK(Accomplish26H_GetError() == ACCOMPLISH_26H_ERROR_START);
}

static void test_key1_starts_line_and_timer(void)
{
    reset_fakes();
    Accomplish26H_Init();
    start_one_lap();

    CHECK(s_startLineCount == 1U);
    CHECK(Accomplish26H_GetState() ==
          ACCOMPLISH_26H_STATE_LEAVING_START);
    CHECK(Accomplish26H_IsTiming() != 0U);
    CHECK(Accomplish26H_GetElapsedTicks() == 0U);
    CHECK(TaskTimer_IsRunning() != 0U);
    CHECK(TaskTimer_GetOwner() == TASK_TIMER_OWNER_LINE);
    CHECK_NEAR(s_startLineSpeedMMps,
               ACCOMPLISH_26H_CRUISE_SPEED_MMPS, 0.001f);
}

static void test_start_line_is_left_before_finish_is_armed(void)
{
    reset_fakes();
    Accomplish26H_Init();
    start_one_lap();
    leave_start_line();

    CHECK(Accomplish26H_GetState() == ACCOMPLISH_26H_STATE_RUNNING);
    CHECK(Accomplish26H_GetElapsedTicks() == 3U);
}

static void test_finish_approach_slows_before_marker(void)
{
    App_UpdateContext_t context = make_context(1U, 0U);

    reset_fakes();
    Accomplish26H_Init();
    start_one_lap();
    leave_start_line();

    s_distanceMM = ACCOMPLISH_26H_NOMINAL_LAP_DISTANCE_MM -
                   ACCOMPLISH_26H_FINISH_APPROACH_DISTANCE_MM;
    Accomplish26H_Update(&context);
    CHECK(s_setSpeedCount == 1U);
    CHECK_NEAR(s_lastLineSpeedMMps,
               ACCOMPLISH_26H_FINISH_CRAWL_SPEED_MMPS, 0.001f);
}

static void test_marker_brakes_then_settles_before_freezing_time(void)
{
    App_UpdateContext_t context = make_context(1U, 0U);

    reset_fakes();
    Accomplish26H_Init();
    start_one_lap();
    leave_start_line();

    s_distanceMM = ACCOMPLISH_26H_FINISH_MARKER_ARM_DISTANCE_MM + 1.0f;
    s_lineState = 0x3FU;
    Accomplish26H_Update(&context);
    CHECK(Accomplish26H_GetState() == ACCOMPLISH_26H_STATE_RUNNING);

    Accomplish26H_Update(&context);
    CHECK(s_startBrakeCount == 1U);
    CHECK(s_requestStopCount == 0U);
    CHECK(Accomplish26H_GetState() == ACCOMPLISH_26H_STATE_SOFT_STOP);
    CHECK(TaskTimer_IsRunning() == 0U);

    s_motionBusy = 0U;
    s_motionFinished = 1U;
    Accomplish26H_Update(&context);
    CHECK(Accomplish26H_GetState() ==
          ACCOMPLISH_26H_STATE_SETTLING);
    CHECK(Accomplish26H_IsTiming() != 0U);

    context = make_context(ACCOMPLISH_26H_SETTLE_CONFIRM_TICKS, 0U);
    Accomplish26H_Update(&context);
    CHECK(Accomplish26H_GetState() ==
          ACCOMPLISH_26H_STATE_FINISHED);
    CHECK(Accomplish26H_IsTiming() == 0U);
    CHECK(Accomplish26H_GetError() == ACCOMPLISH_26H_ERROR_NONE);
}

static void test_marker_requires_two_consecutive_three_or_more_channel_samples(void)
{
    App_UpdateContext_t context = make_context(1U, 0U);

    reset_fakes();
    Accomplish26H_Init();
    start_one_lap();
    leave_start_line();

    s_distanceMM = ACCOMPLISH_26H_FINISH_MARKER_ARM_DISTANCE_MM + 1.0f;
    /* 任意三个通道压线都可作为终点横线，不要求相邻或六路全黑。 */
    s_lineState = 0x25U;
    Accomplish26H_Update(&context);
    CHECK(Accomplish26H_GetState() == ACCOMPLISH_26H_STATE_RUNNING);

    s_lineState = 0x03U;
    Accomplish26H_Update(&context);
    CHECK(Accomplish26H_GetState() == ACCOMPLISH_26H_STATE_RUNNING);

    s_lineState = 0x25U;
    Accomplish26H_Update(&context);
    Accomplish26H_Update(&context);
    CHECK(s_startBrakeCount == 1U);
    CHECK(Accomplish26H_GetState() == ACCOMPLISH_26H_STATE_SOFT_STOP);
}

static void test_marker_is_ignored_until_distance_is_greater_than_threshold(void)
{
    App_UpdateContext_t context = make_context(1U, 0U);

    reset_fakes();
    Accomplish26H_Init();
    start_one_lap();
    leave_start_line();

    s_distanceMM = ACCOMPLISH_26H_FINISH_MARKER_ARM_DISTANCE_MM;
    s_lineState = 0x3FU;
    Accomplish26H_Update(&context);
    Accomplish26H_Update(&context);
    Accomplish26H_Update(&context);
    CHECK(Accomplish26H_GetState() == ACCOMPLISH_26H_STATE_RUNNING);
    CHECK(s_startBrakeCount == 0U);
}

static void test_time_limit_freezes_timer_and_soft_stops(void)
{
    App_UpdateContext_t context = make_context(250U, 0U);
    uint8_t index;

    reset_fakes();
    Accomplish26H_Init();
    start_one_lap();
    leave_start_line();

    for (index = 0U; index < 8U; index++)
    {
        Accomplish26H_Update(&context);
    }

    CHECK(Accomplish26H_GetState() == ACCOMPLISH_26H_STATE_SOFT_STOP);
    CHECK(Accomplish26H_GetError() == ACCOMPLISH_26H_ERROR_TIME_LIMIT);
    CHECK(Accomplish26H_IsTiming() == 0U);
    CHECK(s_requestStopCount == 1U);
}

static void test_sensor_offline_stops_without_blind_run(void)
{
    App_UpdateContext_t context = make_context(1U, 0U);

    reset_fakes();
    Accomplish26H_Init();
    start_one_lap();
    s_lineOnline = 0U;
    Accomplish26H_Update(&context);

    CHECK(s_stopCount == 1U);
    CHECK(Accomplish26H_GetState() == ACCOMPLISH_26H_STATE_ERROR);
    CHECK(Accomplish26H_GetError() ==
          ACCOMPLISH_26H_ERROR_SENSOR_OFFLINE);
    CHECK(Accomplish26H_IsTiming() == 0U);
}

static void test_key_chord_stops_and_freezes(void)
{
    App_UpdateContext_t context;

    reset_fakes();
    Accomplish26H_Init();
    start_one_lap();
    context = make_context(9U, ACCOMPLISH_26H_EMERGENCY_STOP_KEY_MASK);
    context.pressedKeys = ACCOMPLISH_26H_EMERGENCY_STOP_KEY_MASK;
    Accomplish26H_Update(&context);

    CHECK(s_stopCount == 1U);
    CHECK(Accomplish26H_GetState() == ACCOMPLISH_26H_STATE_ERROR);
    CHECK(Accomplish26H_GetError() ==
          ACCOMPLISH_26H_ERROR_EMERGENCY_STOP);
    CHECK(Accomplish26H_IsTiming() == 0U);
    CHECK(Accomplish26H_GetElapsedTicks() == 0U);
}

static void test_cancel_returns_to_ready_and_stops(void)
{
    reset_fakes();
    Accomplish26H_Init();
    start_one_lap();

    Accomplish26H_Cancel();

    CHECK(s_stopCount == 1U);
    CHECK(TaskTimer_IsRunning() == 0U);
    CHECK(Accomplish26H_GetState() == ACCOMPLISH_26H_STATE_READY);
    CHECK(Accomplish26H_GetError() == ACCOMPLISH_26H_ERROR_NONE);
    CHECK(Accomplish26H_IsTiming() == 0U);
    CHECK(Accomplish26H_GetElapsedTicks() == 0U);
}

int main(void)
{
    test_init_is_ready_at_zero();
    test_key1_starts_line_and_timer();
    test_parameters_are_snapshotted_at_key1_start();
    test_next_key1_uses_new_parameters();
    test_invalid_stage_configuration_is_rejected_before_motion();
    test_start_line_is_left_before_finish_is_armed();
    test_finish_approach_slows_before_marker();
    test_marker_brakes_then_settles_before_freezing_time();
    test_marker_requires_two_consecutive_three_or_more_channel_samples();
    test_marker_is_ignored_until_distance_is_greater_than_threshold();
    test_time_limit_freezes_timer_and_soft_stops();
    test_sensor_offline_stops_without_blind_run();
    test_key_chord_stops_and_freezes();
    test_cancel_returns_to_ready_and_stops();

    if (s_failures == 0)
    {
        printf("test_26h: ALL PASS\n");
        return 0;
    }
    printf("test_26h: %d FAILURE(S)\n", s_failures);
    return 1;
}
