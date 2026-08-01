#include "Accomplish/26H.h"
#include "Application/Control/MotionManager.h"
#include "Application/Control/TaskTimer.h"
#include "Application/State/Odometry.h"
#include "Hardware/Sensors/Graydetect.h"
#include <limits.h>
#include <math.h>
#include <stddef.h>

typedef struct
{
    float cruiseSpeedMMps;
    float finishCrawlSpeedMMps;
    float startClearDistanceMM;
    float nominalLapDistanceMM;
    float finishApproachDistanceMM;
    float finishMarkerArmDistanceMM;
    float maxLapDistanceMM;
    float finishRolloutMM;
} Accomplish26H_RunParameters_t;

static uint32_t s_elapsedTicks;
static uint8_t s_timing;
static Accomplish26H_State_t s_state;
static Accomplish26H_Error_t s_error;
static float s_startDistanceMM;
static float s_markerDistanceMM;
static uint16_t s_startClearTicks;
static uint8_t s_markerConfirmTicks;
static uint16_t s_settleTicks;
static uint16_t s_settleElapsedTicks;
static Accomplish26H_RunParameters_t s_runParameters;

float Accomplish26H_TuneCruiseSpeedMMps =
    ACCOMPLISH_26H_CRUISE_SPEED_MMPS;
float Accomplish26H_TuneFinishCrawlSpeedMMps =
    ACCOMPLISH_26H_FINISH_CRAWL_SPEED_MMPS;
float Accomplish26H_TuneStartClearDistanceMM =
    ACCOMPLISH_26H_START_CLEAR_DISTANCE_MM;
float Accomplish26H_TuneNominalLapDistanceMM =
    ACCOMPLISH_26H_NOMINAL_LAP_DISTANCE_MM;
float Accomplish26H_TuneFinishApproachDistanceMM =
    ACCOMPLISH_26H_FINISH_APPROACH_DISTANCE_MM;
float Accomplish26H_TuneFinishMarkerArmDistanceMM =
    ACCOMPLISH_26H_FINISH_MARKER_ARM_DISTANCE_MM;
float Accomplish26H_TuneMaxLapDistanceMM =
    ACCOMPLISH_26H_MAX_LAP_DISTANCE_MM;
float Accomplish26H_TuneFinishRolloutMM =
    ACCOMPLISH_26H_FINISH_ROLLOUT_MM;

static uint8_t Accomplish26H_SnapshotParameters(void)
{
    Accomplish26H_RunParameters_t parameters = {
        .cruiseSpeedMMps = Accomplish26H_TuneCruiseSpeedMMps,
        .finishCrawlSpeedMMps = Accomplish26H_TuneFinishCrawlSpeedMMps,
        .startClearDistanceMM = Accomplish26H_TuneStartClearDistanceMM,
        .nominalLapDistanceMM = Accomplish26H_TuneNominalLapDistanceMM,
        .finishApproachDistanceMM =
            Accomplish26H_TuneFinishApproachDistanceMM,
        .finishMarkerArmDistanceMM =
            Accomplish26H_TuneFinishMarkerArmDistanceMM,
        .maxLapDistanceMM = Accomplish26H_TuneMaxLapDistanceMM,
        .finishRolloutMM = Accomplish26H_TuneFinishRolloutMM,
    };

    if ((!isfinite(parameters.cruiseSpeedMMps)) ||
        (!isfinite(parameters.finishCrawlSpeedMMps)) ||
        (!isfinite(parameters.startClearDistanceMM)) ||
        (!isfinite(parameters.nominalLapDistanceMM)) ||
        (!isfinite(parameters.finishApproachDistanceMM)) ||
        (!isfinite(parameters.finishMarkerArmDistanceMM)) ||
        (!isfinite(parameters.maxLapDistanceMM)) ||
        (!isfinite(parameters.finishRolloutMM)))
    {
        return 0U;
    }

    if ((parameters.cruiseSpeedMMps <= 0.0f) ||
        (parameters.finishCrawlSpeedMMps <= 0.0f) ||
        (parameters.finishCrawlSpeedMMps > parameters.cruiseSpeedMMps) ||
        (parameters.startClearDistanceMM < 0.0f) ||
        (parameters.nominalLapDistanceMM <= 0.0f) ||
        (parameters.finishApproachDistanceMM < 0.0f) ||
        (parameters.finishApproachDistanceMM >
         parameters.nominalLapDistanceMM) ||
        (parameters.finishMarkerArmDistanceMM <
         parameters.startClearDistanceMM) ||
        (parameters.maxLapDistanceMM < parameters.nominalLapDistanceMM) ||
        (parameters.finishMarkerArmDistanceMM >
         parameters.maxLapDistanceMM) ||
        (parameters.finishRolloutMM < 0.0f))
    {
        return 0U;
    }

    s_runParameters = parameters;
    return 1U;
}

