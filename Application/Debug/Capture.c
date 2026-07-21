#include "Application/Debug/Capture.h"
#include "Application/Debug/TelemFrame.h"
#include "Application/Debug/Telemetry.h"
#include "Hardware/Comms/Serial.h"
#include <string.h>

/* 每样本 = 时间戳 + 各通道 float32。 */
#define CAPTURE_SAMPLE_BYTES(channels) \
    (sizeof(uint32_t) + ((uint32_t)(channels) * sizeof(float)))

/* 一次 DumpNext 发送的样本帧数。dump 时车已停稳，且 TX 已 DMA 化，
 * 但仍分批避免一次性把上千帧塞满 TX 缓冲导致丢弃。 */
#define CAPTURE_DUMP_FRAMES_PER_TICK 8U

static uint8_t s_buffer[CAPTURE_BUFFER_BYTES];
static Capture_State_t s_state;
static uint16_t s_channelMask;
static uint8_t s_channelCount;
static uint16_t s_sampleCount;
static uint16_t s_capacity;
static uint16_t s_dumpIndex;
static uint32_t s_elapsedMs;
static uint8_t s_sequence;

static uint8_t Capture_CountChannels(uint16_t mask)
{
    uint8_t count = 0U;
    uint16_t bit;

    for (bit = TELEMETRY_CH_TL; bit <= TELEMETRY_CH_RD; bit <<= 1U)
    {
        if ((mask & bit) != 0U)
        {
            count++;
        }
    }
    return count;
}

void Capture_Init(void)
{
    s_state = CAPTURE_STATE_IDLE;
    s_channelMask = 0U;
    s_channelCount = 0U;
    s_sampleCount = 0U;
    s_capacity = 0U;
    s_dumpIndex = 0U;
    s_elapsedMs = 0U;
    s_sequence = 0U;
}

uint8_t Capture_Arm(uint16_t channelMask)
{
    uint8_t count;

    if (s_state == CAPTURE_STATE_DUMPING)
    {
        return 0U;
    }
    if ((channelMask == 0U) || ((channelMask & ~TELEMETRY_CH_ALL) != 0U))
    {
        return 0U;
    }

    count = Capture_CountChannels(channelMask);
    if ((count == 0U) || (count > CAPTURE_MAX_CHANNELS))
    {
        return 0U;
    }

    s_channelMask = channelMask;
    s_channelCount = count;
    s_capacity = (uint16_t)((uint32_t)CAPTURE_BUFFER_BYTES /
                            CAPTURE_SAMPLE_BYTES(count));
    s_sampleCount = 0U;
    s_dumpIndex = 0U;
    s_elapsedMs = 0U;
    s_state = CAPTURE_STATE_ARMED;
    return 1U;
}

void Capture_Trigger(void)
{
    if (s_state != CAPTURE_STATE_ARMED)
    {
        return;
    }
    s_sampleCount = 0U;
    s_elapsedMs = 0U;
    s_state = CAPTURE_STATE_RUNNING;
}

void Capture_Update(uint32_t elapsedMs)
{
    uint32_t offset;
    float values[CAPTURE_MAX_CHANNELS];
    uint8_t count;

    if (s_state != CAPTURE_STATE_RUNNING)
    {
        return;
    }

    s_elapsedMs += elapsedMs;
    if (s_sampleCount >= s_capacity)
    {
        /* 写满即停：继续覆盖会让时间轴失去连续性，分析阶跃时更麻烦。 */
        s_state = CAPTURE_STATE_FULL;
        return;
    }

    offset = (uint32_t)s_sampleCount * CAPTURE_SAMPLE_BYTES(s_channelCount);

    /* 逐字节写时间戳与各通道 float，不做 4 字节对齐假设：样本大小随通道数变化，
     * 直接按 float* 解引用会在奇数偏移触发 Cortex-M0+ 非对齐访问异常。 */
    {
        uint8_t tmp[4];
        (void)TelemFrame_PackU32(tmp, s_elapsedMs);
        (void)memcpy(&s_buffer[offset], tmp, 4U);
        offset += 4U;
    }

    /* 复用 Telemetry 的通道表取值，保证与实时流的列语义完全一致。 */
    count = Telemetry_SampleChannels(s_channelMask, values);
    {
        uint8_t index;
        for (index = 0U; index < count; index++)
        {
            uint8_t tmp[4];
            (void)TelemFrame_PackFloat(tmp, values[index]);
            (void)memcpy(&s_buffer[offset], tmp, 4U);
            offset += 4U;
        }
    }
    s_sampleCount++;
}

