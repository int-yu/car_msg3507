#ifndef APPLICATION_CONTROL_NAV_H
#define APPLICATION_CONTROL_NAV_H

#include <stdint.h>

/* 双轮反向转向参数：若角度误差持续增大，只翻转方向符号。 */
#define NAV_MAX_TURN_SPEED_MMPS   200.0f /* 每侧轮允许的最高转向速度。 */
#define NAV_MIN_TURN_SPEED_MMPS   40.0f  /* 接近目标角时的最低轮速。 */
#define NAV_SLOWDOWN_ANGLE_DEG    45.0f  /* 进入低速区的剩余角度。 */
#define NAV_ACCELERATION_MMPS2    150.0f /* 起转轮速上升斜率。 */
#define NAV_DECELERATION_MMPS2    600.0f /* 接近目标时的轮速下降斜率。 */
#define NAV_ANGLE_TOLERANCE_DEG   2.0f   /* 到达目标角的允许误差。 */
#define NAV_SETTLE_TICKS          3U     /* 连续稳定 30 ms 后完成。 */
#define NAV_ROTATION_COMMAND_SIGN 1      /* 误差增大时翻转为 -1。 */

/* 运行时可调参数：上电恢复上方 #define 默认值，由 K 命令经 Param 模块读写。
 * 每拍直接读取变量，写入即生效。调好后把数值写回 #define 固化。 */
extern float Nav_TuneMaxTurnSpeedMMps;
extern float Nav_TuneMinTurnSpeedMMps;
extern float Nav_TuneSlowdownAngleDeg;
extern float Nav_TuneAngleToleranceDeg;

typedef enum
{
    NAV_STATE_IDLE = 0,
    NAV_STATE_RUNNING,
    NAV_STATE_COMPLETED,
    NAV_STATE_ERROR
} Nav_State_t;

typedef enum
{
    NAV_ERROR_NONE = 0,
    NAV_ERROR_HEADING_OFFLINE,
    NAV_ERROR_UPDATE_PERIOD_INVALID,
    NAV_ERROR_WHEEL
} Nav_Error_t;

typedef enum
{
    NAV_RESULT_OK = 0,
    NAV_RESULT_BUSY,
    NAV_RESULT_INVALID_ARGUMENT,
    NAV_RESULT_NOT_CONFIGURED,
    NAV_RESULT_SENSOR_NOT_READY
} Nav_Result_t;

Nav_Result_t Nav_Init(void);

/* To：转到 Heading 连续累计角中的绝对角度，不做 ±180° 归一化。 */
Nav_Result_t Nav_StartTo(float targetYawDeg, float speedMMps);

/* By：从当前角度再旋转 deltaYawDeg，可填写正数、负数或多圈角度。 */
Nav_Result_t Nav_StartBy(float deltaYawDeg, float speedMMps);

void Nav_Update(float dt);
void Nav_Stop(void);

uint8_t Nav_IsConfigured(void);
uint8_t Nav_IsBusy(void);
uint8_t Nav_IsFinished(void);
Nav_State_t Nav_GetState(void);
Nav_Error_t Nav_GetError(void);
float Nav_GetTargetYawDeg(void);
float Nav_GetAngleErrorDeg(void);

#endif