static void Accomplish26H_AddElapsedTicks(uint8_t elapsedTicks)
{
    if ((UINT32_MAX - s_elapsedTicks) < elapsedTicks)
    {
        s_elapsedTicks = UINT32_MAX;
    }
    else
    {
        s_elapsedTicks += elapsedTicks;
    }
}

static uint16_t Accomplish26H_AddSaturatingTicks(
    uint16_t current, uint8_t elapsedTicks)
{
    if (((uint32_t)current + (uint32_t)elapsedTicks) > UINT16_MAX)
    {
        return UINT16_MAX;
    }
    return (uint16_t)(current + elapsedTicks);
}

static uint8_t Accomplish26H_CountActiveChannels(void)
{
    uint8_t state = Graydetect_GetState();
    uint8_t index;
    uint8_t count = 0U;

    for (index = 0U; index < GRAY_CHANNEL_COUNT; index++)
    {
        if ((state & (uint8_t)(1U << index)) != 0U)
        {
            count++;
        }
    }
    return count;
}

static uint8_t Accomplish26H_IsStartFinishMarker(void)
{
    return (Accomplish26H_CountActiveChannels() >=
            ACCOMPLISH_26H_MARKER_MIN_ACTIVE_CHANNELS) ? 1U : 0U;
}

static float Accomplish26H_GetTravelledDistanceMM(void)
{
    return fabsf(Odometry_GetDistanceMM() - s_startDistanceMM);
}

static void Accomplish26H_Fail(Accomplish26H_Error_t error)
{
    /* 传感器/控制故障时以安全停车优先；正常终点不走这里。 */
    MotionManager_Stop();
    TaskTimer_Stop(TASK_TIMER_OWNER_LINE);
    s_timing = 0U;
    s_error = error;
    s_state = ACCOMPLISH_26H_STATE_ERROR;
}

static uint8_t Accomplish26H_LineIsHealthy(void)
{
    if (Graydetect_IsOnline() == 0U)
    {
        Accomplish26H_Fail(ACCOMPLISH_26H_ERROR_SENSOR_OFFLINE);
        return 0U;
    }
    if ((MotionManager_GetError() != MOTION_MANAGER_ERROR_NONE) ||
        (MotionManager_GetMode() != MOTION_MANAGER_MODE_LINE) ||
        (MotionManager_IsBusy() == 0U))
    {
        Accomplish26H_Fail(ACCOMPLISH_26H_ERROR_MOTION);
        return 0U;
    }
    return 1U;
}

static void Accomplish26H_BeginSoftStop(
    Accomplish26H_Error_t finishError)
{
    TaskTimer_Stop(TASK_TIMER_OWNER_LINE);
    if (MotionManager_RequestLineStop() != MOTION_MANAGER_RESULT_OK)
    {
        Accomplish26H_Fail(ACCOMPLISH_26H_ERROR_MOTION);
        return;
    }

    s_error = finishError;
    s_state = ACCOMPLISH_26H_STATE_SOFT_STOP;
}

