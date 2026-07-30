#ifndef APPLICATION_CONTROL_BALL_HOLD_H
#define APPLICATION_CONTROL_BALL_HOLD_H

#include <stdint.h>

/*
 * H 题要求 4：钢球从摆杆上任意位置放下，摆杆把它收敛到中心点 O 并长期
 * 稳定在 O 点附近。本文件是「小车静止」这一步；A→B 直线段随后加进来时
 * 只需让底盘动起来，本模块不必再改。
 *
 * 与要求 3（O→+5cm→-5cm 序列，见 Application/Control/BallSequence）并列而不是复用：
 * 两者是不同测试项、不同时进行，序列逻辑也完全不同。把两套时序塞进一个
 * 状态机只会让已经跑通的要求 3 变脆。
 *
 * 分层不变：BallSensor（球位）→ BallBalance（梯形轨迹 + PD + 车体加速度
 * 前馈）→ BeamActuator（度→步）。本模块只管「目标恒为 O」这件事，以及
 * 把车体加速度前馈接上来。
 */

/* 目标就是摆杆中心。写成宏而不是散落的 0.0f，是为了让「要求 4 的目标是
 * O 点」这件事在代码里只有一个出处。 */
#define BALL_HOLD_TARGET_MM 0.0f

/*
 * 收敛超时（秒）。钢球最远可能放在 ±120 mm，按 BallBalance 的 100 mm/s
 * 和 250 mm/s^2 算，单程约 1.6 s，加上末端收敛留到 8 s 已经很宽松。
 * 只在「还没第一次稳住」时计时：一旦进入保持状态就不再超时，否则长时间
 * 保持反而会被自己判失败。
 */
#define BALL_HOLD_CONVERGE_TIMEOUT_S 8.0f

/*
 * 车体加速度前馈比例。1.0 = 完全按 MotionLine 的规划加速度补偿。
 * 小车静止时 MotionLine 的规划加速度本来就是 0，所以要求 4 这一步无论
 * 取多少都不影响结果；把它做成可调是为了 A→B 直线接进来后能在实车上
 * 直接加减前馈强度，而不必重新烧录。补偿明显过头（球被甩向反侧）就调小。
 */

/* 运行时可调，经 Param 的 K 命令读写；掉电恢复上面的默认值。 */

typedef enum
{
    BALL_HOLD_STATE_READY = 0,
    BALL_HOLD_STATE_CONVERGING, /* 正把球收回 O 点。 */
    BALL_HOLD_STATE_HOLDING,    /* 已稳住，持续闭环保持。 */
    BALL_HOLD_STATE_FINISHED,
    BALL_HOLD_STATE_ERROR
} BallHold_State_t;

typedef enum
{
    BALL_HOLD_ERROR_NONE = 0,
    BALL_HOLD_ERROR_VISION,  /* 起步时看不到球，或中途丢失。 */
    BALL_HOLD_ERROR_BALANCE, /* 平衡控制器自身进入错误。 */
    BALL_HOLD_ERROR_CONVERGE_TIMEOUT
} BallHold_Error_t;

void BallHold_Init(void);

/* 启动保持：钢球必须已被看到，位置不限。返回 0 表示没能起步。 */
uint8_t BallHold_Start(void);

/* 每个控制拍调用一次。内部会把车体加速度前馈喂给 BallBalance。 */
void BallHold_Update(float dt);

/* 中止并让摆杆回中。 */
void BallHold_Stop(void);

/* 正在收敛或保持。上层用它做要求 3/要求 4 的互斥判定。 */
uint8_t BallHold_IsActive(void);

BallHold_State_t BallHold_GetState(void);
BallHold_Error_t BallHold_GetError(void);
/* 本轮总耗时，供 OLED 和网页显示。 */
uint32_t BallHold_GetElapsedTicks(void);
/* 从起步到第一次稳住 O 点用了多少拍；未稳住时为 0。 */
uint32_t BallHold_GetConvergeTicks(void);
/* 最近一拍实际下发的车体加速度前馈，用于排查前馈是否接通。 */
float BallHold_GetFeedforwardMMps2(void);

#endif
