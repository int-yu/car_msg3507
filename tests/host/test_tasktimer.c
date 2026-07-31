#include "Application/Control/TaskTimer.h"
#include "tests/host/test_assert.h"

static void test_start_update_stop_is_owner_scoped(void)
{
    TaskTimer_Init();
    CHECK(TaskTimer_IsRunning() == 0U);

    TaskTimer_Start(TASK_TIMER_OWNER_LINE);
    CHECK(TaskTimer_IsRunning() != 0U);
    CHECK(TaskTimer_GetOwner() == TASK_TIMER_OWNER_LINE);
    CHECK(TaskTimer_GetElapsedTicks() == 0U);

    TaskTimer_Update(3U);
    CHECK(TaskTimer_GetElapsedTicks() == 3U);
    TaskTimer_Stop(TASK_TIMER_OWNER_BALL);
    CHECK(TaskTimer_IsRunning() != 0U);
    TaskTimer_Stop(TASK_TIMER_OWNER_LINE);
    CHECK(TaskTimer_IsRunning() == 0U);
    TaskTimer_Update(4U);
    CHECK(TaskTimer_GetElapsedTicks() == 3U);
}

static void test_start_resets_duration(void)
{
    TaskTimer_Start(TASK_TIMER_OWNER_BALL);
    TaskTimer_Update(100U);
    CHECK(TaskTimer_GetElapsedTicks() == 100U);
    TaskTimer_Start(TASK_TIMER_OWNER_LINE);
    CHECK(TaskTimer_GetElapsedTicks() == 0U);
    CHECK(TaskTimer_GetOwner() == TASK_TIMER_OWNER_LINE);
}

int main(void)
{
    test_start_update_stop_is_owner_scoped();
    test_start_resets_duration();

    if (s_failures == 0)
    {
        printf("test_tasktimer: ALL PASS\n");
        return 0;
    }
    printf("test_tasktimer: %d FAILURE(S)\n", s_failures);
    return 1;
}
