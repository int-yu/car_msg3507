#ifndef APPLICATION_CONTROL_MOTION_LANE_H
#define APPLICATION_CONTROL_MOTION_LANE_H

#include <stdint.h>

/*
 * 视觉巡道：白路面 + 两侧黑线。误差来自 K230 的 LANE 帧，与灰度巡线
 * （MotionLine）互斥——正常行驶时车在两条黑线之间的白路面上，灰度阵列
 * 全白，MotionLine 会立刻判丢线结束任务，不能复用。
 */

#define MOTION_LANE_MAX_SPEED_MMPS      1000.0f /* Start() 允许的巡道速度上限。 */

/* 千分比误差 → mm/s 差速。满偏 500 千分比时贡献 100 mm/s。 */
#define MOTION_LANE_KP                  0.20f

/* (°/s) → mm/s 阻尼。100 °/s 的转向速率贡献 30 mm/s 反向修正。 */
#define MOTION_LANE_KD_YAW              0.30f

#define MOTION_LANE_MAX_ADJUST_RATIO    0.35f   /* 差速修正相对巡航速度的上限。 */

/* 视觉数据允许的最大陈旧拍数。K230 约 25 fps（40 ms/帧），100 ms 容忍
 * 连丢 2~3 帧。比 K230Link 自己的 300 ms 链路超时更紧：那个超时是给
 * "链路还活着但偶尔丢帧"用的，而控制律拿着更旧的数据打方向没有意义。
 * 有了这道闸，端到端最长盲开 = 本阈值 + LOST_HOLD_TICKS = 400 ms，
 * 而不是两个 300 ms 窗口串起来的 600 ms。 */
#define MOTION_LANE_MAX_LANE_AGE_TICKS  10U

/* 100 Hz 下 300 ms。这是"视觉已经被判定失效"之后的保持窗口——短暂
 * 丢失时先保持上一拍轮速，超过这个窗口才结束任务；不是从链路物理断开
 * 那一刻开始算的总时长，那段由 MOTION_LANE_MAX_LANE_AGE_TICKS 单独把关。
 * 二者相加才是端到端最长盲开时间，见上面的注释。 */
#define MOTION_LANE_LOST_HOLD_TICKS     30U
#define MOTION_LANE_MIN_CONFIDENCE      30U     /* 低于此置信度按丢失处理。 */

typedef enum
{
    MOTION_LANE_STATE_IDLE = 0,
    MOTION_LANE_STATE_RUNNING,
    MOTION_LANE_STATE_FINISHED,
    MOTION_LANE_STATE_ERROR
} MotionLane_State_t;

typedef enum
{
    MOTION_LANE_ERROR_NONE = 0,
    MOTION_LANE_ERROR_UPDATE_PERIOD_INVALID,
    MOTION_LANE_ERROR_WHEEL
} MotionLane_Error_t;

typedef enum
{
    MOTION_LANE_RESULT_OK = 0,
    MOTION_LANE_RESULT_BUSY,
    MOTION_LANE_RESULT_INVALID_ARGUMENT,
    MOTION_LANE_RESULT_NOT_CONFIGURED
} MotionLane_Result_t;

/* 运行时可调参数，默认值取上面的 #define；范围校验由 Param 模块负责。 */
extern float MotionLane_TuneKp;
extern float MotionLane_TuneKdYaw;
extern float MotionLane_TuneMaxAdjustRatio;

MotionLane_Result_t MotionLane_Init(void);
MotionLane_Result_t MotionLane_Start(float speedMMps);
void MotionLane_Update(float dt);
void MotionLane_Stop(void);

uint8_t MotionLane_IsConfigured(void);
uint8_t MotionLane_IsBusy(void);
uint8_t MotionLane_IsFinished(void);
MotionLane_State_t MotionLane_GetState(void);
MotionLane_Error_t MotionLane_GetError(void);

/* 遥测：最近带偏差，千分比，车道中心偏右为正（已相对 LANE 帧翻过符号）。 */
float MotionLane_GetLaneError(void);
/* 遥测：本拍差速修正量，mm/s，正 = 右转。 */
float MotionLane_GetAdjustMMps(void);

#endif
