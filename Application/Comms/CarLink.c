#include "Application/Comms/CarLink.h"
#include "Application/Core/CarRole.h"
#include "Application/Debug/TelemFrame.h"
#include "Hardware/Comms/Serial.h"
#include <stddef.h>

/* 收帧队列深度：主从都可能主动发，命令还没被 App 读走时又来一条也不丢。
 * 主循环里“解析入队”和“App 出队”顺序执行、非中断，故无需加锁。 */
#define CAR_LINK_RX_QUEUE_LEN 6U

typedef enum
{
    CAR_LINK_PARSER_MAGIC_0 = 0,
    CAR_LINK_PARSER_MAGIC_1,
    CAR_LINK_PARSER_VERSION,
    CAR_LINK_PARSER_TYPE,
    CAR_LINK_PARSER_SEQUENCE,
    CAR_LINK_PARSER_LENGTH,
    CAR_LINK_PARSER_PAYLOAD,
    CAR_LINK_PARSER_CRC
} CarLink_ParserState_t;

typedef struct
{
    CarLink_ParserState_t state;
    uint8_t type;
    uint8_t sequence;
    uint8_t length;
    uint8_t payloadIndex;
    uint8_t payload[CAR_LINK_MAX_PAYLOAD];
    uint8_t crc;
} CarLink_Parser_t;

static CarLink_Parser_t s_parser;

/* 环形消息队列（count 语义：head 入、tail 出）。 */
static CarLink_Message_t s_rxQueue[CAR_LINK_RX_QUEUE_LEN];
static uint8_t s_rxHead;
static uint8_t s_rxTail;
static uint8_t s_rxCount;

static uint8_t s_nextSequence;
static uint8_t s_heartbeatTicks;
static uint16_t s_peerSilenceTicks;
static uint8_t s_peerAlive;
static uint16_t s_txDropCount;

static uint8_t CarLink_Crc8Update(uint8_t crc, uint8_t data)
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

static uint8_t CarLink_NextSequence(void)
{
    uint8_t sequence = s_nextSequence;
    s_nextSequence++;
    return sequence;
}

static void CarLink_ResetParser(void)
{
    s_parser.state = CAR_LINK_PARSER_MAGIC_0;
    s_parser.type = 0U;
    s_parser.sequence = 0U;
    s_parser.length = 0U;
    s_parser.payloadIndex = 0U;
    s_parser.crc = 0U;
}

/* 收到有效业务帧后压入队列；队满则丢最旧一条（保留最新，命令更实时）。 */
static void CarLink_Enqueue(uint8_t type, const uint8_t *payload, uint8_t length)
{
    CarLink_Message_t *slot;
    uint8_t index;

    if (length > CAR_LINK_MAX_PAYLOAD)
    {
        return;
    }

    if (s_rxCount >= CAR_LINK_RX_QUEUE_LEN)
    {
        /* 丢最旧：tail 前移一格，腾出空间。 */
        s_rxTail = (uint8_t)((s_rxTail + 1U) % CAR_LINK_RX_QUEUE_LEN);
        s_rxCount--;
    }

    slot = &s_rxQueue[s_rxHead];
    slot->type = type;
    slot->length = length;
    for (index = 0U; index < length; index++)
    {
        slot->payload[index] = payload[index];
    }
    s_rxHead = (uint8_t)((s_rxHead + 1U) % CAR_LINK_RX_QUEUE_LEN);
    s_rxCount++;
}

static void CarLink_HandleFrame(void)
{
    /* 任意合法帧都算“对端还活着”，刷新掉线计时。 */
    s_peerSilenceTicks = 0U;
    s_peerAlive = 1U;

    /* 心跳只维护链路存活，不打扰上层，不入队。 */
    if (s_parser.type == CAR_LINK_MSG_HEARTBEAT)
    {
        return;
    }

    CarLink_Enqueue(s_parser.type, s_parser.payload, s_parser.length);
}

static void CarLink_SendFrame(uint8_t type,
                              uint8_t sequence,
                              const uint8_t *payload,
                              uint8_t length)
{
    uint8_t frame[7U + CAR_LINK_MAX_PAYLOAD];
    uint8_t crc = 0U;
    uint8_t index;

    if ((length > CAR_LINK_MAX_PAYLOAD) ||
        ((length > 0U) && (payload == NULL)))
    {
        return;
    }

    frame[0] = CAR_LINK_FRAME_MAGIC_0;
    frame[1] = CAR_LINK_FRAME_MAGIC_1;
    frame[2] = CAR_LINK_FRAME_VERSION;
    frame[3] = type;
    frame[4] = sequence;
    frame[5] = length;

    for (index = 2U; index < 6U; index++)
    {
        crc = CarLink_Crc8Update(crc, frame[index]);
    }
    for (index = 0U; index < length; index++)
    {
        frame[6U + index] = payload[index];
        crc = CarLink_Crc8Update(crc, payload[index]);
    }
    frame[6U + length] = crc;

    /* Serial2 现为 DMA 非阻塞发送：缓冲够则整帧入环形缓冲，满则整帧丢弃。 */
    if (Serial2_SendArray(frame, (uint16_t)(7U + length)) == 0U)
    {
        s_txDropCount++;
    }
}

