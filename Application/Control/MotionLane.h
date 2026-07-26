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
#define MOTION_LANE_LOST_HOLD_TICKS     30U     /* 100 Hz 下丢失保持 300 ms。 */
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