static void Accomplish26H_BeginBrakeStop(
    Accomplish26H_Error_t finishError)
{
    TaskTimer_Stop(TASK_TIMER_OWNER_LINE);
    if (MotionManager_StartBrake() != MOTION_MANAGER_RESULT_OK)
    {
        Accomplish26H_Fail(ACCOMPLISH_26H_ERROR_MOTION);
        return;
    }

    s_error = finishError;
    s_state = ACCOMPLISH_26H_STATE_SOFT_STOP;
}

static void Accomplish26H_Start(void)
{
    if (Graydetect_IsOnline() == 0U)
    {
        MotionManager_Stop();
        s_error = ACCOMPLISH_26H_ERROR_SENSOR_OFFLINE;
        s_state = ACCOMPLISH_26H_STATE_ERROR;
        return;
    }
    if (Accomplish26H_SnapshotParameters() == 0U)
    {
        MotionManager_Stop();
        s_error = ACCOMPLISH_26H_ERROR_START;
        s_state = ACCOMPLISH_26H_STATE_ERROR;
        return;
    }
    if (MotionManager_StartLine(s_runParameters.cruiseSpeedMMps) !=
        MOTION_MANAGER_RESULT_OK)
    {
        MotionManager_Stop();
        s_error = ACCOMPLISH_26H_ERROR_START;
        s_state = ACCOMPLISH_26H_STATE_ERROR;
        return;
    }

    s_elapsedTicks = 0U;
    s_timing = 1U;
    TaskTimer_Start(TASK_TIMER_OWNER_LINE);
    s_error = ACCOMPLISH_26H_ERROR_NONE;
    s_startDistanceMM = Odometry_GetDistanceMM();
    s_markerDistanceMM = 0.0f;
    s_startClearTicks = 0U;
    s_markerConfirmTicks = 0U;
    s_settleTicks = 0U;
    s_settleElapsedTicks = 0U;
    s_state = ACCOMPLISH_26H_STATE_LEAVING_START;
}

static void Accomplish26H_UpdateLeavingStart(uint8_t elapsedTicks)
{
    float travelledDistanceMM;

    if (Accomplish26H_LineIsHealthy() == 0U)
    {
        return;
    }

    travelledDistanceMM = Accomplish26H_GetTravelledDistanceMM();
    if (Accomplish26H_IsStartFinishMarker() == 0U)
    {
        s_startClearTicks = Accomplish26H_AddSaturatingTicks(
            s_startClearTicks, elapsedTicks);
    }
    else
    {
        s_startClearTicks = 0U;
    }

    /* 先离开 A 的横向启停线且走出一小段，再允许它成为终点候选。 */
    if ((travelledDistanceMM >= s_runParameters.startClearDistanceMM) &&
        (s_startClearTicks >= ACCOMPLISH_26H_MARKER_CLEAR_CONFIRM_TICKS))
    {
        s_state = ACCOMPLISH_26H_STATE_RUNNING;
    }
}

static void Accomplish26H_UpdateRunning(void)
{
    float travelledDistanceMM;
    float finishApproachStartMM;

    if (Accomplish26H_LineIsHealthy() == 0U)
    {
        return;
    }

    travelledDistanceMM = Accomplish26H_GetTravelledDistanceMM();
    finishApproachStartMM = s_runParameters.nominalLapDistanceMM -
                            s_runParameters.finishApproachDistanceMM;
    if ((finishApproachStartMM < 0.0f) ||
        (travelledDistanceMM >= finishApproachStartMM))
    {
        if (MotionManager_SetLineSpeed(
                s_runParameters.finishCrawlSpeedMMps) !=
            MOTION_MANAGER_RESULT_OK)
        {
            Accomplish26H_Fail(ACCOMPLISH_26H_ERROR_MOTION);
            return;
        }
    }

    /* 只在已离开 A 且接近一圈后识别横向启停线，防止刚起跑就停。 */
    if (travelledDistanceMM >
        s_runParameters.finishMarkerArmDistanceMM)
    {
        if (Accomplish26H_IsStartFinishMarker() != 0U)
        {
            if (s_markerConfirmTicks == 0U)
            {
                /* 锁存第一帧的里程，三帧确认不会额外引入停车偏移。 */
                s_markerDistanceMM = travelledDistanceMM;
            }
            if (s_markerConfirmTicks < UINT8_MAX)
            {
                s_markerConfirmTicks++;
            }
            if (s_markerConfirmTicks >= ACCOMPLISH_26H_MARKER_CONFIRM_TICKS)
            {
                Accomplish26H_BeginBrakeStop(ACCOMPLISH_26H_ERROR_NONE);
                return;
            }
        }
        else
        {
            s_markerConfirmTicks = 0U;
        }
    }

    /* 未找到 A 标志不能再盲目多跑一圈；同样用软停保护车辆。 */
    if (travelledDistanceMM >= s_runParameters.maxLapDistanceMM)
    {
        Accomplish26H_BeginSoftStop(
            ACCOMPLISH_26H_ERROR_MARKER_MISSED);
    }
}

