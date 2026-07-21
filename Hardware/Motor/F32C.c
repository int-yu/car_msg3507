#include "Hardware/Motor/F32C.h"
#include "Hardware/Comms/Serial.h"
#include "System/Delay.h"
#include <math.h>
#include <stddef.h>

#define F32C_FRAME_HEADER              0x7AU
#define F32C_FRAME_TAIL                0x7BU
#define F32C_MAX_TX_FRAME_LENGTH       9U
#define F32C_FEEDBACK_FRAME_LENGTH     9U

#define F32C_FUNCTION_MODE             0x00U
#define F32C_FUNCTION_SPEED            0x01U
#define F32C_FUNCTION_MULTI_POSITION   0x02U
#define F32C_FUNCTION_DISABLE          0x05U
#define F32C_FUNCTION_ENABLE           0x06U
#define F32C_FUNCTION_ACCELERATION     0x07U
#define F32C_FUNCTION_CLEAR_ANGLE      0x09U
#define F32C_FUNCTION_FEEDBACK         0x0EU

static uint8_t s_rxFrame[F32C_FEEDBACK_FRAME_LENGTH];
static uint8_t s_rxIndex;

/* BCC 为帧头、地址、功能/反馈类型和所有数据字节逐字节异或。 */
static uint8_t F32C_CalculateBCC(const uint8_t *data, uint8_t length)
{
    uint8_t index;
    uint8_t bcc = 0U;

    for (index = 0U; index < length; index++)
    {
        bcc ^= data[index];
    }
    return bcc;
}

static uint8_t F32C_AddressIsValid(uint8_t address)
{
    return (address >= F32C_MIN_ADDRESS) ? 1U : 0U;
}

static F32C_Result_t F32C_SendCommand(
    uint8_t address, uint8_t function,
    const uint8_t *payload, uint8_t payloadLength)
{
    uint8_t frame[F32C_MAX_TX_FRAME_LENGTH];
    uint8_t index;
    uint8_t checksumIndex;
    uint8_t frameLength;

    if ((F32C_AddressIsValid(address) == 0U) ||
        (payloadLength > 4U) ||
        ((payloadLength != 0U) && (payload == NULL)))
    {
        return F32C_RESULT_INVALID_ARGUMENT;
    }

    frame[0] = F32C_FRAME_HEADER;
    frame[1] = address;
    frame[2] = function;
    for (index = 0U; index < payloadLength; index++)
    {
        frame[3U + index] = payload[index];
    }

    checksumIndex = (uint8_t)(3U + payloadLength);
    frame[checksumIndex] = F32C_CalculateBCC(frame, checksumIndex);
    frame[checksumIndex + 1U] = F32C_FRAME_TAIL;
    frameLength = (uint8_t)(checksumIndex + 2U);

    Serial2_SendArray(frame, frameLength);
    /* 手册要求多电机级联时相邻命令至少间隔 1 ms。 */
    Delay_ms(F32C_COMMAND_INTERVAL_MS);
    return F32C_RESULT_OK;
}

static F32C_Result_t F32C_SendU16(
    uint8_t address, uint8_t function, uint16_t value)
{
    uint8_t payload[2];

    payload[0] = (uint8_t)(value >> 8U);
    payload[1] = (uint8_t)value;
    return F32C_SendCommand(address, function, payload, 2U);
}

void F32C_Init(void)
{
    s_rxIndex = 0U;
    Serial2_Init();
}

F32C_Result_t F32C_Enable(uint8_t address)
{
    return F32C_SendCommand(address, F32C_FUNCTION_ENABLE, NULL, 0U);
}

F32C_Result_t F32C_Disable(uint8_t address)
{
    return F32C_SendCommand(address, F32C_FUNCTION_DISABLE, NULL, 0U);
}

F32C_Result_t F32C_SetMode(uint8_t address, F32C_Mode_t mode)
{
    if (mode > F32C_MODE_SINGLE_TURN_DIRECT)
    {
        return F32C_RESULT_INVALID_ARGUMENT;
    }
    return F32C_SendU16(address, F32C_FUNCTION_MODE, (uint16_t)mode);
}

F32C_Result_t F32C_SetSpeedRPM(uint8_t address, int16_t speedRPM)
{
    if ((speedRPM > F32C_MAX_SPEED_RPM) ||
        (speedRPM < -F32C_MAX_SPEED_RPM))
    {
        return F32C_RESULT_INVALID_ARGUMENT;
    }
    return F32C_SendU16(
        address, F32C_FUNCTION_SPEED, (uint16_t)speedRPM);
}

