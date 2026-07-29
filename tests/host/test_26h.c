#include "Accomplish/26H.h"
#include "tests/host/test_assert.h"

static App_UpdateContext_t make_context(
    uint8_t elapsedTicks, uint8_t pressedEdges)
{
    App_UpdateContext_t context = {0};

    context.elapsedTicks = elapsedTicks;
    context.pressedEdges = pressedEdges;
    return context;
}

static void test_init_is_stopped_at_zero(void)
{
    Accomplish26H_Init();
    CHECK(Accomplish26H_IsTiming() == 0U);
    CHECK(Accomplish26H_GetElapsedTicks() == 0U);
}

static void test_first_key_edge_starts_without_counting_old_ticks(void)
{
    App_UpdateContext_t context = make_context(
        9U, ACCOMPLISH_26H_START_STOP_KEY_MASK);

    Accomplish26H_Init();
    Accomplish26H_Update(&context);
    CHECK(Accomplish26H_IsTiming() != 0U);
    CHECK(Accomplish26H_GetElapsedTicks() == 0U);
}

static void test_running_timer_accumulates_all_elapsed_ticks(void)
{
    App_UpdateContext_t context = make_context(
        1U, ACCOMPLISH_26H_START_STOP_KEY_MASK);

    Accomplish26H_Init();
    Accomplish26H_Update(&context);
    context = make_context(1U, 0U);
    Accomplish26H_Update(&context);
    context = make_context(37U, 0U);
    Accomplish26H_Update(&context);
    CHECK(Accomplish26H_GetElapsedTicks() == 38U);
}

static void test_held_key_without_new_edge_does_not_toggle(void)
{
    App_UpdateContext_t context = make_context(
        1U, ACCOMPLISH_26H_START_STOP_KEY_MASK);

    Accomplish26H_Init();
    Accomplish26H_Update(&context);
    context = make_context(5U, 0U);
    context.pressedKeys = ACCOMPLISH_26H_START_STOP_KEY_MASK;
    Accomplish26H_Update(&context);
    CHECK(Accomplish26H_IsTiming() != 0U);
    CHECK(Accomplish26H_GetElapsedTicks() == 5U);
}

static void test_second_key_edge_stops_and_freezes(void)
{
    App_UpdateContext_t context = make_context(
        9U, ACCOMPLISH_26H_START_STOP_KEY_MASK);

    Accomplish26H_Init();
    Accomplish26H_Update(&context);
    context = make_context(8U, ACCOMPLISH_26H_START_STOP_KEY_MASK);
    Accomplish26H_Update(&context);
    CHECK(Accomplish26H_IsTiming() == 0U);
    CHECK(Accomplish26H_GetElapsedTicks() == 8U);

    context = make_context(255U, 0U);
    Accomplish26H_Update(&context);
    CHECK(Accomplish26H_GetElapsedTicks() == 8U);
}

static void test_later_key_edge_resets_and_starts_new_run(void)
{
    App_UpdateContext_t context = make_context(
        1U, ACCOMPLISH_26H_START_STOP_KEY_MASK);

    Accomplish26H_Init();
    Accomplish26H_Update(&context);
    context = make_context(42U, 0U);
    Accomplish26H_Update(&context);
    context = make_context(1U, ACCOMPLISH_26H_START_STOP_KEY_MASK);
    Accomplish26H_Update(&context);
    context = make_context(7U, ACCOMPLISH_26H_START_STOP_KEY_MASK);
    Accomplish26H_Update(&context);
    CHECK(Accomplish26H_IsTiming() != 0U);
    CHECK(Accomplish26H_GetElapsedTicks() == 0U);
}

static void test_null_context_does_not_change_state(void)
{
    Accomplish26H_Init();
    Accomplish26H_Update(NULL);
    CHECK(Accomplish26H_IsTiming() == 0U);
    CHECK(Accomplish26H_GetElapsedTicks() == 0U);
}

static void test_tick_counter_saturates_instead_of_wrapping(void)
{
    App_UpdateContext_t context = make_context(
        1U, ACCOMPLISH_26H_START_STOP_KEY_MASK);
    uint32_t updateIndex;

    Accomplish26H_Init();
    Accomplish26H_Update(&context);
    context = make_context(UINT8_MAX, 0U);
    for (updateIndex = 0U;
         updateIndex < (UINT32_MAX / UINT8_MAX);
         updateIndex++)
    {
        Accomplish26H_Update(&context);
    }
    CHECK(Accomplish26H_GetElapsedTicks() == UINT32_MAX);

    context = make_context(1U, 0U);
    Accomplish26H_Update(&context);
    CHECK(Accomplish26H_GetElapsedTicks() == UINT32_MAX);
}

int main(void)
{
    test_init_is_stopped_at_zero();
    test_first_key_edge_starts_without_counting_old_ticks();
    test_running_timer_accumulates_all_elapsed_ticks();
    test_held_key_without_new_edge_does_not_toggle();
    test_second_key_edge_stops_and_freezes();
    test_later_key_edge_resets_and_starts_new_run();
    test_null_context_does_not_change_state();
    test_tick_counter_saturates_instead_of_wrapping();

    if (s_failures == 0)
    {
        printf("test_26h: ALL PASS\n");
        return 0;
    }
    printf("test_26h: %d FAILURE(S)\n", s_failures);
    return 1;
}
