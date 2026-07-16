#ifndef OLED_H
#define OLED_H

#include "Hardware/Display/OLED_Data.h"
#include <stdint.h>

#define OLED_8X16     8U
#define OLED_6X8      6U
#define OLED_UNFILLED 0U
#define OLED_FILLED   1U

void OLED_Init(void);
uint8_t OLED_IsReady(void);
void OLED_Update(void);
void OLED_UpdateArea(int16_t x, int16_t y, uint8_t width, uint8_t height);
void OLED_Clear(void);
void OLED_ClearArea(int16_t x, int16_t y, uint8_t width, uint8_t height);
void OLED_Reverse(void);
void OLED_ReverseArea(int16_t x, int16_t y, uint8_t width, uint8_t height);
void OLED_ShowChar(int16_t x, int16_t y, char value, uint8_t fontSize);
void OLED_ShowString(int16_t x, int16_t y, const char *string, uint8_t fontSize);
void OLED_ShowNum(int16_t x, int16_t y, uint32_t number, uint8_t length, uint8_t fontSize);
void OLED_ShowSignedNum(int16_t x, int16_t y, int32_t number, uint8_t length, uint8_t fontSize);
void OLED_ShowHexNum(int16_t x, int16_t y, uint32_t number, uint8_t length, uint8_t fontSize);
void OLED_ShowBinNum(int16_t x, int16_t y, uint32_t number, uint8_t length, uint8_t fontSize);
void OLED_ShowFloatNum(int16_t x, int16_t y, double number,
                       uint8_t intLength, uint8_t fraLength,
                       uint8_t fontSize);
void OLED_ShowImage(int16_t x, int16_t y, uint8_t width, uint8_t height, const uint8_t *image);
void OLED_Printf(int16_t x, int16_t y, uint8_t fontSize, const char *format, ...);
void OLED_DrawPoint(int16_t x, int16_t y);
uint8_t OLED_GetPoint(int16_t x, int16_t y);
void OLED_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
void OLED_DrawRectangle(int16_t x, int16_t y, uint8_t width, uint8_t height, uint8_t filled);
void OLED_DrawTriangle(int16_t x0, int16_t y0,
                       int16_t x1, int16_t y1,
                       int16_t x2, int16_t y2,
                       uint8_t filled);
void OLED_DrawCircle(int16_t x, int16_t y, uint8_t radius, uint8_t filled);
void OLED_DrawEllipse(int16_t x, int16_t y, uint8_t a, uint8_t b, uint8_t filled);
void OLED_DrawArc(int16_t x, int16_t y, uint8_t radius,
                  int16_t startAngle, int16_t endAngle,
                  uint8_t filled);

#endif
