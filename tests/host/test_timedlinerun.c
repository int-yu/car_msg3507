#include "Application/Control/TimedLineRun.h"
#include "Application/Control/MotionLine.h"
#include "Application/Control/MotionManager.h"
#include "Application/Control/TaskTimer.h"
#include "tests/host/test_assert.h"

static uint8_t s_lineOnline;
static MotionManager_Mode_t s_motionMode;
static MotionManager_Error_t s_motionError;
static uint8_t s_motionBusy;
static uint8_t s_motionFinished;
static uint16_t s_startCount;
static uint16_t s_requestStopCount;
static uint16_t s_stopCount;
static float s_startSpeedMMps;
static float s_startAccelerationMMps2;
static float s_speedLMMps;
static float s_speedRMMps;

float MotionLine_TuneAccelerationMMps2 = MOTION_LINE_ACCELERATION_MMPS2;

uint8_t Graydetect_IsOnline(void) { return s_lineOnline; }
float Odometry_GetSpeedL(void) { return s_speedLMMps; }
float Odometry_GetSpeedR(void) { return s_speedRMMps; }

MotionManager_Result_t MotionManager_StartLine(float speedMMps)
{
    s_startCount++;
    s_startSpeedMMps = speedMMps;
    s_startAccelerationMMps2 = MotionLine_TuneAccelerationMMps2;
    s_motionMode = MOTION_MANAGER_MODE_LINE;
    s_motionError = MOTION_MANAGER_ERROR_NONE;
    s_motionBusy = 1U;
    s_motionFinished = 0U;
    return MOTION_MANAGER_RESULT_OK;
}

