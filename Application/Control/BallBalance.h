#ifndef APPLICATION_CONTROL_BALL_BALANCE_H
#define APPLICATION_CONTROL_BALL_BALANCE_H

#include <stdint.h>

/*
 * 摆杆滚球平衡控制器：梯形速度轨迹 + PD + 加速度前馈，输出摆杆倾角。
 *
 * 全部以「摆杆倾角（度）」为单位，不知道执行器是什么、也不知道传动比；
 * 度到步的换算由 BeamActuator 独占。因此换执行器或改齿轮比都不需要
 * 重调这里的增益。
 *
 * 物理模型：凹槽内的钢球在小角度下 x'' = k * theta - a_car。
 * k 是重力耦合系数，实心球在平面上是 (5/7)g，V 型槽接触半径更小、
 * 系数更大，默认取 0.6g 作起点，必须实测修正。
 * a_car 是车体加速度带来的惯性力，要求 3 静止时恒为 0。
 */

/* 重力耦合系数，单位 mm/s^2 每度。0.6g = 5886 mm/s^2 每弧度，
 * 折合每度 5886 * pi / 180 约 102.8。 */
#define BALL_BALANCE_GRAVITY_COUPLING_MMPS2_PER_DEG 102.8f

/*
 * PD 增益按 ωn = 5 rad/s、ζ = 0.9 设计。ωn 被相机带宽（约 25 fps）
 * 卡住不能再高；ζ 接近临界阻尼是为了不让过冲吃掉 1 cm 的误差预算。
 * Kp = ωn^2 / k、Kd = 2ζωn / k，单位分别是 度/mm 和 度/(mm/s)。
 */
#define BALL_BALANCE_KP_DEG_PER_MM      0.24f
#define BALL_BALANCE_KD_DEG_PER_MMPS    0.087f

/* 摆杆机械可用行程。要 0.3 m/s^2 只需约 2.9 度，行程本就很小，
 * 超出会撞限位或让连杆卡死。 */
#define BALL_BALANCE_MAX_TILT_DEG       6.0f

/* 梯形轨迹参数。速度压低是主动用时间换稳定：题目只在 ±5 cm 两个静止
 * 点要精度，那里速度归零，相机延迟不产生位置误差。 */
#define BALL_BALANCE_MAX_SPEED_MMPS     100.0f
#define BALL_BALANCE_ACCELERATION_MMPS2 250.0f

/* 到位判据：题目要求 1 cm，这里留一半余量。 */
#define BALL_BALANCE_SETTLE_TOLERANCE_MM 5.0f
#define BALL_BALANCE_SETTLE_CONFIRM_TICKS 15U

/* 视觉失效多久后放弃闭环并回中。100 Hz 下 100 ms。 */
#define BALL_BALANCE_VISION_LOST_TICKS  10U

/* 运行时可调，实测标定后写入；掉电恢复上面的 #define。 */
extern float BallBalance_TuneKp;
extern float BallBalance_TuneKd;
extern float BallBalance_TuneGravityCoupling;

typedef enum
{
    BALL_BALANCE_STATE_IDLE = 0,
    BALL_BALANCE_STATE_RUNNING,
    BALL_BALANCE_STATE_ERROR
} BallBalance_State_t;

typedef enum
{
    BALL_BALANCE_ERROR_NONE = 0,
    BALL_BALANCE_ERROR_VISION_LOST,
    BALL_BALANCE_ERROR_UPDATE_PERIOD_INVALID
} BallBalance_Error_t;

typedef enum
{
    BALL_BALANCE_RESULT_OK = 0,
    BALL_BALANCE_RESULT_INVALID_ARGUMENT,
    BALL_BALANCE_RESULT_NOT_RUNNING
} BallBalance_Result_t;

void BallBalance_Init(void);

/* 启动闭环。必须在钢球已被看到时调用。 */
BallBalance_Result_t BallBalance_Start(void);

/* 设置目标位置（mm，以摆杆中心 O 为原点）。轨迹发生器会平滑过去，
 * 不会把阶跃直接送给 PD——那会命令远超行程的倾角把球打到挡片。 */
BallBalance_Result_t BallBalance_SetTarget(float targetMM);

/* 车体加速度前馈（mm/s^2）。要求 3 静止时传 0；要求 4/5/6 传
 * MotionLine_GetProfileAccelerationMMps2()。 */
void BallBalance_SetCarAcceleration(float accelerationMMps2);

void BallBalance_Update(float dt);
void BallBalance_Stop(void);

BallBalance_State_t BallBalance_GetState(void);
BallBalance_Error_t BallBalance_GetError(void);
/* 钢球在目标附近且持续足够久。任务层用它判定「到位可以折返」。 */
uint8_t BallBalance_IsStable(void);
float BallBalance_GetPositionMM(void);
float BallBalance_GetTargetMM(void);
/* 轨迹当前的参考位置，用于排查是轨迹没到还是球没跟上。 */
float BallBalance_GetProfilePositionMM(void);
float BallBalance_GetTiltCommandDeg(void);

#endif
