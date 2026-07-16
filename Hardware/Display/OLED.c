#include "Hardware/Display/OLED.h"
#include "System/Delay.h"
#include "ti_msp_dl_config.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OLED_ADDRESS     0x3CU
#define OLED_TIMEOUT     100000UL
#define OLED_CHUNK_BYTES 7U

static uint8_t s_buffer[8][128];
static uint8_t s_ready;
static uint8_t s_busError;

static uint8_t OLED_Transfer(const uint8_t *data, uint8_t length)
{
    uint32_t timeout = OLED_TIMEOUT;
    while (((DL_I2C_getControllerStatus(OLED_I2C_INST) & DL_I2C_CONTROLLER_STATUS_IDLE) == 0U) &&
           (timeout-- > 0U)) {}
    if (timeout == 0U) return 0U;

    DL_I2C_flushControllerTXFIFO(OLED_I2C_INST);
    DL_I2C_fillControllerTXFIFO(OLED_I2C_INST, data, length);
    DL_I2C_startControllerTransfer(OLED_I2C_INST, OLED_ADDRESS,
                                   DL_I2C_CONTROLLER_DIRECTION_TX, length);
    delay_cycles(16U);
    timeout = OLED_TIMEOUT;
    while (((DL_I2C_getControllerStatus(OLED_I2C_INST) & DL_I2C_CONTROLLER_STATUS_BUSY) != 0U) &&
           (timeout-- > 0U)) {}
    if ((timeout == 0U) || ((DL_I2C_getControllerStatus(OLED_I2C_INST) &
                             DL_I2C_CONTROLLER_STATUS_ERROR) != 0U))
    {
        DL_I2C_resetControllerTransfer(OLED_I2C_INST);
        return 0U;
    }
    return 1U;
}

static void OLED_Command(uint8_t command)
{
    uint8_t packet[2] = {0x00U, command};
    if (OLED_Transfer(packet, 2U) == 0U) s_busError = 1U;
}

static void OLED_Data(const uint8_t *data, uint8_t count)
{
    uint8_t packet[OLED_CHUNK_BYTES + 1U];
    uint8_t offset = 0U;
    packet[0] = 0x40U;
    while (offset < count)
    {
        uint8_t i;
        uint8_t chunk = (uint8_t)(count - offset);
        if (chunk > OLED_CHUNK_BYTES) chunk = OLED_CHUNK_BYTES;
        for (i = 0U; i < chunk; i++) packet[i + 1U] = data[offset + i];
        if (OLED_Transfer(packet, (uint8_t)(chunk + 1U)) == 0U)
        {
            s_busError = 1U;
            return;
        }
        offset = (uint8_t)(offset + chunk);
    }
}

static void OLED_SetCursor(uint8_t page, uint8_t x)
{
    OLED_Command((uint8_t)(0xB0U | page));
    OLED_Command((uint8_t)(0x10U | ((x & 0xF0U) >> 4)));
    OLED_Command((uint8_t)(x & 0x0FU));
}

void OLED_Init(void)
{
    static const uint8_t commands[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
        0xA1, 0xC8, 0xDA, 0x12, 0x81, 0xCF, 0xD9, 0xF1,
        0xDB, 0x30, 0xA4, 0xA6, 0x8D, 0x14, 0xAF
    };
    uint8_t i;
    Delay_ms(100U);
    s_busError = 0U;
    for (i = 0U; i < (uint8_t)sizeof(commands); i++) OLED_Command(commands[i]);
    s_ready = (s_busError == 0U) ? 1U : 0U;
    OLED_Clear();
    if (s_ready != 0U) OLED_Update();
}

uint8_t OLED_IsReady(void) { return s_ready; }

void OLED_Update(void)
{
    uint8_t page;
    if (s_ready == 0U) return;
    for (page = 0U; page < 8U; page++)
    {
        OLED_SetCursor(page, 0U);
        OLED_Data(s_buffer[page], 128U);
    }
    if (s_busError != 0U) s_ready = 0U;
}

