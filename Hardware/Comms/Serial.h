#ifndef SERIAL_H
#define SERIAL_H

/* UART1 连接蓝牙调试台，UART2 连接 F32C 云台。协议解析由应用层完成。 */

#include <stdint.h>

#define SERIAL1_RX_BUFFER_SIZE 1024U
#define SERIAL1_TX_BUFFER_SIZE 256U
#define SERIAL2_RX_BUFFER_SIZE 256U

extern volatile uint8_t Serial1_RxFlag;

void Serial1_Init(void);
uint32_t Serial1_Available(void);
uint8_t Serial1_ReadByte(uint8_t *byte);
/* 将完整数据加入 TX 队列；空间不足时不写入并返回 0。 */
uint8_t Serial1_QueueArray(const uint8_t *array, uint16_t length);

void Serial2_Init(void);
uint32_t Serial2_Available(void);
uint8_t Serial2_ReadByte(uint8_t *byte);
void Serial2_SendByte(uint8_t byte);
void Serial2_SendArray(const uint8_t *array, uint16_t length);

#endif
