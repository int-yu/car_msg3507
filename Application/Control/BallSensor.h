#ifndef APPLICATION_CONTROL_BALL_SENSOR_H
#define APPLICATION_CONTROL_BALL_SENSOR_H

#include <stdint.h>

/*
 * 摆杆钢球位置观测：把 K230 的 TARGET 帧换算成以摆杆中心点 O 为原点的
 * 毫米位置，并估计滚动速度供平衡环的阻尼项使用。
 *
 * K230 BALL_POSITION maps the complete 250 mm pipe to -50.00..+50.00.
 * The link transmits that value multiplied by 100, so +/-5000 maps to the
 * tunable physical half length (125 mm by default).
 */

/* 摆杆中心到端部的距离；千分比换算的基准。 */
#define BALL_SENSOR_HALF_LENGTH_MM 125.0f

/* 超出这个范围判定为误检：钢球不可能跑到摆杆外面，读到就是视觉抖动或
 * 认错了目标，宁可报无效也不能把错误位置喂进闭环。 */
#define BALL_SENSOR_VALID_LIMIT_MM 130.0f

/*
 * 速度估计的一阶低通系数。相机约 25 fps 而控制环 100 Hz，裸差分会把
 * 采样抖动放大成尖峰并直接灌进 Kd，所以必须滤波。0.25 在 100 Hz 下
 * 约 40 ms 时间常数，与相机帧间隔相当。
 */
#define BALL_SENSOR_SPEED_FILTER_ALPHA 0.25f

/* 运行时可调：摄像头视场标定后写入，掉电恢复上面的默认值。 */
extern float BallSensor_TuneHalfLengthMM;

typedef enum
{
    BALL_SENSOR_SPEED_SOURCE_NONE = 0,
    BALL_SENSOR_SPEED_SOURCE_TI,
    BALL_SENSOR_SPEED_SOURCE_K230
} BallSensor_SpeedSource_t;

void BallSensor_Init(void);

/* 每个控制拍调用一次；内部只读 K230Link 的缓存，不触发串口收发。 */
void BallSensor_Update(float dt);

/*
 * 位置和速度只在 IsFresh() 为真时有意义。视觉失效时位置保持最后一次
 * 有效值、速度归零，但调用方必须靠 IsFresh() 判断，绝不能拿失效数据
 * 继续做闭环——钢球是双积分对象，冻结的位置会让倾角锁死。
 */
uint8_t BallSensor_IsFresh(void);
float BallSensor_GetPositionMM(void);
float BallSensor_GetSpeedMMps(void);
BallSensor_SpeedSource_t BallSensor_GetSpeedSource(void);
/* 当前有效视觉帧的序号；配合 IsFresh() 判断后使用。 */
uint8_t BallSensor_GetFrameSequence(void);

#endif
