#ifndef SERIAL_H
#define SERIAL_H

/* UART1 连接蓝牙，UART2 连接 F32C 云台；协议解析放在专用硬件层。 */

#include <stdint.h>

#define SERIAL1_RX_BUFFER_SIZE 1024U
#define SERIAL2_RX_BUFFER_SIZE 256U

/* TX 环形缓冲：DMA 从这里搬到 UART，主循环只写不等。二进制遥测一帧可达
 * 数百字节且高频，缓冲要够大以吸收突发；2048 可容纳一帧 schema + 多帧样本。 */
#define SERIAL1_TX_BUFFER_SIZE 2048U

extern volatile uint8_t Serial1_RxFlag;

void Serial1_Init(void);
uint32_t Serial1_Available(void);
uint8_t Serial1_ReadByte(uint8_t *byte);
void Serial1_SendByte(uint8_t byte);
void Serial1_SendArray(const uint8_t *array, uint16_t length);
void Serial1_SendString(const char *string);
void Serial1_Printf(const char *format, ...);
/* DMA 发送完成中断调用，推进环形缓冲的下一段搬运。 */
void Serial1_OnDmaTxComplete(void);
/* TX 缓冲满时丢弃的字节数，用于诊断带宽是否超限。 */
uint32_t Serial1_GetTxDropCount(void);

void Serial2_Init(void);
uint32_t Serial2_Available(void);
uint8_t Serial2_ReadByte(uint8_t *byte);
void Serial2_SendByte(uint8_t byte);
void Serial2_SendArray(const uint8_t *array, uint16_t length);

#endif