static void Accomplish26H_UpdateFinishRollout(void)
{
    if (Accomplish26H_LineIsHealthy() == 0U)
    {
        return;
    }

    /* 该偏移须实测为“红外阵列 -> 题目停车判定点”的等效前进距离。 */
    if ((Accomplish26H_GetTravelledDistanceMM() - s_markerDistanceMM) >=
        s_runParameters.finishRolloutMM)
    {
        Accomplish26H_BeginSoftStop(ACCOMPLISH_26H_ERROR_NONE);
    }
}

static void Accomplish26H_UpdateSoftStop(void)
{
    if (MotionManager_GetError() != MOTION_MANAGER_ERROR_NONE)
    {
        Accomplish26H_Fail(ACCOMPLISH_26H_ERROR_MOTION);
        return;
    }
    if (MotionManager_IsFinished() != 0U)
    {
        s_settleTicks = 0U;
        s_settleElapsedTicks = 0U;
        s_state = ACCOMPLISH_26H_STATE_SETTLING;
        return;
    }
    if (MotionManager_IsBusy() == 0U)
    {
        Accomplish26H_Fail(ACCOMPLISH_26H_ERROR_MOTION);
    }
}

static void Accomplish26H_UpdateSettling(uint8_t elapsedTicks)
{
    float averageSpeedMMps =
        (fabsf(Odometry_GetSpeedL()) + fabsf(Odometry_GetSpeedR())) * 0.5f;

    if (averageSpeedMMps <= ACCOMPLISH_26H_SETTLE_SPEED_MMPS)
    {
        s_settleTicks = Accomplish26H_AddSaturatingTicks(
            s_settleTicks, elapsedTicks);
    }
    else
    {
        s_settleTicks = 0U;
    }

    s_settleElapsedTicks = Accomplish26H_AddSaturatingTicks(
        s_settleElapsedTicks, elapsedTicks);

    if (s_settleTicks >= ACCOMPLISH_26H_SETTLE_CONFIRM_TICKS)
    {
        s_timing = 0U;
        s_state = (s_error == ACCOMPLISH_26H_ERROR_NONE) ?
            ACCOMPLISH_26H_STATE_FINISHED : ACCOMPLISH_26H_STATE_ERROR;
    }
    else if (s_settleElapsedTicks >= ACCOMPLISH_26H_SETTLE_TIMEOUT_TICKS)
    {
        s_timing = 0U;
        s_error = ACCOMPLISH_26H_ERROR_SETTLE_TIMEOUT;
        s_state = ACCOMPLISH_26H_STATE_ERROR;
    }
}

void Accomplish26H_Init(void)
{
    s_elapsedTicks = 0U;
    s_timing = 0U;
    s_state = ACCOMPLISH_26H_STATE_READY;
    s_error = ACCOMPLISH_26H_ERROR_NONE;
    s_startDistanceMM = 0.0f;
    s_markerDistanceMM = 0.0f;
    s_startClearTicks = 0U;
    s_markerConfirmTicks = 0U;
    s_settleTicks = 0U;
    s_settleElapsedTicks = 0U;
}

