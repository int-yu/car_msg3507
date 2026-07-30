#include "Application/Control/BallHold.h"
#include "Application/Control/BallBalance.h"
#include <math.h>

typedef struct
{
    BallHold_State_t state;
    BallHold_Error_t error;
    uint32_t elapsedTicks;
    uint32_t convergeTicks;
    /* 用秒而不是拍计超时：dt 由 App 给出，掉拍时按拍计会低估真实时间。 */
    float convergeSeconds;
    float feedforwardMMps2;
} BallHold_Context_t;

static BallHold_Context_t s_context;

static void BallHold_Fail(BallHold_Error_t error)
{
    /* 失败一律先让摆杆回中：保持一个大倾角只会把球推到挡片上。 */
    BallBalance_Stop();
    s_context.feedforwardMMps2 = 0.0f;
    s_context.error = error;
    s_context.state = BALL_HOLD_STATE_ERROR;
}

void BallHold_Init(void)
{
    s_context.state = BALL_HOLD_STATE_READY;
    s_context.error = BALL_HOLD_ERROR_NONE;
    s_context.elapsedTicks = 0U;
    s_context.convergeTicks = 0U;
    s_context.convergeSeconds = 0.0f;
    s_context.feedforwardMMps2 = 0.0f;
    BallBalance_Init();
}

uint8_t BallHold_Start(void)
{
    if (BallBalance_Start(BALL_HOLD_TARGET_MM) != BALL_BALANCE_RESULT_OK)
    {
        s_context.error = BALL_HOLD_ERROR_VISION;
        s_context.state = BALL_HOLD_STATE_ERROR;
        return 0U;
    }

    s_context.feedforwardMMps2 = 0.0f;
    s_context.elapsedTicks = 0U;
    s_context.convergeTicks = 0U;
    s_context.convergeSeconds = 0.0f;
    s_context.error = BALL_HOLD_ERROR_NONE;
    s_context.state = BALL_HOLD_STATE_CONVERGING;
    return 1U;
}

void BallHold_Update(float dt)
{
    if (BallHold_IsActive() == 0U)
    {
        return;
    }

    s_context.feedforwardMMps2 = 0.0f;
    BallBalance_Update(dt);

    if (BallBalance_GetState() == BALL_BALANCE_STATE_ERROR)
    {
        BallHold_Fail(
            (BallBalance_GetError() == BALL_BALANCE_ERROR_VISION_LOST) ?
                BALL_HOLD_ERROR_VISION :
                BALL_HOLD_ERROR_BALANCE);
        return;
    }

    if (s_context.elapsedTicks < UINT32_MAX)
    {
        s_context.elapsedTicks++;
    }

    if (s_context.state == BALL_HOLD_STATE_CONVERGING)
    {
        if (BallBalance_IsStable() != 0U)
        {
            s_context.convergeTicks = s_context.elapsedTicks;
            s_context.state = BALL_HOLD_STATE_HOLDING;
            return;
        }

        /*
         * 只有还没稳住过才计超时。进入保持后不再判：要求 4 就是要长期
         * 待在 O 点，给保持阶段设上限等于自己把成功判成失败。
         */
        if (isfinite(dt) && (dt > 0.0f))
        {
            s_context.convergeSeconds += dt;
        }
        if (s_context.convergeSeconds >= BALL_HOLD_CONVERGE_TIMEOUT_S)
        {
            BallHold_Fail(
                BALL_HOLD_ERROR_CONVERGE_TIMEOUT);
        }
        return;
    }

    /*
     * HOLDING：什么都不做就是对的。目标已经是 O，闭环每拍继续跑，
     * 球被手拨走后 BallBalance 会自己把它拉回来，不需要重新起一轮。
     */
}

void BallHold_Stop(void)
{
    BallBalance_Stop();
    s_context.feedforwardMMps2 = 0.0f;
    s_context.state = BALL_HOLD_STATE_FINISHED;
}

uint8_t BallHold_IsActive(void)
{
    return ((s_context.state == BALL_HOLD_STATE_CONVERGING) ||
            (s_context.state == BALL_HOLD_STATE_HOLDING)) ?
        1U : 0U;
}

BallHold_State_t BallHold_GetState(void)
{
    return s_context.state;
}

BallHold_Error_t BallHold_GetError(void)
{
    return s_context.error;
}

uint32_t BallHold_GetElapsedTicks(void)
{
    return s_context.elapsedTicks;
}

uint32_t BallHold_GetConvergeTicks(void)
{
    return s_context.convergeTicks;
}

float BallHold_GetFeedforwardMMps2(void)
{
    return s_context.feedforwardMMps2;
}