void OLED_UpdateArea(int16_t x, int16_t y, uint8_t width, uint8_t height)
{
    int16_t firstPage = y / 8;
    int16_t lastPage = (y + height - 1) / 8;
    int16_t page;
    if ((x < 0) || (x > 127) || (width == 0U) || (height == 0U)) return;
    if ((x + width) > 128) width = (uint8_t)(128 - x);
    if (firstPage < 0) firstPage = 0;
    if (lastPage > 7) lastPage = 7;
    for (page = firstPage; page <= lastPage; page++)
    {
        OLED_SetCursor((uint8_t)page, (uint8_t)x);
        OLED_Data(&s_buffer[page][x], width);
    }
}

void OLED_Clear(void) { memset(s_buffer, 0, sizeof(s_buffer)); }

void OLED_ClearArea(int16_t x, int16_t y, uint8_t width, uint8_t height)
{
    int16_t px;
    int16_t py;
    for (py = y; py < (int16_t)(y + height); py++)
        for (px = x; px < (int16_t)(x + width); px++)
            if ((px >= 0) && (px < 128) && (py >= 0) && (py < 64))
                s_buffer[py / 8][px] &= (uint8_t)~(1U << (py & 7));
}

void OLED_Reverse(void)
{
    uint16_t i;
    for (i = 0U; i < sizeof(s_buffer); i++) ((uint8_t *)s_buffer)[i] ^= 0xFFU;
}

void OLED_ReverseArea(int16_t x, int16_t y, uint8_t width, uint8_t height)
{
    int16_t px;
    int16_t py;
    for (py = y; py < (int16_t)(y + height); py++)
        for (px = x; px < (int16_t)(x + width); px++)
            if ((px >= 0) && (px < 128) && (py >= 0) && (py < 64))
                s_buffer[py / 8][px] ^= (uint8_t)(1U << (py & 7));
}

void OLED_DrawPoint(int16_t x, int16_t y)
{
    if ((x >= 0) && (x < 128) && (y >= 0) && (y < 64))
        s_buffer[y / 8][x] |= (uint8_t)(1U << (y & 7));
}

uint8_t OLED_GetPoint(int16_t x, int16_t y)
{
    if ((x < 0) || (x >= 128) || (y < 0) || (y >= 64)) return 0U;
    return ((s_buffer[y / 8][x] & (1U << (y & 7))) != 0U) ? 1U : 0U;
}

void OLED_ShowImage(int16_t x, int16_t y, uint8_t width, uint8_t height, const uint8_t *image)
{
    uint8_t px;
    uint8_t py;
    if (image == NULL) return;
    OLED_ClearArea(x, y, width, height);
    for (py = 0U; py < height; py++)
        for (px = 0U; px < width; px++)
            if ((image[(py / 8U) * width + px] & (1U << (py & 7U))) != 0U)
                OLED_DrawPoint((int16_t)(x + px), (int16_t)(y + py));
}

void OLED_ShowChar(int16_t x, int16_t y, char value, uint8_t fontSize)
{
    uint8_t index = ((value >= ' ') && (value <= '~')) ?
                        (uint8_t)(value - ' ') :
                        (uint8_t)('?' - ' ');
    if (fontSize == OLED_8X16) OLED_ShowImage(x, y, 8U, 16U, OLED_F8x16[index]);
    else OLED_ShowImage(x, y, 6U, 8U, OLED_F6x8[index]);
}

void OLED_ShowString(int16_t x, int16_t y, const char *string, uint8_t fontSize)
{
    int16_t offset = 0;
    if (string == NULL) return;
    while (*string != '\0')
    {
        if (((uint8_t)*string & 0x80U) != 0U)
        {
            OLED_ShowChar((int16_t)(x + offset), y, '?', fontSize);
            while ((((uint8_t)string[1] & 0xC0U) == 0x80U)) string++;
        }
        else OLED_ShowChar((int16_t)(x + offset), y, *string, fontSize);
        offset = (int16_t)(offset + fontSize);
        string++;
    }
}

