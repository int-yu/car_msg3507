#ifndef ACCOMPLISH_26H_BALL_H
#define ACCOMPLISH_26H_BALL_H

#include <stdint.h>

/*
 * H 题要求 3：小车静止时，摆杆把钢球从中心点 O 送到 +5 cm、到达后
 * 折返再送到 -5 cm 并稳定在该点附近。运行时间要求 <= 5 s，两个
 * ±5 cm 点的最大误差绝对值 <= 1 cm。
 *
 * 独立于要求 2 的单圈巡线：那是另一个测试项，两者不同时进行，所以
 * 这里不碰底盘，只驱动摆杆。
 *
 * 时间账（按 BallBalance 的 100 mm/s、250 mm/s^2）：O->+50 约 1.0 s，
 * 到位确认 0.15 s，+50->-50 约 1.4 s，终点收敛约 1.0 s，合计约 3.6 s，
 * 对 5 s 有余量。速度压低是主动用时间换稳定。
 */

/* 题目的 ±5 cm 两个目标点。 */
#define ACCOMPLISH_26H_BALL_TARGET_MM 50.0f

/* 整个序列的超时保护；100 Hz 下 6 秒。超过即判失败并回中，不要让
 * 摆杆在失控状态下继续动。 */
#define ACCOMPLISH_26H_BALL_TIMEOUT_TICKS 600U

typedef enum
{
    ACCOMPLISH_26H_BALL_STATE_READY = 0,
    ACCOMPLISH_26H_BALL_STATE_TO_PLUS,
    ACCOMPLISH_26H_BALL_STATE_TO_MINUS,
    ACCOMPLISH_26H_BALL_STATE_HOLD_MINUS,
    ACCOMPLISH_26H_BALL_STATE_FINISHED,
    ACCOMPLISH_26H_BALL_STATE_ERROR
} Accomplish26HBall_State_t;

typedef enum
{
    ACCOMPLISH_26H_BALL_ERROR_NONE = 0,
    ACCOMPLISH_26H_BALL_ERROR_VISION,   /* 起步时看不到球，或中途丢失。 */
    ACCOMPLISH_26H_BALL_ERROR_BALANCE,  /* 平衡控制器自身进入错误。 */
    ACCOMPLISH_26H_BALL_ERROR_TIMEOUT
} Accomplish26HBall_Error_t;

void Accomplish26HBall_Init(void);

/* 启动一轮 O -> +5 cm -> -5 cm 序列。钢球必须已被看到。 */
uint8_t Accomplish26HBall_Start(void);

/* 每个控制拍调用一次。 */
void Accomplish26HBall_Update(float dt);

/* 中止并让摆杆回中。 */
void Accomplish26HBall_Stop(void);

Accomplish26HBall_State_t Accomplish26HBall_GetState(void);
Accomplish26HBall_Error_t Accomplish26HBall_GetError(void);
/* 序列耗时，供 OLED 显示是否满足 5 s 要求。 */
uint32_t Accomplish26HBall_GetElapsedTicks(void);

#endif
