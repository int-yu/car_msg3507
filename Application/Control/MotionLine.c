#include "Application/Control/MotionLine.h"
#include "Application/Control/MotionLineConfig.h"
#include "Application/Control/MotionWheel.h"
#include "Application/Control/PID.h"
#include "Hardware/Sensors/Graydetect.h"
#include <math.h>
#include <stddef.h>

/* 巡线模块运行状态集中保存，避免状态变量散落在文件各处。 */
typedef struct
{
    MotionLine_State_t state;
    MotionLine_Error_t error;
    float cruiseSpeedMMps;
    float lineError;
    uint8_t configured;
} MotionLine_Context_t;

static MotionLine_Config_t s_config;
static PID_t s_linePID;
static MotionLine_Context_t s_context = {
    .state = MOTION_LINE_STATE_IDLE,
    .error = MOTION_LINE_ERROR_NONE,
};

/* 检查所有公共调参，防止非法参数直接传入电机控制层。 */
static uint8_t MotionLine_ConfigIsValid(const MotionLine_Config_t *config)
{
    if (config == NULL)
    {
        return 0U;
    }
    if ((!isfinite(config->kp)) || (!isfinite(config->kd)) ||
        (!isfinite(config->correctionLimitPWM)) ||
        (!isfinite(config->maximumSpeedMMps)))
    {
        return 0U;
    }
    if ((config->kp < 0.0f) || (config->kd < 0.0f) ||
        ((config->kp == 0.0f) && (config->kd == 0.0f)) ||
        (config->correctionLimitPWM <= 0.0f) ||
        (config->correctionLimitPWM >
         MotionWheel_GetMaximumCommandPWM()) ||
        (config->maximumSpeedMMps <= 0.0f) ||
        ((config->correctionSign != 1) &&
         (config->correctionSign != -1)))
    {
        return 0U;
    }
    return 1U;
}

static void MotionLine_ResetControl(void)
{
    PID_Reset(&s_linePID);
    s_context.cruiseSpeedMMps = 0.0f;
    s_context.lineError = 0.0f;
}

static void MotionLine_SetError(MotionLine_Error_t error)
{
    MotionWheel_Stop();
    MotionLine_ResetControl();
    s_context.error = error;
    s_context.state = MOTION_LINE_STATE_ERROR;
}

/* 读取五路灰度并计算巡线差速修正；返回 0 表示已经丢线。 */
static uint8_t MotionLine_CalculateCorrection(float dt, float *correctionPWM)
{
    if (Graydetect_GetState() == 0U)
    {
        return 0U;
    }

    s_context.lineError = Graydetect_GetError(GRAY_SIDE_ALL);
    *correctionPWM = PID_Update(&s_linePID, 0.0f,
                                s_context.lineError, dt);
    *correctionPWM *= (float)s_config.correctionSign;
    return 1U;
}

static MotionWheel_Result_t MotionLine_ApplyWheelCommand(
    float correctionPWM, float dt)
{
    MotionWheel_Command_t command;

    command.targetSpeedLMMps = s_context.cruiseSpeedMMps;
    command.targetSpeedRMMps = s_context.cruiseSpeedMMps;
    command.trimLPWM = -correctionPWM;
    command.trimRPWM = correctionPWM;
    return MotionWheel_Update(&command, dt);
}

MotionLine_Result_t MotionLine_Init(const MotionLine_Config_t *config)
{
    MotionWheel_Result_t wheelResult;

    s_context.configured = 0U;
    s_context.state = MOTION_LINE_STATE_IDLE;
    s_context.error = MOTION_LINE_ERROR_NONE;
    MotionLine_ResetControl();

    wheelResult = MotionWheel_InitDefault();
    if ((wheelResult != MOTION_WHEEL_RESULT_OK) ||
        (MotionLine_ConfigIsValid(config) == 0U))
    {
        return MOTION_LINE_RESULT_INVALID_ARGUMENT;
    }

    s_config = *config;
    PID_Init(&s_linePID, s_config.kp, 0.0f, s_config.kd,
             s_config.correctionLimitPWM, 0.0f);
    MotionLine_ResetControl();
    s_context.configured = 1U;
    return MOTION_LINE_RESULT_OK;
}

MotionLine_Result_t MotionLine_InitDefault(void)
{
    return MotionLine_Init(&g_motionLineConfig);
}

MotionLine_Result_t MotionLine_Start(float speedMMps)
{
    if (s_context.configured == 0U)
    {
        return MOTION_LINE_RESULT_NOT_CONFIGURED;
    }
    if (MotionLine_IsBusy() != 0U)
    {
        return MOTION_LINE_RESULT_BUSY;
    }
    if ((!isfinite(speedMMps)) || (speedMMps <= 0.0f))
    {
        return MOTION_LINE_RESULT_INVALID_ARGUMENT;
    }

    MotionWheel_Stop();
    MotionLine_ResetControl();
    s_context.cruiseSpeedMMps =
        (speedMMps > s_config.maximumSpeedMMps) ?
            s_config.maximumSpeedMMps : speedMMps;
    s_context.error = MOTION_LINE_ERROR_NONE;
    s_context.state = MOTION_LINE_STATE_RUNNING;
    return MOTION_LINE_RESULT_OK;
}

void MotionLine_Update(float dt)
{
    float correctionPWM;

    if (s_context.state != MOTION_LINE_STATE_RUNNING)
    {
        return;
    }
    if ((!isfinite(dt)) || (dt <= 0.0f))
    {
        MotionLine_SetError(MOTION_LINE_ERROR_UPDATE_PERIOD_INVALID);
        return;
    }
    if (MotionLine_CalculateCorrection(dt, &correctionPWM) == 0U)
    {
        MotionLine_SetError(MOTION_LINE_ERROR_LINE_LOST);
        return;
    }
    if (MotionLine_ApplyWheelCommand(correctionPWM, dt) !=
        MOTION_WHEEL_RESULT_OK)
    {
        MotionLine_SetError(MOTION_LINE_ERROR_WHEEL);
    }
}

void MotionLine_Stop(void)
{
    MotionWheel_Stop();
    MotionLine_ResetControl();
    s_context.error = MOTION_LINE_ERROR_NONE;
    s_context.state = MOTION_LINE_STATE_IDLE;
}

uint8_t MotionLine_IsConfigured(void)
{
    return s_context.configured;
}

uint8_t MotionLine_IsBusy(void)
{
    return (s_context.state == MOTION_LINE_STATE_RUNNING) ? 1U : 0U;
}

MotionLine_State_t MotionLine_GetState(void)
{
    return s_context.state;
}

MotionLine_Error_t MotionLine_GetError(void)
{
    return s_context.error;
}

float MotionLine_GetLineError(void)
{
    return s_context.lineError;
}