static uint32_t OLED_Pow10(uint8_t power)
{
    uint32_t value = 1U;
    while (power-- > 0U) value *= 10U;
    return value;
}

void OLED_ShowNum(int16_t x, int16_t y, uint32_t number, uint8_t length, uint8_t fontSize)
{
    uint8_t i;
    for (i = 0U; i < length; i++)
        OLED_ShowChar(
            (int16_t)(x + i * fontSize), y,
            (char)('0' +
                   (number / OLED_Pow10((uint8_t)(length - i - 1U))) % 10U),
            fontSize);
}

void OLED_ShowSignedNum(int16_t x, int16_t y, int32_t number, uint8_t length, uint8_t fontSize)
{
    uint32_t magnitude = (number < 0) ?
                             (uint32_t)(-(number + 1)) + 1U :
                             (uint32_t)number;
    OLED_ShowChar(x, y, (number < 0) ? '-' : '+', fontSize);
    OLED_ShowNum((int16_t)(x + fontSize), y, magnitude, length, fontSize);
}

void OLED_ShowHexNum(int16_t x, int16_t y, uint32_t number, uint8_t length, uint8_t fontSize)
{
    int8_t i;
    for (i = (int8_t)length - 1; i >= 0; i--)
    {
        uint8_t digit = (uint8_t)((number >> (4U * i)) & 0x0FU);
        OLED_ShowChar((int16_t)(x + ((int8_t)length - 1 - i) * fontSize), y,
                      (char)((digit < 10U) ? ('0' + digit) : ('A' + digit - 10U)), fontSize);
    }
}

void OLED_ShowBinNum(int16_t x, int16_t y, uint32_t number, uint8_t length, uint8_t fontSize)
{
    uint8_t i;
    for (i = 0U; i < length; i++)
        OLED_ShowChar((int16_t)(x + i * fontSize), y,
                      (char)('0' + ((number >> (length - i - 1U)) & 1U)), fontSize);
}

void OLED_ShowFloatNum(int16_t x, int16_t y, double number,
                       uint8_t intLength, uint8_t fraLength,
                       uint8_t fontSize)
{
    char format[12];
    char value[32];
    (void)snprintf(format, sizeof(format), "%%+%u.%uf",
                   (unsigned)(intLength + fraLength + 2U),
                   (unsigned)fraLength);
    (void)snprintf(value, sizeof(value), format, number);
    OLED_ShowString(x, y, value, fontSize);
}

void OLED_Printf(int16_t x, int16_t y, uint8_t fontSize, const char *format, ...)
{
    char value[192];
    va_list args;
    va_start(args, format);
    (void)vsnprintf(value, sizeof(value), format, args);
    va_end(args);
    OLED_ShowString(x, y, value, fontSize);
}

void OLED_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    int16_t dx = (int16_t)abs(x1 - x0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t dy = (int16_t)-abs(y1 - y0);
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t error = (int16_t)(dx + dy);
    for (;;)
    {
        int16_t twice;
        OLED_DrawPoint(x0, y0);
        if ((x0 == x1) && (y0 == y1)) break;
        twice = (int16_t)(2 * error);
        if (twice >= dy) { error = (int16_t)(error + dy); x0 = (int16_t)(x0 + sx); }
        if (twice <= dx) { error = (int16_t)(error + dx); y0 = (int16_t)(y0 + sy); }
    }
}

void OLED_DrawRectangle(int16_t x, int16_t y,
                        uint8_t width, uint8_t height,
                        uint8_t filled)
{
    uint8_t row;
    if (filled != 0U)
    {
        for (row = 0U; row < height; row++)
        {
            OLED_DrawLine(x, (int16_t)(y + row),
                          (int16_t)(x + width - 1),
                          (int16_t)(y + row));
        }
    }
    else
    {
        OLED_DrawLine(x, y, (int16_t)(x + width - 1), y);
        OLED_DrawLine(x, (int16_t)(y + height - 1),
                      (int16_t)(x + width - 1),
                      (int16_t)(y + height - 1));
        OLED_DrawLine(x, y, x, (int16_t)(y + height - 1));
        OLED_DrawLine((int16_t)(x + width - 1), y,
                      (int16_t)(x + width - 1),
                      (int16_t)(y + height - 1));
    }
}

