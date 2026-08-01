#ifndef APPLICATION_CONTROL_TIMED_LINE_RUN_H
#define APPLICATION_CONTROL_TIMED_LINE_RUN_H

#include "Application/Core/App.h"
#include <stdint.h>

/* KEY3 的默认参数集中放在这里，并可通过 K 参数在线修改。 */
#define TIMED_LINE_RUN_ACCELERATION_MMPS2       100.0f
#define TIMED_LINE_RUN_CRUISE_SPEED_MMPS        400.0f
#define TIMED_LINE_RUN_DURATION_SECONDS          30.0f
#define TIMED_LINE_RUN_SETTLE_SPEED_MMPS         24.0f
#define TIMED_LINE_RUN_SETTLE_CONFIRM_TICKS      10U
#define TIMED_LINE_RUN_SETTLE_TIMEOUT_TICKS     200U
#define TIMED_LINE_RUN_EMERGENCY_KEY_MASK (0x01U | 0x02U)

extern float TimedLineRun_TuneAccelerationMMps2;
extern float TimedLineRun_TuneCruiseSpeedMMps;
extern float TimedLineRun_TuneDurationSeconds;

typedef enum
{
    TIMED_LINE_RUN_STATE_READY = 0,
    TIMED_LINE_RUN_STATE_RUNNING,
    TIMED_LINE_RUN_STATE_SOFT_STOP,
    TIMED_LINE_RUN_STATE_SETTLING,
    TIMED_LINE_RUN_STATE_FINISHED,
    TIMED_LINE_RUN_STATE_ERROR
} TimedLineRun_State_t;

typedef enum
{
    TIMED_LINE_RUN_ERROR_NONE = 0,
    TIMED_LINE_RUN_ERROR_START,
    TIMED_LINE_RUN_ERROR_SENSOR_OFFLINE,
    TIMED_LINE_RUN_ERROR_MOTION,
    TIMED_LINE_RUN_ERROR_SETTLE_TIMEOUT,
    TIMED_LINE_RUN_ERROR_EMERGENCY_STOP
} TimedLineRun_Error_t;

void TimedLineRun_Init(void);
void TimedLineRun_Cancel(void);
uint8_t TimedLineRun_Start(void);
void TimedLineRun_Update(const App_UpdateContext_t *context);
uint8_t TimedLineRun_IsActive(void);
uint8_t TimedLineRun_CanStart(void);
uint32_t TimedLineRun_GetElapsedTicks(void);
TimedLineRun_State_t TimedLineRun_GetState(void);
TimedLineRun_Error_t TimedLineRun_GetError(void);

#endif
