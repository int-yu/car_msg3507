#ifndef SERIAL_H
#define SERIAL_H

/*
 * Serial1 = BLUETOOTH_UART = UART2 / PA21(TX)、PA22(RX)
 *           -> DAPLink / PC / 网页。
 * Serial2 = 当前步进测试配置中停用，UART1未实例化。
 * Serial3 = K230_UART = UART0 / PA10(TX)、PA11(RX)
 *           -> K230 视觉链路。
 * 协议解析放在各自的专用应用层（BluetoothDebug / CarLink / K230Link）。
 */

#include <stdint.h>

#define SERIAL1_RX_BUFFER_SIZE 1024U
#define SERIAL2_RX_BUFFER_SIZE 256U
#define SERIAL3_RX_BUFFER_SIZE 256U

/* TX 环形缓冲：DMA 从这里搬到 UART，主循环只写不等。二进制遥测一帧可达
 * 数百字节且高频，缓冲要够大以吸收突发；2048 可容纳一帧 schema + 多帧样本。 */
#define SERIAL1_TX_BUFFER_SIZE 2048U

/* 主从链路帧短、频率低，512 足以吸收突发（一帧最多 7+32 字节）。 */
#define SERIAL2_TX_BUFFER_SIZE 512U

/* K230 帧最长 39 字节；256 字节可覆盖多帧突发。 */
#define SERIAL3_TX_BUFFER_SIZE 256U

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

/*
 * K230 独立 UART。RX 环形缓冲写满时丢弃新字节并累计 overflow 计数，解析器
 * 靠 AA 55 同步头自愈；不熔断 RX 中断，避免瞬时拥塞永久掉线。
 */
void Serial3_Init(void);
uint32_t Serial3_Available(void);
uint8_t Serial3_ReadByte(uint8_t *byte);
void Serial3_SendByte(uint8_t byte);
uint8_t Serial3_SendArray(const uint8_t *array, uint16_t length);
void Serial3_OnDmaTxComplete(void);
uint32_t Serial3_GetTxDropCount(void);
uint32_t Serial3_GetRxOverflowCount(void);

#endif
