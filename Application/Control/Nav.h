#ifndef APPLICATION_CONTROL_NAV_H
#define APPLICATION_CONTROL_NAV_H

#include <stdint.h>

/* Nav 的两种转向机构。 */
typedef enum
{
    NAV_MODE_PIVOT = 0, /* 单轮保持不动，另一侧车轮向前转动。 */
    NAV_MODE_SPIN       /* 两轮等速反向，车体中心基本保持不动。 */
} Nav_Mode_t;

/* 转向调参；当前默认值必须经过实车验证后再用于比赛任务。 */
typedef struct
{
    float maximumTurnSpeedMMps; /* 转向接口允许请求的最大轮速。 */
    float minimumTurnSpeedMMps; /* 接近目标角时克服静摩擦的最低轮速。 */
    float slowdownAngleDeg;     /* 剩余角小于该值时开始按比例降低轮速。 */
    float accelerationMMps2;    /* 转向轮速上升斜率。 */
    float decelerationMMps2;    /* 转向轮速下降斜率。 */
    float angleToleranceDeg;    /* 判断到达目标角度的允许误差。 */
    uint8_t settleTicks;        /* 连续稳定在误差范围内的 100 Hz 周期数。 */
    int8_t rotationCommandSign; /* 角度正方向与车轮指令映射，只能为 1 或 -1。 */
} Nav_Config_t;

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

Nav_Result_t Nav_Init(const Nav_Config_t *config);
Nav_Result_t Nav_InitDefault(void);

/* To：目标为 Heading 连续累计角中的绝对角度，不做 ±180° 归一化。 */
Nav_Result_t Nav_StartPivotTo(float targetYawDeg, float speedMMps);
Nav_Result_t Nav_StartSpinTo(float targetYawDeg, float speedMMps);

/* By：目标为相对当前角度再旋转 deltaYawDeg，可填写正数或负数。 */
Nav_Result_t Nav_StartPivotBy(float deltaYawDeg, float speedMMps);
Nav_Result_t Nav_StartSpinBy(float deltaYawDeg, float speedMMps);

void Nav_Update(float dt);
void Nav_Stop(void);

uint8_t Nav_IsConfigured(void);
uint8_t Nav_IsBusy(void);
uint8_t Nav_IsFinished(void);
Nav_State_t Nav_GetState(void);
Nav_Error_t Nav_GetError(void);
Nav_Mode_t Nav_GetMode(void);
float Nav_GetTargetYawDeg(void);
float Nav_GetAngleErrorDeg(void);

#endif
