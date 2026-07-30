#ifndef APPLICATION_CONTROL_MOTION_LINE_H
#define APPLICATION_CONTROL_MOTION_LINE_H

#include <stdint.h>

/* 六路红外巡线参数：状态位为 1 表示检测到已学习的黑线目标色。 */
#define MOTION_LINE_OUTER_WEIGHT          6       /* 最外侧通道的权重绝对值。 */
#define MOTION_LINE_INNER_WEIGHT          2.5       /* 内侧通道的权重绝对值。 */
#define MOTION_LINE_MAX_ADJUST_RATIO      0.24f    /* 最外侧压线时，每侧增减当前速度的比例。 */
#define MOTION_LINE_MAX_SPEED_MMPS        1000.0f /* MotionLine_Start() 允许的巡线速度上限。 */
#define MOTION_LINE_ACCELERATION_MMPS2    300.0f  /* 启动/提速斜坡；避免目标速度突变。 */
#define MOTION_LINE_DECELERATION_MMPS2    360.0f  /* 降速/停车斜坡；低于急停的安全减速度。 */
#define MOTION_LINE_CURVE_MIN_SPEED_RATIO 0.68f   /* 最外侧压线时的最低巡线速度比例。 */
#define MOTION_LINE_WEIGHT_FILTER_ALPHA   0.25f   /* 六路离散位置的一阶低通系数（约 40 ms）。 */
#define MOTION_LINE_MAX_ADJUST_RATE_MMPS2 1200.0f /* 左右差速修正的最大变化率。 */
#define MOTION_LINE_LOST_CONFIRM_TICKS    8U      /* 连续全白 80 ms 后确认丢线。 */
#define MOTION_LINE_CURVE_TRIGGER_MASK    0x12U   /* CH2 or CH5 declares a curve candidate. */
#define MOTION_LINE_CURVE_ENTRY_CONFIRM_TICKS 3U  /* Consecutive IR samples for a candidate. */
#define MOTION_LINE_CURVE_SPEED_MMPS       380.0f /* 弧线低速上限，低于 26H 默认巡航速度。 */
#define MOTION_LINE_CURVE_HOLD_DISTANCE_MM 1700.0f /* 半圆弧长加停车裕量后的低速保持距离。 */

/* 可调参数：上电恢复默认值，由 K 命令经 Param 模块读写；MotionLine_Start()
 * 会一次性快照，运行中继续写入只影响下一次启动。TuneWeightKd 是权重变化率
 * 阻尼（mm/s 每 权重单位/s），弧线上左右摆动明显时可少量增加。 */
extern float MotionLine_TuneMaxAdjustRatio;
extern float MotionLine_TuneWeightKd;
extern float MotionLine_TuneCurveSpeedMMps;
extern float MotionLine_TuneCurveHoldDistanceMM;
extern float MotionLine_TuneAccelerationMMps2;
extern float MotionLine_TuneDecelerationMMps2;

typedef enum
{
    MOTION_LINE_PATH_STRAIGHT = 0,
    MOTION_LINE_PATH_CURVE
} MotionLine_PathState_t;

typedef enum
{
    MOTION_LINE_STATE_IDLE = 0,
    MOTION_LINE_STATE_RUNNING,
    MOTION_LINE_STATE_FINISHED,
    MOTION_LINE_STATE_ERROR
} MotionLine_State_t;

typedef enum
{
    MOTION_LINE_ERROR_NONE = 0,
    MOTION_LINE_ERROR_UPDATE_PERIOD_INVALID,
    MOTION_LINE_ERROR_WHEEL
} MotionLine_Error_t;

typedef enum
{
    MOTION_LINE_RESULT_OK = 0,
    MOTION_LINE_RESULT_BUSY,
    MOTION_LINE_RESULT_INVALID_ARGUMENT,
    MOTION_LINE_RESULT_NOT_CONFIGURED
} MotionLine_Result_t;

MotionLine_Result_t MotionLine_Init(void);
/* 持续巡线，直到调用 MotionLine_Stop() 或确认丢线。 */
MotionLine_Result_t MotionLine_Start(float speedMMps);
/* 运行中平滑修改基准速度；speed=0 会按减速度斜坡停车并进入 FINISHED。 */
MotionLine_Result_t MotionLine_SetSpeed(float speedMMps);
MotionLine_Result_t MotionLine_RequestStop(void);
void MotionLine_Update(float dt);
void MotionLine_Stop(void);

uint8_t MotionLine_IsConfigured(void);
uint8_t MotionLine_IsBusy(void);
uint8_t MotionLine_IsFinished(void);
MotionLine_State_t MotionLine_GetState(void);
MotionLine_Error_t MotionLine_GetError(void);
float MotionLine_GetLineError(void);
MotionLine_PathState_t MotionLine_GetPathState(void);
float MotionLine_GetRequestedSpeedMMps(void);
float MotionLine_GetProfileSpeedMMps(void);
/* 本拍速度斜坡的规划加速度，正为加速、负为减速。摆杆平衡用它做车体
 * 加速度前馈：钢球在非惯性系里受 a_car 的惯性力，需 θ_ff = a_car / k
 * 抵消。必须用规划量而不是编码器差分，后者的噪声会让摆杆抖动。 */
float MotionLine_GetProfileAccelerationMMps2(void);

#endif
