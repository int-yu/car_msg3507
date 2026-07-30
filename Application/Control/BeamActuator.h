#ifndef APPLICATION_CONTROL_BEAM_ACTUATOR_H
#define APPLICATION_CONTROL_BEAM_ACTUATOR_H

#include <stdint.h>

/*
 * 摆杆倾角执行层：把「倾角（度）」翻译成步进电机的绝对步坐标。
 *
 * 这是唯一知道执行器类型、传动比和零点的地方。BallBalance 全程只谈
 * 倾角，因此换执行器或改齿轮比都不需要重调控制增益。
 *
 * 开环说明：MT6816 绝对编码器占用的 PA25/PA14 已被六路红外拿走，
 * 反馈以 STEPPER_FEEDBACK_ENABLED=0 停用。步进本身是位置型执行器，
 * 真正的位置闭环由外层的钢球视觉负责，所以开环可用；代价是没有失步
 * 检测，靠下面的一致性检查兜底。
 */

/* 电机轴转一圈对应摆杆转多少圈的倒数：N>1 表示减速，摆杆比电机慢。
 * 实测传动比后经 Param 的 K 命令写入，不必重新烧录。 */
#define BEAM_ACTUATOR_GEAR_RATIO 1.0f

/* 摆杆机械软限位。与 BallBalance 的倾角上限一致，双重保护。 */
#define BEAM_ACTUATOR_MAX_TILT_DEG 6.0f

/*
 * 倾角命令的最大变化率（度/秒）。这个值只为防止步进丢步，不是为了
 * 平滑：设得过小会让外环爬不过齿轮回差和静摩擦死区，钢球会长时间
 * 不响应。回差实测偏大时应优先加大它而不是减小。
 */
#define BEAM_ACTUATOR_MAX_RATE_DEG_PER_S 240.0f

/* 运行时可调：实测传动比与零点偏置后写入。 */
extern float BeamActuator_TuneGearRatio;
extern float BeamActuator_TuneZeroOffsetDeg;

void BeamActuator_Init(void);

/* 设置摆杆目标倾角。内部做软限位和斜率限制，再换算成步坐标下发。 */
void BeamActuator_SetTiltDeg(float tiltDeg);

/* 每个控制拍调用一次，推进斜率限制。 */
void BeamActuator_Update(float dt);

/* 最近一次实际下发的倾角（经限幅和限斜率之后）。 */
float BeamActuator_GetTiltDeg(void);

/* 请求的倾角（限幅前）。与 GetTiltDeg 差得多说明正被斜率限制卡住。 */
float BeamActuator_GetRequestedTiltDeg(void);

#endif
