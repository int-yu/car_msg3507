#ifndef ACCOMPLISH_26H_H
#define ACCOMPLISH_26H_H

#include "Application/Core/App.h"
#include <stdint.h>

#define ACCOMPLISH_26H_START_STOP_KEY_MASK 0x01U
#define ACCOMPLISH_26H_EMERGENCY_STOP_KEY_MASK (0x01U | 0x02U)

/* H 题要求 2：A 点起跑，沿黑线顺时针一圈后回 A 点软停。
 * 赛道理论周长为 2*1500 + 2*pi*500 约 6142 mm。下面仅是首轮软件值，
 * 传感器到题目判定点的偏移和轮速/里程必须按实车标定。 */
#define ACCOMPLISH_26H_CRUISE_SPEED_MMPS             450.0f
#define ACCOMPLISH_26H_FINISH_CRAWL_SPEED_MMPS       160.0f
#define ACCOMPLISH_26H_NOMINAL_LAP_DISTANCE_MM      6142.0f
#define ACCOMPLISH_26H_FINISH_APPROACH_DISTANCE_MM   360.0f
#define ACCOMPLISH_26H_FINISH_MARKER_ARM_DISTANCE_MM 6000.0f
#define ACCOMPLISH_26H_MAX_LAP_DISTANCE_MM          6700.0f
#define ACCOMPLISH_26H_START_CLEAR_DISTANCE_MM         80.0f
#define ACCOMPLISH_26H_MARKER_MIN_ACTIVE_CHANNELS       6U
#define ACCOMPLISH_26H_MARKER_CLEAR_CONFIRM_TICKS        3U
#define ACCOMPLISH_26H_MARKER_CONFIRM_TICKS              3U
#define ACCOMPLISH_26H_FINISH_ROLLOUT_MM                0.0f
#define ACCOMPLISH_26H_SETTLE_SPEED_MMPS              20.0f
#define ACCOMPLISH_26H_SETTLE_CONFIRM_TICKS            10U
#define ACCOMPLISH_26H_SETTLE_TIMEOUT_TICKS           200U
#define ACCOMPLISH_26H_MAX_RUN_TICKS                  2000U

/* 红外横线被首次确认到后，保持巡线的额外行程。正值会让车再前进；
 * 它用于抵消“红外条 -> 题设车身测试点”的距离和软停滑行量，现场可用
 * K 命令临时修改，掉电恢复为上面的默认值。 */
extern float Accomplish26H_TuneFinishRolloutMM;

typedef enum
{
    ACCOMPLISH_26H_STATE_READY = 0,
    ACCOMPLISH_26H_STATE_LEAVING_START,
    ACCOMPLISH_26H_STATE_RUNNING,
    ACCOMPLISH_26H_STATE_FINISH_ROLLOUT,
    ACCOMPLISH_26H_STATE_SOFT_STOP,
    ACCOMPLISH_26H_STATE_SETTLING,
    ACCOMPLISH_26H_STATE_FINISHED,
    ACCOMPLISH_26H_STATE_ERROR
} Accomplish26H_State_t;

typedef enum
{
    ACCOMPLISH_26H_ERROR_NONE = 0,
    ACCOMPLISH_26H_ERROR_START,
    ACCOMPLISH_26H_ERROR_SENSOR_OFFLINE,
    ACCOMPLISH_26H_ERROR_MOTION,
    ACCOMPLISH_26H_ERROR_MARKER_MISSED,
    ACCOMPLISH_26H_ERROR_SETTLE_TIMEOUT,
    ACCOMPLISH_26H_ERROR_TIME_LIMIT,
    ACCOMPLISH_26H_ERROR_EMERGENCY_STOP
} Accomplish26H_Error_t;

void Accomplish26H_Init(void);
void Accomplish26H_Update(const App_UpdateContext_t *context);
uint8_t Accomplish26H_IsTiming(void);
uint32_t Accomplish26H_GetElapsedTicks(void);
Accomplish26H_State_t Accomplish26H_GetState(void);
Accomplish26H_Error_t Accomplish26H_GetError(void);

#endif
