#ifndef SERIAL_H
#define SERIAL_H

/*
 * UART1 连接蓝牙（BLOOTH → 无线 daplink → 电脑/网页）。
 * UART2（PA21/PA22）原为 F32C 云台/K230，现改接 HC05 主从链路（CarLink）；
 *   F32C/K230 代码保留但暂时停用（见 App.c），等挪到新串口再启用。
 * 协议解析放在各自的专用应用层（BluetoothDebug / CarLink）。
 */

#include <stdint.h>

#define SERIAL1_RX_BUFFER_SIZE 1024U
#define SERIAL2_RX_BUFFER_SIZE 256U

/* TX 环形缓冲：DMA 从这里搬到 UART，主循环只写不等。二进制遥测一帧可达
 * 数百字节且高频，缓冲要够大以吸收突发；2048 可容纳一帧 schema + 多帧样本。 */
#define SERIAL1_TX_BUFFER_SIZE 2048U

/* 主从链路帧短、频率低，512 足以吸收突发（一帧最多 7+32 字节）。 */
#define SERIAL2_TX_BUFFER_SIZE 512U

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

/*
 * 输出捕获（从机用）。BeginCapture 后，Serial1 的所有输出改写入 buf（不发往
 * UART）；EndCapture 返回捕获到的字节数并恢复正常发送。从机借此把“执行转发
 * 命令产生的 OK/ERR 回应”收集起来，经 CarLink 回传给主机。仅在主循环调用，
 * 不可嵌套。
 */
void Serial1_BeginCapture(uint8_t *buffer, uint16_t capacity);
uint16_t Serial1_EndCapture(void);

void Serial2_Init(void);
uint32_t Serial2_Available(void);
uint8_t Serial2_ReadByte(uint8_t *byte);
void Serial2_SendByte(uint8_t byte);
/* DMA 非阻塞发送：缓冲够则整段入环形缓冲返回 1；满则整段丢弃返回 0（绝不阻塞）。
 * 整段全进或全不进，避免对端收到半帧。 */
uint8_t Serial2_SendArray(const uint8_t *array, uint16_t length);
/* DMA 发送完成中断调用，推进环形缓冲的下一段搬运。 */
void Serial2_OnDmaTxComplete(void);
/* TX 缓冲满时丢弃的字节数，用于诊断链路带宽是否超限。 */
uint32_t Serial2_GetTxDropCount(void);

#endif