static void CarLink_ParseByte(uint8_t data)
{
    switch (s_parser.state)
    {
        case CAR_LINK_PARSER_MAGIC_0:
            if (data == CAR_LINK_FRAME_MAGIC_0)
            {
                s_parser.state = CAR_LINK_PARSER_MAGIC_1;
            }
            break;

        case CAR_LINK_PARSER_MAGIC_1:
            if (data == CAR_LINK_FRAME_MAGIC_1)
            {
                s_parser.state = CAR_LINK_PARSER_VERSION;
            }
            else
            {
                s_parser.state = (data == CAR_LINK_FRAME_MAGIC_0) ?
                    CAR_LINK_PARSER_MAGIC_1 : CAR_LINK_PARSER_MAGIC_0;
            }
            break;

        case CAR_LINK_PARSER_VERSION:
            if (data != CAR_LINK_FRAME_VERSION)
            {
                CarLink_ResetParser();
                break;
            }
            s_parser.crc = CarLink_Crc8Update(0U, data);
            s_parser.state = CAR_LINK_PARSER_TYPE;
            break;

        case CAR_LINK_PARSER_TYPE:
            s_parser.type = data;
            s_parser.crc = CarLink_Crc8Update(s_parser.crc, data);
            s_parser.state = CAR_LINK_PARSER_SEQUENCE;
            break;

        case CAR_LINK_PARSER_SEQUENCE:
            s_parser.sequence = data;
            s_parser.crc = CarLink_Crc8Update(s_parser.crc, data);
            s_parser.state = CAR_LINK_PARSER_LENGTH;
            break;

        case CAR_LINK_PARSER_LENGTH:
            if (data > CAR_LINK_MAX_PAYLOAD)
            {
                CarLink_ResetParser();
                break;
            }
            s_parser.length = data;
            s_parser.payloadIndex = 0U;
            s_parser.crc = CarLink_Crc8Update(s_parser.crc, data);
            s_parser.state = (data == 0U) ?
                CAR_LINK_PARSER_CRC : CAR_LINK_PARSER_PAYLOAD;
            break;

        case CAR_LINK_PARSER_PAYLOAD:
            s_parser.payload[s_parser.payloadIndex] = data;
            s_parser.payloadIndex++;
            s_parser.crc = CarLink_Crc8Update(s_parser.crc, data);
            if (s_parser.payloadIndex >= s_parser.length)
            {
                s_parser.state = CAR_LINK_PARSER_CRC;
            }
            break;

        case CAR_LINK_PARSER_CRC:
            if (data == s_parser.crc)
            {
                CarLink_HandleFrame();
            }
            CarLink_ResetParser();
            break;

        default:
            CarLink_ResetParser();
            break;
    }
}

#if CAR_IS_MASTER
/*
 * 从车遥测透传（仅主车）。从车遥测帧以 0xAB 起头，格式与主车遥测相同。主车不
 * 解析其内容，只按帧长收齐后原样转发给电脑（Serial1）——网页按 0xAB 分流到从车
 * 曲线面板。与 0xAA 控制帧靠首字节区分，各自独立状态机；帧整体发送不会字节级
 * 交错，故把每个字节同时喂给两个状态机即可，互不干扰（偶发误触发由 CRC 兜底）。
 */
typedef enum
{
    TELEM_PT_MAGIC_0 = 0,
    TELEM_PT_MAGIC_1,
    TELEM_PT_HEADER,
    TELEM_PT_BODY
} CarLink_TelemPtState_t;

static CarLink_TelemPtState_t s_ptState;
static uint8_t s_ptFrame[TELEM_FRAME_MAX_BYTES];
static uint16_t s_ptIndex;
static uint16_t s_ptRemaining;

static void CarLink_TelemPassthruByte(uint8_t data)
{
    switch (s_ptState)
    {
        case TELEM_PT_MAGIC_0:
            if (data == CAR_LINK_TELEM_MAGIC)
            {
                s_ptFrame[0] = data;
                s_ptIndex = 1U;
                s_ptState = TELEM_PT_MAGIC_1;
            }
            break;

        case TELEM_PT_MAGIC_1:
            if (data == CAR_LINK_FRAME_MAGIC_1)
            {
                s_ptFrame[s_ptIndex++] = data;
                s_ptState = TELEM_PT_HEADER;
            }
            else if (data == CAR_LINK_TELEM_MAGIC)
            {
                /* 连续两个 0xAB：留在此态等下一字节。 */
                s_ptFrame[0] = data;
                s_ptIndex = 1U;
            }
            else
            {
                s_ptState = TELEM_PT_MAGIC_0;
            }
            break;

        case TELEM_PT_HEADER:
            /* 收 VER TYPE SEQ LEN；写满到 index 6 时最后一字节即 LEN。 */
            s_ptFrame[s_ptIndex++] = data;
            if (s_ptIndex == 6U)
            {
                s_ptRemaining = (uint16_t)data + 1U; /* payload(LEN) + CRC(1) */
                s_ptState = TELEM_PT_BODY;
            }
            break;

        case TELEM_PT_BODY:
            s_ptFrame[s_ptIndex++] = data;
            s_ptRemaining--;
            if (s_ptRemaining == 0U)
            {
                /* 整帧收齐，原样转发给电脑（首字节仍是 0xAB，供网页分流）。 */
                Serial1_SendArray(s_ptFrame, s_ptIndex);
                s_ptState = TELEM_PT_MAGIC_0;
            }
            break;

        default:
            s_ptState = TELEM_PT_MAGIC_0;
            break;
    }
}
#endif /* CAR_IS_MASTER */

