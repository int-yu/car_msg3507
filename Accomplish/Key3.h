#ifndef ACCOMPLISH_KEY3_H
#define ACCOMPLISH_KEY3_H

#include "Application/Core/App.h"
#include <stdint.h>

/* Key3: 球平衡稳定性调试任务
 * 按KEY3启动，沿黑线巡航指定时长后软停，用于调试球体平衡控制器。
 * 重点：加速度前馈、速度阈值、PID参数等可在运行中通过串口调整。 */

#define ACCOMPLISH_KEY3_START_KEY_MASK 0x04U  /* KEY3 = bit2 */
#define ACCOMPLISH_KEY3_EMERGENCY_STOP_KEY_MASK (0x04U | 0x02U)

/* 默认运动参数 */
#define ACCOMPLISH_KEY3_ACCELERATION_MMPS2    100.0f
#define ACCOMPLISH_KEY3_CRUISE_SPEED_MMPS     400.0f
#define ACCOMPLISH_KEY3_RUN_DURATION_TICKS    3000U  /* 30秒 @ 100Hz */

/* 安全超时 */
#define ACCOMPLISH_KEY3_MAX_RUN_TICKS         6000U  /* 60秒 */

/* 软停后等待稳定 */
#define ACCOMPLISH_KEY3_SETTLE_SPEED_MMPS     24.0f
#define ACCOMPLISH_KEY3_SETTLE_CONFIRM_TICKS  10U
#define ACCOMPLISH_KEY3_SETTLE_TIMEOUT_TICKS  200U

/* 现场调参入口，掉电后恢复为上面的默认值 */
extern float AccomplishKey3_TuneAccelerationMMps2;
extern float AccomplishKey3_TuneCruiseSpeedMMps;
extern uint32_t AccomplishKey3_TuneRunDurationTicks;

typedef enum
{
    ACCOMPLISH_KEY3_STATE_READY = 0,
    ACCOMPLISH_KEY3_STATE_RUNNING,
    ACCOMPLISH_KEY3_STATE_SOFT_STOP,
    ACCOMPLISH_KEY3_STATE_SETTLING,
    ACCOMPLISH_KEY3_STATE_FINISHED,
    ACCOMPLISH_KEY3_STATE_ERROR
} AccomplishKey3_State_t;

typedef enum
{
    ACCOMPLISH_KEY3_ERROR_NONE = 0,
    ACCOMPLISH_KEY3_ERROR_START,
    ACCOMPLISH_KEY3_ERROR_SENSOR_OFFLINE,
    ACCOMPLISH_KEY3_ERROR_MOTION,
    ACCOMPLISH_KEY3_ERROR_TIMEOUT
} AccomplishKey3_Error_t;

void AccomplishKey3_Init(void);
void AccomplishKey3_Update(uint8_t keys);
AccomplishKey3_State_t AccomplishKey3_GetState(void);
AccomplishKey3_Error_t AccomplishKey3_GetError(void);
uint32_t AccomplishKey3_GetElapsedTicks(void);

#endif
