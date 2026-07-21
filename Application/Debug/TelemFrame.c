#include "Application/Debug/TelemFrame.h"
#include <stddef.h>
#include <string.h>

uint8_t TelemFrame_Crc8Update(uint8_t crc, uint8_t data)
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
            crc = (uint8_t)(crc << 1U);
        }
    }
    return crc;
}

uint16_t TelemFrame_PackU16(uint8_t *out, uint16_t value)
{
    out[0] = (uint8_t)(value & 0xFFU);
    out[1] = (uint8_t)((value >> 8U) & 0xFFU);
    return 2U;
}

uint16_t TelemFrame_PackU32(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)(value & 0xFFU);
    out[1] = (uint8_t)((value >> 8U) & 0xFFU);
    out[2] = (uint8_t)((value >> 16U) & 0xFFU);
    out[3] = (uint8_t)((value >> 24U) & 0xFFU);
    return 4U;
}

uint16_t TelemFrame_PackFloat(uint8_t *out, float value)
{
    uint32_t bits;

    /* 按 IEEE754 位模式搬字节；memcpy 避免违反严格别名规则。 */
    (void)memcpy(&bits, &value, sizeof(bits));
    return TelemFrame_PackU32(out, bits);
}

uint16_t TelemFrame_PackFloats(uint8_t *out, const float *values,
                               uint8_t count)
{
    uint16_t offset = 0U;
    uint8_t index;

    if ((values == NULL) && (count > 0U))
    {
        return 0U;
    }
    for (index = 0U; index < count; index++)
    {
        offset += TelemFrame_PackFloat(&out[offset], values[index]);
    }
    return offset;
}

uint16_t TelemFrame_Build(uint8_t *out, uint8_t type, uint8_t seq,
                          const uint8_t *payload, uint8_t len)
{
    uint8_t crc = 0U;
    uint16_t index;

    if ((out == NULL) || ((payload == NULL) && (len > 0U)))
    {
        return 0U;
    }

    out[0] = TELEM_FRAME_MAGIC_0;
    out[1] = TELEM_FRAME_MAGIC_1;
    out[2] = TELEM_FRAME_VERSION;
    out[3] = type;
    out[4] = seq;
    out[5] = len;

    /* CRC 覆盖 VER..PAYLOAD，与 K230Link 一致，帧头两个魔术字节不计入。 */
    for (index = 2U; index < TELEM_FRAME_HEADER_BYTES; index++)
    {
        crc = TelemFrame_Crc8Update(crc, out[index]);
    }
    for (index = 0U; index < len; index++)
    {
        out[TELEM_FRAME_HEADER_BYTES + index] = payload[index];
        crc = TelemFrame_Crc8Update(crc, payload[index]);
    }
    out[TELEM_FRAME_HEADER_BYTES + len] = crc;
    return (uint16_t)(TELEM_FRAME_OVERHEAD + len);
}
