#include "Accomplish/26H_Ball.h"
#include "Application/Control/BallBalance.h"
#include <math.h>

typedef struct
{
    Accomplish26HBall_State_t state;
    Accomplish26HBall_Error_t error;
    uint32_t elapsedTicks;
} Accomplish26HBall_Context_t;

static Accomplish26HBall_Context_t s_context;

static void Accomplish26HBall_Fail(Accomplish26HBall_Error_t error)
{
    /* 失败一律先让摆杆回中：继续保持一个大倾角只会把球推到挡片上。 */
    BallBalance_Stop();
    s_context.error = error;
    s_context.state = ACCOMPLISH_26H_BALL_STATE_ERROR;
}

void Accomplish26HBall_Init(void)
{
    s_context.state = ACCOMPLISH_26H_BALL_STATE_READY;
    s_context.error = ACCOMPLISH_26H_BALL_ERROR_NONE;
    s_context.elapsedTicks = 0U;
    BallBalance_Init();
}

uint8_t Accomplish26HBall_Start(void)
{
    if (BallBalance_Start() != BALL_BALANCE_RESULT_OK)
    {
        s_context.error = ACCOMPLISH_26H_BALL_ERROR_VISION;
        s_context.state = ACCOMPLISH_26H_BALL_STATE_ERROR;
        return 0U;
    }
    if (BallBalance_SetTarget(ACCOMPLISH_26H_BALL_TARGET_MM) !=
        BALL_BALANCE_RESULT_OK)
    {
        Accomplish26HBall_Fail(ACCOMPLISH_26H_BALL_ERROR_BALANCE);
        return 0U;
    }

    /* 要求 3 是静止测试，车体加速度前馈恒为零。 */
    BallBalance_SetCarAcceleration(0.0f);
    s_context.elapsedTicks = 0U;
    s_context.error = ACCOMPLISH_26H_BALL_ERROR_NONE;
    s_context.state = ACCOMPLISH_26H_BALL_STATE_TO_PLUS;
    return 1U;
}

void Accomplish26HBall_Update(float dt)
{
    if ((s_context.state != ACCOMPLISH_26H_BALL_STATE_TO_PLUS) &&
        (s_context.state != ACCOMPLISH_26H_BALL_STATE_TO_MINUS) &&
        (s_context.state != ACCOMPLISH_26H_BALL_STATE_HOLD_MINUS))
    {
        return;
    }

    BallBalance_Update(dt);

    if (BallBalance_GetState() == BALL_BALANCE_STATE_ERROR)
    {
        Accomplish26HBall_Fail(
            (BallBalance_GetError() == BALL_BALANCE_ERROR_VISION_LOST) ?
                ACCOMPLISH_26H_BALL_ERROR_VISION :
                ACCOMPLISH_26H_BALL_ERROR_BALANCE);
        return;
    }

    if (s_context.elapsedTicks < UINT32_MAX)
    {
        s_context.elapsedTicks++;
    }
    if (s_context.elapsedTicks >= ACCOMPLISH_26H_BALL_TIMEOUT_TICKS)
    {
        Accomplish26HBall_Fail(ACCOMPLISH_26H_BALL_ERROR_TIMEOUT);
        return;
    }

    switch (s_context.state)
    {
        case ACCOMPLISH_26H_BALL_STATE_TO_PLUS:
            if (BallBalance_IsStable() != 0U)
            {
                if (BallBalance_SetTarget(
                        -ACCOMPLISH_26H_BALL_TARGET_MM) !=
                    BALL_BALANCE_RESULT_OK)
                {
                    Accomplish26HBall_Fail(
                        ACCOMPLISH_26H_BALL_ERROR_BALANCE);
                    return;
                }
                s_context.state = ACCOMPLISH_26H_BALL_STATE_TO_MINUS;
            }
            break;

        case ACCOMPLISH_26H_BALL_STATE_TO_MINUS:
            if (BallBalance_IsStable() != 0U)
            {
                /* 题目要求"稳定在该点附近"，所以到达 -5 cm 后继续保持
                 * 闭环，不能停控——一停摆杆回中球就滚走了。 */
                s_context.state = ACCOMPLISH_26H_BALL_STATE_HOLD_MINUS;
            }
            break;

        case ACCOMPLISH_26H_BALL_STATE_HOLD_MINUS:
            break;

        default:
            break;
    }
}

void Accomplish26HBall_Stop(void)
{
    BallBalance_Stop();
    s_context.state = ACCOMPLISH_26H_BALL_STATE_FINISHED;
}

Accomplish26HBall_State_t Accomplish26HBall_GetState(void)
{
    return s_context.state;
}

Accomplish26HBall_Error_t Accomplish26HBall_GetError(void)
{
    return s_context.error;
}

uint32_t Accomplish26HBall_GetElapsedTicks(void)
{
    return s_context.elapsedTicks;
}
