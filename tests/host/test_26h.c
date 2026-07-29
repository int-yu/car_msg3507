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

int main(void)
{
    test_init_is_stopped_at_zero();
    test_first_key_edge_starts_without_counting_old_ticks();
    test_running_timer_accumulates_all_elapsed_ticks();

    if (s_failures == 0)
    {
        printf("test_26h: ALL PASS\n");
        return 0;
    }
    printf("test_26h: %d FAILURE(S)\n", s_failures);
    return 1;
}