void CarLink_Init(void)
{
    Serial2_Init();
    CarLink_ResetParser();
#if CAR_IS_MASTER
    s_ptState = TELEM_PT_MAGIC_0;
    s_ptIndex = 0U;
    s_ptRemaining = 0U;
#endif
    s_rxHead = 0U;
    s_rxTail = 0U;
    s_rxCount = 0U;
    s_nextSequence = 0U;
    s_heartbeatTicks = 0U;
    s_peerSilenceTicks = 0U;
    s_peerAlive = 0U;
    s_txDropCount = 0U;
}

void CarLink_Update(uint8_t elapsedTicks)
{
    uint8_t data;

    while (Serial2_ReadByte(&data) != 0U)
    {
        CarLink_ParseByte(data);
#if CAR_IS_MASTER
        /* 同一字节也喂给遥测透传：0xAB 帧会被收齐并原样转发给电脑。 */
        CarLink_TelemPassthruByte(data);
#endif
    }

    /* 周期性发心跳，让对端能判断本机在线。 */
    {
        uint16_t ticks = (uint16_t)s_heartbeatTicks + elapsedTicks;

        if (ticks >= CAR_LINK_HEARTBEAT_TICKS)
        {
            CarLink_SendFrame(CAR_LINK_MSG_HEARTBEAT,
                              CarLink_NextSequence(), NULL, 0U);
            s_heartbeatTicks = 0U;
        }
        else
        {
            s_heartbeatTicks = (uint8_t)ticks;
        }
    }

    /* 掉线判定：超时未收到任何帧则标记对端离线。 */
    if (s_peerAlive != 0U)
    {
        uint16_t silence = s_peerSilenceTicks + elapsedTicks;

        if (silence >= CAR_LINK_PEER_TIMEOUT_TICKS)
        {
            s_peerAlive = 0U;
            s_peerSilenceTicks = CAR_LINK_PEER_TIMEOUT_TICKS;
        }
        else
        {
            s_peerSilenceTicks = silence;
        }
    }
}

uint8_t CarLink_Send(uint8_t type, const uint8_t *payload, uint8_t length)
{
    if ((length > CAR_LINK_MAX_PAYLOAD) ||
        ((length > 0U) && (payload == NULL)))
    {
        return 0U;
    }
    CarLink_SendFrame(type, CarLink_NextSequence(), payload, length);
    return 1U;
}

uint8_t CarLink_PopMessage(CarLink_Message_t *out)
{
    if ((out == NULL) || (s_rxCount == 0U))
    {
        return 0U;
    }

    *out = s_rxQueue[s_rxTail];
    s_rxTail = (uint8_t)((s_rxTail + 1U) % CAR_LINK_RX_QUEUE_LEN);
    s_rxCount--;
    return 1U;
}

uint8_t CarLink_SendCommand(const char *ascii)
{
    uint8_t length = 0U;

    if (ascii == NULL)
    {
        return 0U;
    }
    while ((ascii[length] != '\0') && (length < CAR_LINK_MAX_PAYLOAD))
    {
        length++;
    }
    if ((length == 0U) || (ascii[length] != '\0'))
    {
        /* 空串或超长（未在上限前遇到结尾）都拒绝。 */
        return 0U;
    }
    return CarLink_Send(CAR_LINK_MSG_RELAY_CMD,
                        (const uint8_t *)ascii, length);
}

uint8_t CarLink_SendEvent(uint8_t eventId, const uint8_t *args, uint8_t argLength)
{
    uint8_t payload[CAR_LINK_MAX_PAYLOAD];
    uint8_t index;

    if (argLength > (CAR_LINK_MAX_PAYLOAD - 1U))
    {
        return 0U;
    }
    if ((argLength > 0U) && (args == NULL))
    {
        return 0U;
    }

    payload[0] = eventId;
    for (index = 0U; index < argLength; index++)
    {
        payload[1U + index] = args[index];
    }
    return CarLink_Send(CAR_LINK_MSG_EVENT, payload, (uint8_t)(argLength + 1U));
}

uint8_t CarLink_IsPeerAlive(void)
{
    return s_peerAlive;
}

uint16_t CarLink_GetTxDropCount(void)
{
    return s_txDropCount;
}