void Accomplish26H_Cancel(void)
{
    MotionManager_Stop();
    TaskTimer_Stop(TASK_TIMER_OWNER_LINE);
    s_elapsedTicks = 0U;
    s_timing = 0U;
    s_state = ACCOMPLISH_26H_STATE_READY;
    s_error = ACCOMPLISH_26H_ERROR_NONE;
    s_startClearTicks = 0U;
    s_markerConfirmTicks = 0U;
    s_settleTicks = 0U;
    s_settleElapsedTicks = 0U;
}

void Accomplish26H_Update(const App_UpdateContext_t *context)
{
    uint8_t keyPressed;

    if (context == NULL)
    {
        return;
    }

    /* App 会先停车；这里同时冻结题目计时，禁止同拍 KEY1 重新起跑。 */
    if (((context->pressedKeys & ACCOMPLISH_26H_EMERGENCY_STOP_KEY_MASK) ==
         ACCOMPLISH_26H_EMERGENCY_STOP_KEY_MASK) ||
        ((context->hasBluetoothSignal != 0U) &&
         (context->bluetoothSignal == 0U)))
    {
        MotionManager_Stop();
        TaskTimer_Stop(TASK_TIMER_OWNER_LINE);
        s_timing = 0U;
        s_error = ACCOMPLISH_26H_ERROR_EMERGENCY_STOP;
        s_state = ACCOMPLISH_26H_STATE_ERROR;
        return;
    }

    keyPressed = ((context->pressedEdges &
                   ACCOMPLISH_26H_START_STOP_KEY_MASK) != 0U) ?
        1U : 0U;

    if ((s_state == ACCOMPLISH_26H_STATE_READY) ||
        (s_state == ACCOMPLISH_26H_STATE_FINISHED) ||
        (s_state == ACCOMPLISH_26H_STATE_ERROR))
    {
        if (keyPressed != 0U)
        {
            Accomplish26H_Start();
        }
        return;
    }

    if (s_timing != 0U)
    {
        Accomplish26H_AddElapsedTicks(context->elapsedTicks);
        if (s_elapsedTicks >= ACCOMPLISH_26H_MAX_RUN_TICKS)
        {
            /* 超出题目时间上限时不再继续盲跑；冻结显示并平滑停车为失败。 */
            s_timing = 0U;
            Accomplish26H_BeginSoftStop(ACCOMPLISH_26H_ERROR_TIME_LIMIT);
            return;
        }
    }

    switch (s_state)
    {
        case ACCOMPLISH_26H_STATE_LEAVING_START:
            Accomplish26H_UpdateLeavingStart(context->elapsedTicks);
            break;

        case ACCOMPLISH_26H_STATE_RUNNING:
            Accomplish26H_UpdateRunning();
            break;

        case ACCOMPLISH_26H_STATE_FINISH_ROLLOUT:
            Accomplish26H_UpdateFinishRollout();
            break;

        case ACCOMPLISH_26H_STATE_SOFT_STOP:
            Accomplish26H_UpdateSoftStop();
            break;

        case ACCOMPLISH_26H_STATE_SETTLING:
            Accomplish26H_UpdateSettling(context->elapsedTicks);
            break;

        case ACCOMPLISH_26H_STATE_READY:
        case ACCOMPLISH_26H_STATE_FINISHED:
        case ACCOMPLISH_26H_STATE_ERROR:
        default:
            break;
    }
}

uint8_t Accomplish26H_IsTiming(void)
{
    return s_timing;
}

uint32_t Accomplish26H_GetElapsedTicks(void)
{
    return s_elapsedTicks;
}

Accomplish26H_State_t Accomplish26H_GetState(void)
{
    return s_state;
}

Accomplish26H_Error_t Accomplish26H_GetError(void)
{
    return s_error;
}
