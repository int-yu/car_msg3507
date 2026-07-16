#ifndef SERIAL_H
#define SERIAL_H

/* UART1 连接蓝牙，UART2 连接 K230；协议解析放在 Application 层。 */

#include <stdint.h>

#define SERIAL1_RX_BUFFER_SIZE 1024U
#define SERIAL2_RX_BUFFER_SIZE 256U

extern volatile uint8_t Serial1_RxFlag;

void Serial1_Init(void);
uint32_t Serial1_Available(void);
uint8_t Serial1_ReadByte(uint8_t *byte);
void Serial1_SendByte(uint8_t byte);
void Serial1_SendArray(const uint8_t *array, uint16_t length);
void Serial1_SendString(const char *string);
void Serial1_Printf(const char *format, ...);

void Serial2_Init(void);
uint32_t Serial2_Available(void);
uint8_t Serial2_ReadByte(uint8_t *byte);
void Serial2_SendByte(uint8_t byte);
void Serial2_SendArray(const uint8_t *array, uint16_t length);

#endif
