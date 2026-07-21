#include "Application/Gimbal/Gimbal.h"
#include "Hardware/Motor/F32C.h"
#include <math.h>
#include <stddef.h>

typedef struct
{
    uint8_t address;
    uint8_t hasAngleFeedback;
    uint8_t targetValid;
    uint8_t settleFeedbackCount;
    float currentAngleDeg;
    float targetAngleDeg;
    float feedbackElapsedSeconds;
} Gimbal_AxisContext_t;

typedef struct
{
    Gimbal_AxisContext_t axis[GIMBAL_AXIS_COUNT];
    Gimbal_State_t state;
    Gimbal_Error_t error;
    float feedbackRequestElapsedSeconds;
    Gimbal_Axis_t nextFeedbackAxis;
} Gimbal_Context_t;

static Gimbal_Context_t s_context;

static uint8_t Gimbal_ParametersAreValid(void)
{
    if ((GIMBAL_X_MOTOR_ADDRESS < F32C_MIN_ADDRESS) ||
        (GIMBAL_Y_MOTOR_ADDRESS < F32C_MIN_ADDRESS) ||
        (GIMBAL_X_MOTOR_ADDRESS == GIMBAL_Y_MOTOR_ADDRESS) ||
        (GIMBAL_DEFAULT_SPEED_RPM <= 0) ||
        (GIMBAL_DEFAULT_SPEED_RPM > F32C_MAX_SPEED_RPM) ||
        (GIMBAL_DEFAULT_ACCELERATION_RPMS2 == 0U) ||
        (!isfinite(GIMBAL_FEEDBACK_REQUEST_PERIOD_SECONDS)) ||
        (!isfinite(GIMBAL_FEEDBACK_TIMEOUT_SECONDS)) ||
        (!isfinite(GIMBAL_POSITION_TOLERANCE_DEG)) ||
        (GIMBAL_FEEDBACK_REQUEST_PERIOD_SECONDS <= 0.0f) ||
        (GIMBAL_FEEDBACK_TIMEOUT_SECONDS <=
         GIMBAL_FEEDBACK_REQUEST_PERIOD_SECONDS) ||
        (GIMBAL_POSITION_TOLERANCE_DEG < 0.0f) ||
        (GIMBAL_POSITION_SETTLE_FEEDBACKS == 0U))
    {
        return 0U;
    }
    return 1U;
}

static Gimbal_AxisContext_t *Gimbal_GetAxisContext(Gimbal_Axis_t axis)
{
    if ((axis != GIMBAL_AXIS_X) && (axis != GIMBAL_AXIS_Y))
    {
        return NULL;
    }
    return &s_context.axis[(uint8_t)axis];
}

static Gimbal_AxisContext_t *Gimbal_FindAxisByAddress(uint8_t address)
{
    uint8_t index;

    for (index = 0U; index < (uint8_t)GIMBAL_AXIS_COUNT; index++)
    {
        if (s_context.axis[index].address == address)
        {
            return &s_context.axis[index];
        }
    }
    return NULL;
}

static void Gimbal_ResetAxisRuntime(Gimbal_AxisContext_t *axis)
{
    axis->hasAngleFeedback = 0U;
    axis->targetValid = 0U;
    axis->settleFeedbackCount = 0U;
    axis->currentAngleDeg = 0.0f;
    axis->targetAngleDeg = 0.0f;
    axis->feedbackElapsedSeconds = 0.0f;
}

static Gimbal_Result_t Gimbal_MapProtocolResult(F32C_Result_t result)
{
    if (result == F32C_RESULT_OK)
    {
        return GIMBAL_RESULT_OK;
    }
    if (result == F32C_RESULT_INVALID_ARGUMENT)
    {
        return GIMBAL_RESULT_INVALID_ARGUMENT;
    }
    return GIMBAL_RESULT_PROTOCOL_ERROR;
}

static uint8_t Gimbal_ConfigureAxis(uint8_t address)
{
    if ((F32C_Enable(address) != F32C_RESULT_OK) ||
        (F32C_SetMode(address, F32C_MODE_MULTI_TURN_T) !=
         F32C_RESULT_OK) ||
        (F32C_SetAcceleration(
             address, GIMBAL_DEFAULT_ACCELERATION_RPMS2) !=
         F32C_RESULT_OK) ||
        (F32C_SetSpeedRPM(address, GIMBAL_DEFAULT_SPEED_RPM) !=
         F32C_RESULT_OK))
    {
        return 0U;
    }
    return 1U;
}

static void Gimbal_DisableHardware(void)
{
    (void)F32C_Disable(GIMBAL_X_MOTOR_ADDRESS);
    (void)F32C_Disable(GIMBAL_Y_MOTOR_ADDRESS);
}

static void Gimbal_SetError(Gimbal_Error_t error)
{
    Gimbal_DisableHardware();
    s_context.error = error;
    s_context.state = GIMBAL_STATE_ERROR;
}

