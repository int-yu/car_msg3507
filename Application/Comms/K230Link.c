#include "Application/Comms/K230Link.h"
#include "Hardware/Comms/Serial.h"
#include <stddef.h>

typedef enum
{
    K230_PARSER_MAGIC_0 = 0,
    K230_PARSER_MAGIC_1,
    K230_PARSER_VERSION,
    K230_PARSER_TYPE,
    K230_PARSER_SEQUENCE,
    K230_PARSER_LENGTH,
    K230_PARSER_PAYLOAD,
    K230_PARSER_CRC
} K230Link_ParserState_t;

typedef struct
{
    K230Link_ParserState_t state;
    uint8_t type;
    uint8_t sequence;
    uint8_t length;
    uint8_t payloadIndex;
    uint8_t payload[K230_LINK_MAX_PAYLOAD_LENGTH];
    uint8_t crc;
} K230Link_Parser_t;

static K230Link_Parser_t s_parser;
static K230Link_Target_t s_target;
static uint8_t s_hasTarget;
static uint8_t s_peerReadyReceived;
static uint8_t s_readyAcknowledged;
static uint8_t s_readySequence;
static uint8_t s_nextSequence;
static uint8_t s_readyRetryTicks;
static uint8_t s_capturePending;
static uint8_t s_captureTimeoutTicks;
static uint8_t s_captureAckPending;
static uint8_t s_captureAckOk;
static uint16_t s_captureAckIndex;

static uint8_t K230Link_Crc8Update(uint8_t crc, uint8_t data)
{
    uint8_t bit;

    crc ^= data;
    for (bit = 0U; bit < 8U; bit++)
    {
        if ((crc & 0x80U) != 0U)
        {
            crc = (uint8_t)((crc << 1U) ^ 0x07U);
        }
        else
        {
            crc <<= 1U;
        }
    }
    return crc;
}

static uint8_t K230Link_NextSequence(void)
{
    uint8_t sequence = s_nextSequence;
    s_nextSequence++;
    return sequence;
}

static void K230Link_ResetParser(void)
{
    s_parser.state = K230_PARSER_MAGIC_0;
    s_parser.type = 0U;
    s_parser.sequence = 0U;
    s_parser.length = 0U;
    s_parser.payloadIndex = 0U;
    s_parser.crc = 0U;
}

static void K230Link_SendFrame(uint8_t type,
                               uint8_t sequence,
                               const uint8_t *payload,
                               uint8_t length)
{
    uint8_t frame[7U + K230_LINK_MAX_PAYLOAD_LENGTH];
    uint8_t crc = 0U;
    uint8_t index;

    if ((length > K230_LINK_MAX_PAYLOAD_LENGTH) ||
        ((length > 0U) && (payload == NULL)))
    {
        return;
    }

    frame[0] = K230_LINK_FRAME_MAGIC_0;
    frame[1] = K230_LINK_FRAME_MAGIC_1;
    frame[2] = K230_LINK_FRAME_VERSION;
    frame[3] = type;
    frame[4] = sequence;
    frame[5] = length;

    for (index = 2U; index < 6U; index++)
    {
        crc = K230Link_Crc8Update(crc, frame[index]);
    }
    for (index = 0U; index < length; index++)
    {
        frame[6U + index] = payload[index];
        crc = K230Link_Crc8Update(crc, payload[index]);
    }
    frame[6U + length] = crc;
    Serial2_SendArray(frame, (uint16_t)(7U + length));
}

static void K230Link_HandleFrame(void)
{
    if ((s_parser.type == K230_LINK_MESSAGE_READY) &&
        (s_parser.length == 0U))
    {
        uint8_t acknowledgedSequence = s_parser.sequence;

        s_peerReadyReceived = 1U;
        K230Link_SendFrame(
            K230_LINK_MESSAGE_READY_ACK,
            K230Link_NextSequence(),
            &acknowledgedSequence,
            1U);
    }
    else if ((s_parser.type == K230_LINK_MESSAGE_READY_ACK) &&
             (s_parser.length == 1U) &&
             (s_parser.payload[0] == s_readySequence))
    {
        s_readyAcknowledged = 1U;
    }
    else if ((s_parser.type == K230_LINK_MESSAGE_TARGET) &&
             (s_parser.length == 5U) &&
             (K230Link_IsReady() != 0U))
    {
        s_target.valid = (s_parser.payload[0] != 0U) ? 1U : 0U;
        s_target.offsetX = (int16_t)(
            (uint16_t)s_parser.payload[1] |
            ((uint16_t)s_parser.payload[2] << 8U));
        s_target.offsetY = (int16_t)(
            (uint16_t)s_parser.payload[3] |
            ((uint16_t)s_parser.payload[4] << 8U));
        s_target.sequence = s_parser.sequence;
        s_hasTarget = 1U;
    }
    else if ((s_parser.type == K230_LINK_MESSAGE_CAPTURE_ACK) &&
             (s_parser.length == 3U) &&
             (s_capturePending != 0U))
    {
        s_captureAckOk = (s_parser.payload[0] != 0U) ? 1U : 0U;
        s_captureAckIndex = (uint16_t)(
            (uint16_t)s_parser.payload[1] |
            ((uint16_t)s_parser.payload[2] << 8U));
        s_captureAckPending = 1U;
        s_capturePending = 0U;
    }
}

