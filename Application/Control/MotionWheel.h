#ifndef APPLICATION_CONTROL_MOTION_WHEEL_H
#define APPLICATION_CONTROL_MOTION_WHEEL_H

#include <stdint.h>

/* 双轮速度控制公共参考值：保留给兼容入口；实车默认值以下方 LEFT/RIGHT 为准。 */
#define MOTION_WHEEL_KP                       0.6f     /* 左右 Kp 的公共参考值。 */
#define MOTION_WHEEL_KI                       0.0f    /* 消除稳态轮速误差；当前关闭。 */
#define MOTION_WHEEL_INTEGRAL_LIMIT           0.0f    /* KI 开启后限制积分累积。 */
#define MOTION_WHEEL_FEEDFORWARD_PWM_PER_MMPS 0.43702f /* 左右前馈斜率的算术平均值。 */
#define MOTION_WHEEL_STATIC_FRICTION_PWM      21.0445f /* 左右静摩擦补偿的算术平均值。 */
#define MOTION_WHEEL_MAX_COMMAND_PWM          1000.0f /* 每侧车轮最终 PWM 上限。 */

/* 2026-07-20 正向实测标定值：
 * - 开环 PWM150~600 的 21 个升降稳态点分别拟合左右前馈与静摩擦；
 * - Kp 0.5~0.9 细网格中左右均以 0.6 综合误差最低；
 * - Ki 的小幅收益尚未通过重复交叉试验确认，因此安全默认继续关闭。
 * 左右电机、减速箱和静摩擦不同，默认值必须分别固化。 */
#define MOTION_WHEEL_LEFT_KP                       0.6f
#define MOTION_WHEEL_LEFT_KI                       0.0f
#define MOTION_WHEEL_LEFT_INTEGRAL_LIMIT           0.0f
#define MOTION_WHEEL_LEFT_FEEDFORWARD_PWM_PER_MMPS 0.43114f
#define MOTION_WHEEL_LEFT_STATIC_FRICTION_PWM      19.884f
#define MOTION_WHEEL_RIGHT_KP                       0.6f
#define MOTION_WHEEL_RIGHT_KI                       0.0f
#define MOTION_WHEEL_RIGHT_INTEGRAL_LIMIT           0.0f
#define MOTION_WHEEL_RIGHT_FEEDFORWARD_PWM_PER_MMPS 0.44289f
#define MOTION_WHEEL_RIGHT_STATIC_FRICTION_PWM      22.205f

/* 运行时可调参数：上电恢复左右各自的 #define 默认值，由 K 命令经 Param 模块读写。
 * 左右 Kp/Ki/积分限幅写入后调用 MotionWheel_ApplyPidTunings() 进入对应 PID；
 * 左右前馈与静摩擦每拍直接读取，写入即生效。调好后写回对应 LEFT/RIGHT 宏固化。 */
extern float MotionWheel_TuneLeftKp;
extern float MotionWheel_TuneLeftKi;
extern float MotionWheel_TuneLeftIntegralLimit;
extern float MotionWheel_TuneLeftFeedforwardPWMPerMMps;
extern float MotionWheel_TuneLeftStaticFrictionPWM;
extern float MotionWheel_TuneRightKp;
extern float MotionWheel_TuneRightKi;
extern float MotionWheel_TuneRightIntegralLimit;
extern float MotionWheel_TuneRightFeedforwardPWMPerMMps;
extern float MotionWheel_TuneRightStaticFrictionPWM;

/* 上层控制器每个周期提交的左右轮目标和附加差速修正。 */
typedef struct
{
    float targetSpeedLMMps;
    float targetSpeedRMMps;
    float trimLPWM;
    float trimRPWM;
} MotionWheel_Command_t;

typedef enum
{
    MOTION_WHEEL_RESULT_OK = 0,
    MOTION_WHEEL_RESULT_INVALID_ARGUMENT,
    MOTION_WHEEL_RESULT_NOT_CONFIGURED,
    MOTION_WHEEL_RESULT_ODOMETRY_INVALID
} MotionWheel_Result_t;

MotionWheel_Result_t MotionWheel_Init(void);
MotionWheel_Result_t MotionWheel_Update(
    const MotionWheel_Command_t *command, float dt);
void MotionWheel_Reset(void);
void MotionWheel_Stop(void);

/* 把左右轮各自的 Tune 增益写入对应 PID，运行中调用也安全。 */
void MotionWheel_ApplyPidTunings(void);

uint8_t MotionWheel_IsConfigured(void);
float MotionWheel_GetMaximumCommandPWM(void);
float MotionWheel_GetLeftCommandPWM(void);
float MotionWheel_GetRightCommandPWM(void);
/* 最近一拍上层提交的目标轮速，供遥测输出目标 vs 实测曲线。 */
float MotionWheel_GetTargetSpeedL(void);
float MotionWheel_GetTargetSpeedR(void);

#endif