void Capture_Stop(void)
{
    if ((s_state == CAPTURE_STATE_RUNNING) ||
        (s_state == CAPTURE_STATE_ARMED))
    {
        s_state = (s_sampleCount > 0U) ?
            CAPTURE_STATE_FULL : CAPTURE_STATE_IDLE;
    }
}

uint16_t Capture_StartDump(void)
{
    uint8_t payload[6];
    uint8_t frame[TELEM_FRAME_MAX_BYTES];
    uint16_t offset = 0U;
    uint16_t frameLength;

    if ((s_state == CAPTURE_STATE_DUMPING) || (s_sampleCount == 0U))
    {
        return 0U;
    }

    /* 先发 SCHEMA（复用 Telemetry 的列名/单位表），网页据此解析后续样本；
     * 再发 CAP_META 告知样本数与周期。 */
    Telemetry_SendSchema(s_channelMask, TELEM_FRAME_TYPE_CAP_META);

    offset += TelemFrame_PackU16(&payload[offset], s_channelMask);
    offset += TelemFrame_PackU16(&payload[offset], s_sampleCount);
    offset += TelemFrame_PackU16(&payload[offset], 10U); /* 周期 ms，100 Hz */
    frameLength = TelemFrame_Build(frame, TELEM_FRAME_TYPE_CAP_META,
                                   s_sequence++, payload, (uint8_t)offset);
    Serial1_SendArray(frame, frameLength);

    s_dumpIndex = 0U;
    s_state = CAPTURE_STATE_DUMPING;
    return s_sampleCount;
}

void Capture_DumpNext(void)
{
    uint8_t batch;

    if (s_state != CAPTURE_STATE_DUMPING)
    {
        return;
    }

    for (batch = 0U; batch < CAPTURE_DUMP_FRAMES_PER_TICK; batch++)
    {
        uint32_t offset;
        uint16_t payloadLen;
        uint8_t frame[TELEM_FRAME_MAX_BYTES];
        uint16_t frameLength;

        if (s_dumpIndex >= s_sampleCount)
        {
            uint8_t endPayload[2];
            (void)TelemFrame_PackU16(endPayload, s_sampleCount);
            frameLength = TelemFrame_Build(frame, TELEM_FRAME_TYPE_CAP_END,
                                           s_sequence++, endPayload, 2U);
            Serial1_SendArray(frame, frameLength);
            s_state = CAPTURE_STATE_IDLE;
            return;
        }

        /* 样本在缓冲里已是「ms + floats」的连续二进制，正好就是 CAP_SAMPLE 的
         * payload，直接原样引用，无需重新编码。 */
        offset = (uint32_t)s_dumpIndex *
                 CAPTURE_SAMPLE_BYTES(s_channelCount);
        payloadLen = (uint16_t)CAPTURE_SAMPLE_BYTES(s_channelCount);
        frameLength = TelemFrame_Build(frame, TELEM_FRAME_TYPE_CAP_SAMPLE,
                                       s_sequence++, &s_buffer[offset],
                                       (uint8_t)payloadLen);
        Serial1_SendArray(frame, frameLength);
        s_dumpIndex++;
    }
}

Capture_State_t Capture_GetState(void)
{
    return s_state;
}

uint16_t Capture_GetSampleCount(void)
{
    return s_sampleCount;
}

uint16_t Capture_GetCapacity(void)
{
    return s_capacity;
}

uint16_t Capture_GetChannelMask(void)
{
    return s_channelMask;
}