MotionManager_Result_t MotionManager_RequestLineStop(void)
{
    if (s_motionMode != MOTION_MANAGER_MODE_LINE)
    {
        return MOTION_MANAGER_RESULT_START_FAILED;
    }
    s_requestStopCount++;
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

static App_UpdateContext_t make_context(
    uint8_t elapsedTicks, uint8_t pressedKeys)
{
    App_UpdateContext_t context = {0};

    context.elapsedTicks = elapsedTicks;
    context.pressedKeys = pressedKeys;
    return context;
}

static void reset_fakes(void)
{
    TaskTimer_Init();
    TimedLineRun_TuneAccelerationMMps2 =
        TIMED_LINE_RUN_ACCELERATION_MMPS2;
    TimedLineRun_TuneCruiseSpeedMMps =
        TIMED_LINE_RUN_CRUISE_SPEED_MMPS;
    TimedLineRun_TuneDurationSeconds =
        TIMED_LINE_RUN_DURATION_SECONDS;
    MotionLine_TuneAccelerationMMps2 = MOTION_LINE_ACCELERATION_MMPS2;
    s_lineOnline = 1U;
    s_motionMode = MOTION_MANAGER_MODE_IDLE;
    s_motionError = MOTION_MANAGER_ERROR_NONE;
    s_motionBusy = 0U;
    s_motionFinished = 0U;
    s_startCount = 0U;
    s_requestStopCount = 0U;
    s_stopCount = 0U;
    s_startSpeedMMps = 0.0f;
    s_startAccelerationMMps2 = 0.0f;
    s_speedLMMps = 0.0f;
    s_speedRMMps = 0.0f;
    TimedLineRun_Init();
}

static void test_start_uses_independent_tunable_profile(void)
{
    reset_fakes();
    TimedLineRun_TuneAccelerationMMps2 = 125.0f;
    TimedLineRun_TuneCruiseSpeedMMps = 430.0f;
    MotionLine_TuneAccelerationMMps2 = 777.0f;

    CHECK(TimedLineRun_Start() != 0U);
    CHECK(s_startCount == 1U);
    CHECK_NEAR(s_startSpeedMMps, 430.0f, 0.001f);
    CHECK_NEAR(s_startAccelerationMMps2, 125.0f, 0.001f);
    CHECK_NEAR(MotionLine_TuneAccelerationMMps2, 777.0f, 0.001f);
    CHECK(TimedLineRun_GetState() == TIMED_LINE_RUN_STATE_RUNNING);
    CHECK(TaskTimer_GetOwner() == TASK_TIMER_OWNER_LINE);
}

static void test_default_run_soft_stops_at_thirty_seconds(void)
{
    App_UpdateContext_t context = make_context(250U, 0U);
    uint8_t index;

    reset_fakes();
    CHECK(TimedLineRun_Start() != 0U);

    for (index = 0U; index < 11U; index++)
    {
        TimedLineRun_Update(&context);
    }
    CHECK(TimedLineRun_GetState() == TIMED_LINE_RUN_STATE_RUNNING);
    CHECK(s_requestStopCount == 0U);

    TimedLineRun_Update(&context);
    CHECK(TimedLineRun_GetElapsedTicks() == 3000U);
    CHECK(TimedLineRun_GetState() == TIMED_LINE_RUN_STATE_SOFT_STOP);
    CHECK(TimedLineRun_GetError() == TIMED_LINE_RUN_ERROR_NONE);
    CHECK(s_requestStopCount == 1U);
    CHECK(TaskTimer_IsRunning() == 0U);
}

static void test_run_duration_is_snapshotted_at_start(void)
{
    App_UpdateContext_t context = make_context(100U, 0U);
    uint8_t index;

    reset_fakes();
    TimedLineRun_TuneDurationSeconds = 2.0f;
    CHECK(TimedLineRun_Start() != 0U);
    TimedLineRun_TuneDurationSeconds = 50.0f;

    for (index = 0U; index < 2U; index++)
    {
        TimedLineRun_Update(&context);
    }
    CHECK(TimedLineRun_GetState() == TIMED_LINE_RUN_STATE_SOFT_STOP);
    CHECK(s_requestStopCount == 1U);
}

static void test_soft_stop_finishes_after_wheel_settles(void)
{
    App_UpdateContext_t context = make_context(1U, 0U);

    reset_fakes();
    TimedLineRun_TuneDurationSeconds = 0.01f;
    CHECK(TimedLineRun_Start() != 0U);
    TimedLineRun_Update(&context);
    CHECK(TimedLineRun_GetState() == TIMED_LINE_RUN_STATE_SOFT_STOP);

    s_motionBusy = 0U;
    s_motionFinished = 1U;
    TimedLineRun_Update(&context);
    CHECK(TimedLineRun_GetState() == TIMED_LINE_RUN_STATE_SETTLING);

    context.elapsedTicks = TIMED_LINE_RUN_SETTLE_CONFIRM_TICKS;
    TimedLineRun_Update(&context);
    CHECK(TimedLineRun_GetState() == TIMED_LINE_RUN_STATE_FINISHED);
    CHECK(TimedLineRun_GetError() == TIMED_LINE_RUN_ERROR_NONE);
}

static void test_emergency_chord_stops_active_run(void)
{
    App_UpdateContext_t context = make_context(
        1U, TIMED_LINE_RUN_EMERGENCY_KEY_MASK);

    reset_fakes();
    CHECK(TimedLineRun_Start() != 0U);
    TimedLineRun_Update(&context);

    CHECK(TimedLineRun_GetState() == TIMED_LINE_RUN_STATE_ERROR);
    CHECK(TimedLineRun_GetError() ==
          TIMED_LINE_RUN_ERROR_EMERGENCY_STOP);
    CHECK(s_stopCount == 1U);
}

int main(void)
{
    test_start_uses_independent_tunable_profile();
    test_default_run_soft_stops_at_thirty_seconds();
    test_run_duration_is_snapshotted_at_start();
    test_soft_stop_finishes_after_wheel_settles();
    test_emergency_chord_stops_active_run();

    if (s_failures == 0)
    {
        printf("test_timedlinerun: ALL PASS\n");
        return 0;
    }
    printf("test_timedlinerun: %d FAILURE(S)\n", s_failures);
    return 1;
}