Gimbal_Result_t Gimbal_Init(void)
{
    s_context.state = GIMBAL_STATE_UNINITIALIZED;
    s_context.error = GIMBAL_ERROR_NONE;
    s_context.feedbackRequestElapsedSeconds = 0.0f;
    s_context.nextFeedbackAxis = GIMBAL_AXIS_X;
    s_context.axis[GIMBAL_AXIS_X].address = GIMBAL_X_MOTOR_ADDRESS;
    s_context.axis[GIMBAL_AXIS_Y].address = GIMBAL_Y_MOTOR_ADDRESS;
    Gimbal_ResetAxisRuntime(&s_context.axis[GIMBAL_AXIS_X]);
    Gimbal_ResetAxisRuntime(&s_context.axis[GIMBAL_AXIS_Y]);

    if (Gimbal_ParametersAreValid() == 0U)
    {
        s_context.error = GIMBAL_ERROR_PARAMETER;
        s_context.state = GIMBAL_STATE_ERROR;
        return GIMBAL_RESULT_INVALID_ARGUMENT;
    }

    F32C_Init();
    s_context.state = GIMBAL_STATE_DISABLED;
    return GIMBAL_RESULT_OK;
}

Gimbal_Result_t Gimbal_Enable(void)
{
    if (s_context.state == GIMBAL_STATE_UNINITIALIZED)
    {
        return GIMBAL_RESULT_NOT_INITIALIZED;
    }

    Gimbal_ResetAxisRuntime(&s_context.axis[GIMBAL_AXIS_X]);
    Gimbal_ResetAxisRuntime(&s_context.axis[GIMBAL_AXIS_Y]);
    s_context.feedbackRequestElapsedSeconds =
        GIMBAL_FEEDBACK_REQUEST_PERIOD_SECONDS;
    s_context.nextFeedbackAxis = GIMBAL_AXIS_X;
    s_context.error = GIMBAL_ERROR_NONE;

    if ((Gimbal_ConfigureAxis(GIMBAL_X_MOTOR_ADDRESS) == 0U) ||
        (Gimbal_ConfigureAxis(GIMBAL_Y_MOTOR_ADDRESS) == 0U))
    {
        Gimbal_SetError(GIMBAL_ERROR_PROTOCOL);
        return GIMBAL_RESULT_PROTOCOL_ERROR;
    }

    s_context.state = GIMBAL_STATE_ENABLED;
    return GIMBAL_RESULT_OK;
}

Gimbal_Result_t Gimbal_Disable(void)
{
    if (s_context.state == GIMBAL_STATE_UNINITIALIZED)
    {
        return GIMBAL_RESULT_NOT_INITIALIZED;
    }

    Gimbal_DisableHardware();
    s_context.state = GIMBAL_STATE_DISABLED;
    s_context.error = GIMBAL_ERROR_NONE;
    return GIMBAL_RESULT_OK;
}

Gimbal_Result_t Gimbal_SetTargetAngle(
    Gimbal_Axis_t axis, float targetAngleDeg)
{
    Gimbal_AxisContext_t *axisContext = Gimbal_GetAxisContext(axis);
    Gimbal_Result_t result;

    if (axisContext == NULL)
    {
        return GIMBAL_RESULT_INVALID_ARGUMENT;
    }
    if (s_context.state != GIMBAL_STATE_ENABLED)
    {
        return GIMBAL_RESULT_NOT_ENABLED;
    }

    result = Gimbal_MapProtocolResult(
        F32C_SetMultiTurnPositionDegrees(
            axisContext->address, targetAngleDeg));
    if (result != GIMBAL_RESULT_OK)
    {
        return result;
    }

    axisContext->targetAngleDeg = targetAngleDeg;
    axisContext->targetValid = 1U;
    axisContext->settleFeedbackCount = 0U;
    return GIMBAL_RESULT_OK;
}

Gimbal_Result_t Gimbal_SetTargetAngles(
    float targetXDeg, float targetYDeg)
{
    Gimbal_Result_t result;

    result = Gimbal_SetTargetAngle(GIMBAL_AXIS_X, targetXDeg);
    if (result != GIMBAL_RESULT_OK)
    {
        return result;
    }
    return Gimbal_SetTargetAngle(GIMBAL_AXIS_Y, targetYDeg);
}

static void Gimbal_UpdateSettleState(Gimbal_AxisContext_t *axis)
{
    if ((axis->targetValid != 0U) &&
        (fabsf(axis->currentAngleDeg - axis->targetAngleDeg) <=
         GIMBAL_POSITION_TOLERANCE_DEG))
    {
        if (axis->settleFeedbackCount <
            GIMBAL_POSITION_SETTLE_FEEDBACKS)
        {
            axis->settleFeedbackCount++;
        }
    }
    else
    {
        axis->settleFeedbackCount = 0U;
    }
}