static void K230Link_ParseByte(uint8_t data)
{
    switch (s_parser.state)
    {
        case K230_PARSER_MAGIC_0:
            if (data == K230_LINK_FRAME_MAGIC_0)
            {
                s_parser.state = K230_PARSER_MAGIC_1;
            }
            break;

        case K230_PARSER_MAGIC_1:
            if (data == K230_LINK_FRAME_MAGIC_1)
            {
                s_parser.state = K230_PARSER_VERSION;
            }
            else
            {
                s_parser.state = (data == K230_LINK_FRAME_MAGIC_0) ?
                    K230_PARSER_MAGIC_1 : K230_PARSER_MAGIC_0;
            }
            break;

        case K230_PARSER_VERSION:
            if (data != K230_LINK_FRAME_VERSION)
            {
                K230Link_ResetParser();
                break;
            }
            s_parser.crc = K230Link_Crc8Update(0U, data);
            s_parser.state = K230_PARSER_TYPE;
            break;

        case K230_PARSER_TYPE:
            s_parser.type = data;
            s_parser.crc = K230Link_Crc8Update(s_parser.crc, data);
            s_parser.state = K230_PARSER_SEQUENCE;
            break;

        case K230_PARSER_SEQUENCE:
            s_parser.sequence = data;
            s_parser.crc = K230Link_Crc8Update(s_parser.crc, data);
            s_parser.state = K230_PARSER_LENGTH;
            break;

        case K230_PARSER_LENGTH:
            if (data > K230_LINK_MAX_PAYLOAD_LENGTH)
            {
                K230Link_ResetParser();
                break;
            }
            s_parser.length = data;
            s_parser.payloadIndex = 0U;
            s_parser.crc = K230Link_Crc8Update(s_parser.crc, data);
            s_parser.state = (data == 0U) ?
                K230_PARSER_CRC : K230_PARSER_PAYLOAD;
            break;

        case K230_PARSER_PAYLOAD:
            s_parser.payload[s_parser.payloadIndex] = data;
            s_parser.payloadIndex++;
            s_parser.crc = K230Link_Crc8Update(s_parser.crc, data);
            if (s_parser.payloadIndex >= s_parser.length)
            {
                s_parser.state = K230_PARSER_CRC;
            }
            break;

        case K230_PARSER_CRC:
            if (data == s_parser.crc)
            {
                K230Link_HandleFrame();
            }
            K230Link_ResetParser();
            break;

        default:
            K230Link_ResetParser();
            break;
    }
}

void K230Link_Init(void)
{
    Serial2_Init();
    K230Link_ResetParser();
    s_target.valid = 0U;
    s_target.offsetX = 0;
    s_target.offsetY = 0;
    s_target.sequence = 0U;
    s_hasTarget = 0U;
    s_peerReadyReceived = 0U;
    s_readyAcknowledged = 0U;
    s_nextSequence = 0U;
    s_readySequence = K230Link_NextSequence();
    s_readyRetryTicks = K230_LINK_READY_RETRY_TICKS;
    s_capturePending = 0U;
    s_captureTimeoutTicks = 0U;
    s_captureAckPending = 0U;
    s_captureAckOk = 0U;
    s_captureAckIndex = 0U;
}

void K230Link_Update(uint8_t elapsedTicks)
{
    uint8_t data;

    while (Serial2_ReadByte(&data) != 0U)
    {
        K230Link_ParseByte(data);
    }

    if (s_readyAcknowledged == 0U)
    {
        uint16_t ticks = (uint16_t)s_readyRetryTicks + elapsedTicks;

        s_readyRetryTicks = (ticks >= K230_LINK_READY_RETRY_TICKS) ?
            K230_LINK_READY_RETRY_TICKS : (uint8_t)ticks;
        if (s_readyRetryTicks >= K230_LINK_READY_RETRY_TICKS)
        {
            K230Link_SendFrame(
                K230_LINK_MESSAGE_READY,
                s_readySequence,
                NULL,
                0U);
            s_readyRetryTicks = 0U;
        }
    }

    if (s_capturePending != 0U)
    {
        uint16_t ticks = (uint16_t)s_captureTimeoutTicks + elapsedTicks;

        if (ticks >= K230_LINK_CAPTURE_TIMEOUT_TICKS)
        {
            /* 超时按失败上报，index 固定为 0。 */
            s_capturePending = 0U;
            s_captureTimeoutTicks = 0U;
            s_captureAckOk = 0U;
            s_captureAckIndex = 0U;
            s_captureAckPending = 1U;
        }
        else
        {
            s_captureTimeoutTicks = (uint8_t)ticks;
        }
    }
}

uint8_t K230Link_IsReady(void)
{
    return ((s_peerReadyReceived != 0U) &&
            (s_readyAcknowledged != 0U)) ? 1U : 0U;
}

uint8_t K230Link_GetTarget(K230Link_Target_t *target)
{
    if ((target == NULL) || (s_hasTarget == 0U))
    {
        return 0U;
    }
    *target = s_target;
    return 1U;
}

uint8_t K230Link_RequestCapture(uint8_t count)
{
    uint8_t payload;

    if ((count == 0U) || (count > K230_LINK_CAPTURE_MAX_COUNT))
    {
        return 0U;
    }
    if (K230Link_IsReady() == 0U)
    {
        return 0U;
    }
    if (s_capturePending != 0U)
    {
        return 0U;
    }

    payload = count;
    K230Link_SendFrame(
        K230_LINK_MESSAGE_CAPTURE,
        K230Link_NextSequence(),
        &payload,
        1U);

    s_capturePending = 1U;
    s_captureTimeoutTicks = 0U;
    return 1U;
}

uint8_t K230Link_IsCapturePending(void)
{
    return s_capturePending;
}

uint8_t K230Link_PopCaptureAck(uint8_t *ok, uint16_t *index)
{
    if ((ok == NULL) || (index == NULL) || (s_captureAckPending == 0U))
    {
        return 0U;
    }

    *ok = s_captureAckOk;
    *index = s_captureAckIndex;
    s_captureAckPending = 0U;
    return 1U;
}
