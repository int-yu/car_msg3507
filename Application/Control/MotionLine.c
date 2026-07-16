#include "Application/Control/MotionLine.h"
#include "Application/Control/MotionLineConfig.h"
#include "Application/Control/PID.h"
#include "Application/Control/MotionWheel.h"
#include "Hardware/Sensors/Graydetect.h"
#include <math.h>
#include <stddef.h>

static MotionLine_Config_t s_config;
static PID_t s_linePID;
static MotionLine_State_t s_state = MOTION_LINE_STATE_IDLE;
static MotionLine_Error_t s_error = MOTION_LINE_ERROR_NONE;
static float s_cruiseSpeedMMps;
static float s_lineError;
static uint8_t s_configured;

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

static void MotionLine_SetError(MotionLine_Error_t error)
{
    MotionWheel_Stop();
    s_error = error;
    s_state = MOTION_LINE_STATE_ERROR;
}

MotionLine_Result_t MotionLine_Init(const MotionLine_Config_t *config)
{
    MotionWheel_Result_t wheelResult;

    s_configured = 0U;
    s_state = MOTION_LINE_STATE_IDLE;
    s_error = MOTION_LINE_ERROR_NONE;
    s_cruiseSpeedMMps = 0.0f;
    s_lineError = 0.0f;

    wheelResult = MotionWheel_InitDefault();
    if ((wheelResult != MOTION_WHEEL_RESULT_OK) ||
        (MotionLine_ConfigIsValid(config) == 0U))
    {
        return MOTION_LINE_RESULT_INVALID_ARGUMENT;
    }

    s_config = *config;
    PID_Init(&s_linePID, s_config.kp, 0.0f, s_config.kd,
             s_config.correctionLimitPWM, 0.0f);
    s_configured = 1U;
    return MOTION_LINE_RESULT_OK;
}

MotionLine_Result_t MotionLine_InitDefault(void)
{
    return MotionLine_Init(&g_motionLineConfig);
}

MotionLine_Result_t MotionLine_Start(float speedMMps)
{
    if (s_configured == 0U)
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

    s_cruiseSpeedMMps = (speedMMps > s_config.maximumSpeedMMps) ?
                            s_config.maximumSpeedMMps : speedMMps;
    s_lineError = 0.0f;
    s_error = MOTION_LINE_ERROR_NONE;
    PID_Reset(&s_linePID);
    MotionWheel_Stop();
    s_state = MOTION_LINE_STATE_RUNNING;
    return MOTION_LINE_RESULT_OK;
}

void MotionLine_Update(float dt)
{
    MotionWheel_Command_t wheelCommand;
    MotionWheel_Result_t wheelResult;
    float correctionPWM;
    uint8_t sensorState;

    if (s_state != MOTION_LINE_STATE_RUNNING)
    {
        return;
    }
    if ((!isfinite(dt)) || (dt <= 0.0f))
    {
        MotionLine_SetError(MOTION_LINE_ERROR_UPDATE_PERIOD_INVALID);
        return;
    }

    sensorState = Graydetect_GetState();
    if (sensorState == 0U)
    {
        MotionLine_SetError(MOTION_LINE_ERROR_LINE_LOST);
        return;
    }

    s_lineError = Graydetect_GetError(GRAY_SIDE_ALL);
    correctionPWM = PID_Update(&s_linePID, 0.0f, s_lineError, dt);
    correctionPWM *= (float)s_config.correctionSign;

    wheelCommand.targetSpeedLMMps = s_cruiseSpeedMMps;
    wheelCommand.targetSpeedRMMps = s_cruiseSpeedMMps;
    wheelCommand.trimLPWM = -correctionPWM;
    wheelCommand.trimRPWM = correctionPWM;
    wheelResult = MotionWheel_Update(&wheelCommand, dt);
    if (wheelResult != MOTION_WHEEL_RESULT_OK)
    {
        MotionLine_SetError(MOTION_LINE_ERROR_WHEEL);
    }
}

void MotionLine_Stop(void)
{
    MotionWheel_Stop();
    PID_Reset(&s_linePID);
    s_cruiseSpeedMMps = 0.0f;
    s_lineError = 0.0f;
    s_error = MOTION_LINE_ERROR_NONE;
    s_state = MOTION_LINE_STATE_IDLE;
}

uint8_t MotionLine_IsConfigured(void)
{
    return s_configured;
}

uint8_t MotionLine_IsBusy(void)
{
    return (s_state == MOTION_LINE_STATE_RUNNING) ? 1U : 0U;
}

MotionLine_State_t MotionLine_GetState(void)
{
    return s_state;
}

MotionLine_Error_t MotionLine_GetError(void)
{
    return s_error;
}

float MotionLine_GetLineError(void)
{
    return s_lineError;
}