static void Gimbal_ProcessFeedback(const F32C_Feedback_t *feedback)
{
    Gimbal_AxisContext_t *axis =
        Gimbal_FindAxisByAddress(feedback->address);

    if (axis == NULL)
    {
        return;
    }

    axis->feedbackElapsedSeconds = 0.0f;
    switch (feedback->type)
    {
        case F32C_FEEDBACK_TOTAL_ANGLE:
            axis->currentAngleDeg = (float)feedback->rawValue * 0.1f;
            axis->hasAngleFeedback = 1U;
            Gimbal_UpdateSettleState(axis);
            break;

        case F32C_FEEDBACK_SPEED:
        case F32C_FEEDBACK_MECHANICAL_ANGLE:
        case F32C_FEEDBACK_ACCELERATION:
        case F32C_FEEDBACK_BUS_VOLTAGE:
        default:
            break;
    }
}

static uint8_t Gimbal_RequestNextAngleFeedback(void)
{
    Gimbal_AxisContext_t *axis =
        &s_context.axis[(uint8_t)s_context.nextFeedbackAxis];

    if (F32C_RequestFeedback(
            axis->address, F32C_FEEDBACK_TOTAL_ANGLE) !=
        F32C_RESULT_OK)
    {
        return 0U;
    }

    s_context.nextFeedbackAxis =
        (s_context.nextFeedbackAxis == GIMBAL_AXIS_X) ?
        GIMBAL_AXIS_Y : GIMBAL_AXIS_X;
    return 1U;
}

void Gimbal_Update(float dt)
{
    F32C_Feedback_t feedback;
    uint8_t axisIndex;

    if (s_context.state == GIMBAL_STATE_UNINITIALIZED)
    {
        return;
    }
    if ((!isfinite(dt)) || (dt <= 0.0f))
    {
        Gimbal_SetError(GIMBAL_ERROR_PARAMETER);
        return;
    }

    while (F32C_PopFeedback(&feedback) != 0U)
    {
        Gimbal_ProcessFeedback(&feedback);
    }

    if (s_context.state != GIMBAL_STATE_ENABLED)
    {
        return;
    }

    for (axisIndex = 0U;
         axisIndex < (uint8_t)GIMBAL_AXIS_COUNT;
         axisIndex++)
    {
        s_context.axis[axisIndex].feedbackElapsedSeconds += dt;
        if (s_context.axis[axisIndex].feedbackElapsedSeconds >
            GIMBAL_FEEDBACK_TIMEOUT_SECONDS)
        {
            Gimbal_SetError(GIMBAL_ERROR_FEEDBACK_TIMEOUT);
            return;
        }
    }

    s_context.feedbackRequestElapsedSeconds += dt;
    if (s_context.feedbackRequestElapsedSeconds >=
        GIMBAL_FEEDBACK_REQUEST_PERIOD_SECONDS)
    {
        s_context.feedbackRequestElapsedSeconds = 0.0f;
        /* 每次只查询一个地址，避免两个电机的反馈同时占用总线。 */
        if (Gimbal_RequestNextAngleFeedback() == 0U)
        {
            Gimbal_SetError(GIMBAL_ERROR_PROTOCOL);
        }
    }
}

uint8_t Gimbal_IsEnabled(void)
{
    return (s_context.state == GIMBAL_STATE_ENABLED) ? 1U : 0U;
}

uint8_t Gimbal_HasFeedback(Gimbal_Axis_t axis)
{
    Gimbal_AxisContext_t *axisContext = Gimbal_GetAxisContext(axis);
    return (axisContext != NULL) ? axisContext->hasAngleFeedback : 0U;
}

uint8_t Gimbal_IsAxisAtTarget(Gimbal_Axis_t axis)
{
    Gimbal_AxisContext_t *axisContext = Gimbal_GetAxisContext(axis);

    if ((axisContext == NULL) ||
        (axisContext->hasAngleFeedback == 0U) ||
        (axisContext->targetValid == 0U))
    {
        return 0U;
    }
    return (axisContext->settleFeedbackCount >=
            GIMBAL_POSITION_SETTLE_FEEDBACKS) ? 1U : 0U;
}

uint8_t Gimbal_AreTargetsReached(void)
{
    return ((Gimbal_IsAxisAtTarget(GIMBAL_AXIS_X) != 0U) &&
            (Gimbal_IsAxisAtTarget(GIMBAL_AXIS_Y) != 0U)) ? 1U : 0U;
}

float Gimbal_GetCurrentAngleDeg(Gimbal_Axis_t axis)
{
    Gimbal_AxisContext_t *axisContext = Gimbal_GetAxisContext(axis);
    return (axisContext != NULL) ? axisContext->currentAngleDeg : 0.0f;
}

float Gimbal_GetTargetAngleDeg(Gimbal_Axis_t axis)
{
    Gimbal_AxisContext_t *axisContext = Gimbal_GetAxisContext(axis);
    return (axisContext != NULL) ? axisContext->targetAngleDeg : 0.0f;
}

Gimbal_State_t Gimbal_GetState(void)
{
    return s_context.state;
}

Gimbal_Error_t Gimbal_GetError(void)
{
    return s_context.error;
}