static int32_t OLED_Edge(int16_t ax, int16_t ay,
                         int16_t bx, int16_t by,
                         int16_t px, int16_t py)
{
    return (int32_t)(px - ax) * (by - ay) - (int32_t)(py - ay) * (bx - ax);
}

void OLED_DrawTriangle(int16_t x0, int16_t y0,
                       int16_t x1, int16_t y1,
                       int16_t x2, int16_t y2,
                       uint8_t filled)
{
    if (filled == 0U)
    {
        OLED_DrawLine(x0, y0, x1, y1);
        OLED_DrawLine(x1, y1, x2, y2);
        OLED_DrawLine(x2, y2, x0, y0);
    }
    else
    {
        int16_t x;
        int16_t y;
        int16_t minX = (int16_t)fminf((float)x0, fminf((float)x1, (float)x2));
        int16_t maxX = (int16_t)fmaxf((float)x0, fmaxf((float)x1, (float)x2));
        int16_t minY = (int16_t)fminf((float)y0, fminf((float)y1, (float)y2));
        int16_t maxY = (int16_t)fmaxf((float)y0, fmaxf((float)y1, (float)y2));
        for (y = minY; y <= maxY; y++) for (x = minX; x <= maxX; x++)
        {
            int32_t a = OLED_Edge(x0, y0, x1, y1, x, y);
            int32_t b = OLED_Edge(x1, y1, x2, y2, x, y);
            int32_t c = OLED_Edge(x2, y2, x0, y0, x, y);
            if (((a >= 0) && (b >= 0) && (c >= 0)) ||
                ((a <= 0) && (b <= 0) && (c <= 0)))
            {
                OLED_DrawPoint(x, y);
            }
        }
    }
}

void OLED_DrawCircle(int16_t x, int16_t y, uint8_t radius, uint8_t filled)
{
    int16_t px;
    int16_t py;
    int16_t rr = (int16_t)(radius * radius);
    for (py = -(int16_t)radius; py <= radius; py++) for (px = -(int16_t)radius; px <= radius; px++)
    {
        int16_t d = (int16_t)(px * px + py * py);
        if ((filled != 0U && d <= rr) || (filled == 0U && (d >= rr - radius) && (d <= rr + radius)))
            OLED_DrawPoint((int16_t)(x + px), (int16_t)(y + py));
    }
}

void OLED_DrawEllipse(int16_t x, int16_t y, uint8_t a, uint8_t b, uint8_t filled)
{
    int16_t px;
    int16_t py;
    float aa = (float)a * a;
    float bb = (float)b * b;
    for (py = -(int16_t)b; py <= b; py++) for (px = -(int16_t)a; px <= a; px++)
    {
        float d = ((float)px * px) / aa + ((float)py * py) / bb;
        if (((filled != 0U) && (d <= 1.0f)) ||
            ((filled == 0U) && (d >= 0.90f) && (d <= 1.10f)))
            OLED_DrawPoint((int16_t)(x + px), (int16_t)(y + py));
    }
}

void OLED_DrawArc(int16_t x, int16_t y, uint8_t radius,
                  int16_t startAngle, int16_t endAngle,
                  uint8_t filled)
{
    int16_t angle;
    if (endAngle < startAngle) endAngle = (int16_t)(endAngle + 360);
    for (angle = startAngle; angle <= endAngle; angle++)
    {
        float rad = (float)angle * 3.1415926f / 180.0f;
        int16_t px = (int16_t)(x + cosf(rad) * radius);
        int16_t py = (int16_t)(y + sinf(rad) * radius);
        if (filled != 0U) OLED_DrawLine(x, y, px, py);
        else OLED_DrawPoint(px, py);
    }
}