F32C_Result_t F32C_SetAcceleration(
    uint8_t address, uint16_t accelerationRPMS2)
{
    return F32C_SendU16(
        address, F32C_FUNCTION_ACCELERATION, accelerationRPMS2);
}

F32C_Result_t F32C_ClearMultiTurnAngle(uint8_t address)
{
    return F32C_SendCommand(
        address, F32C_FUNCTION_CLEAR_ANGLE, NULL, 0U);
}

F32C_Result_t F32C_SetMultiTurnPositionDegrees(
    uint8_t address, float targetAngleDeg)
{
    uint8_t payload[4];
    int32_t scaledAngle;
    uint32_t encodedAngle;

    if ((!isfinite(targetAngleDeg)) ||
        (fabsf(targetAngleDeg) > F32C_MAX_MULTI_TURN_ANGLE_DEG))
    {
        return F32C_RESULT_INVALID_ARGUMENT;
    }

    /* 协议以 0.1° 为单位，负角度按 int32 补码发送。 */
    scaledAngle = (int32_t)((targetAngleDeg >= 0.0f) ?
        (targetAngleDeg * 10.0f + 0.5f) :
        (targetAngleDeg * 10.0f - 0.5f));
    encodedAngle = (uint32_t)scaledAngle;
    payload[0] = (uint8_t)(encodedAngle >> 24U);
    payload[1] = (uint8_t)(encodedAngle >> 16U);
    payload[2] = (uint8_t)(encodedAngle >> 8U);
    payload[3] = (uint8_t)encodedAngle;
    return F32C_SendCommand(
        address, F32C_FUNCTION_MULTI_POSITION, payload, 4U);
}

F32C_Result_t F32C_RequestFeedback(
    uint8_t address, F32C_FeedbackType_t type)
{
    uint8_t payload;

    if (type > F32C_FEEDBACK_BUS_VOLTAGE)
    {
        return F32C_RESULT_INVALID_ARGUMENT;
    }

    payload = (uint8_t)type;
    return F32C_SendCommand(
        address, F32C_FUNCTION_FEEDBACK, &payload, 1U);
}

static uint8_t F32C_ParseFeedbackFrame(F32C_Feedback_t *feedback)
{
    uint32_t rawValue;

    if ((s_rxFrame[0] != F32C_FRAME_HEADER) ||
        (s_rxFrame[8] != F32C_FRAME_TAIL) ||
        (F32C_AddressIsValid(s_rxFrame[1]) == 0U) ||
        (s_rxFrame[2] > (uint8_t)F32C_FEEDBACK_BUS_VOLTAGE) ||
        (F32C_CalculateBCC(s_rxFrame, 7U) != s_rxFrame[7]))
    {
        return 0U;
    }

    rawValue = ((uint32_t)s_rxFrame[3] << 24U) |
               ((uint32_t)s_rxFrame[4] << 16U) |
               ((uint32_t)s_rxFrame[5] << 8U) |
               (uint32_t)s_rxFrame[6];
    feedback->address = s_rxFrame[1];
    feedback->type = (F32C_FeedbackType_t)s_rxFrame[2];
    feedback->rawValue = (int32_t)rawValue;
    return 1U;
}

uint8_t F32C_PopFeedback(F32C_Feedback_t *feedback)
{
    uint8_t byte;

    if (feedback == NULL)
    {
        return 0U;
    }

    while (Serial2_ReadByte(&byte) != 0U)
    {
        if (s_rxIndex == 0U)
        {
            if (byte == F32C_FRAME_HEADER)
            {
                s_rxFrame[s_rxIndex++] = byte;
            }
            continue;
        }

        s_rxFrame[s_rxIndex++] = byte;
        if (s_rxIndex < F32C_FEEDBACK_FRAME_LENGTH)
        {
            continue;
        }

        s_rxIndex = 0U;
        if (F32C_ParseFeedbackFrame(feedback) != 0U)
        {
            return 1U;
        }

        /* 当前字节若恰好是下一帧帧头，则保留它用于继续同步。 */
        if (byte == F32C_FRAME_HEADER)
        {
            s_rxFrame[0] = byte;
            s_rxIndex = 1U;
        }
    }
    return 0U;
}
