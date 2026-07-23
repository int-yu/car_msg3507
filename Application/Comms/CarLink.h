#ifndef APPLICATION_COMMS_CAR_LINK_H
#define APPLICATION_COMMS_CAR_LINK_H

/*
 * 主从对等链路（HC05，跑在 UART2/Serial2 上，DMA 非阻塞发送）。
 *
 * 帧格式与 K230Link 完全同源（已在本工程验证过的范式）：
 *   0xAA 0x55 | 版本 | 类型 | 序号 | 长度 | 载荷... | CRC8
 * 用 0xAA 0x55 双字节魔数起头，抗噪；CRC8(多项式 0x07) 覆盖 版本..载荷。
 *
 * 对称设计：主车、从车跑同一份 CarLink，两边都能主动 CarLink_Send()。
 * 身份差异（谁转发命令 / 谁触发事件）在 App 层用 CAR_IS_MASTER/SLAVE 区分，
 * 链路层不关心谁是主谁是从。
 *
 * 【怎么加一种新的传输内容】——这是本模块最想让你以后好改的地方：
 *   1. 在 CarLink_MsgType_t 里加一个新的类型值（避开已用的）。
 *   2. 发送端：调用 CarLink_Send(新类型, 载荷指针, 载荷长度)。
 *   3. 接收端：在 App_HandlePeerMessage()（App.c）里加一个 case。
 *   帧的封装/解析/CRC/DMA 全不用动。
 */

#include <stdint.h>

#define CAR_LINK_FRAME_MAGIC_0    0xAAU
#define CAR_LINK_FRAME_MAGIC_1    0x55U
#define CAR_LINK_FRAME_VERSION    0x01U
#define CAR_LINK_MAX_PAYLOAD      32U

/* 从车遥测帧的首字节（与控制帧 0xAA 区分）。从车遥测帧格式与主车遥测完全相同，
 * 只是首字节换成 0xAB；主车在链路上按此识别并原样透传给电脑，不进控制帧解析。 */
#define CAR_LINK_TELEM_MAGIC      0xABU

/* 心跳间隔与对端存活超时（100 Hz 系统拍）。心跳只用于判断链路通不通，
 * 不进消息队列，不打扰上层。 */
#define CAR_LINK_HEARTBEAT_TICKS  50U  /* 每 0.5 s 发一次心跳 */
#define CAR_LINK_PEER_TIMEOUT_TICKS 200U /* 2 s 收不到任何帧即判对端掉线 */

/* 消息类型。0x01..0x0F 留给链路自身管理帧，0x10 起是业务消息。 */
typedef enum
{
    CAR_LINK_MSG_HEARTBEAT   = 0x01U, /* 无载荷，保活/探测对端 */
    CAR_LINK_MSG_RELAY_CMD   = 0x10U, /* 载荷=ASCII 运动命令串（网页→主机→从机） */
    CAR_LINK_MSG_RELAY_REPLY = 0x11U, /* 载荷=ASCII 回应文本（从机执行结果→主机→网页） */
    CAR_LINK_MSG_EVENT       = 0x20U, /* 载荷[0]=事件号，其余为参数（主机条件触发→从机） */
    CAR_LINK_MSG_ACK         = 0x30U  /* 载荷[0]=被确认的类型或事件号 */
    /* ↑ 在此继续添加你自己的业务类型（如状态上报、传感器数据…） */
} CarLink_MsgType_t;

typedef struct
{
    uint8_t type;
    uint8_t length;
    uint8_t payload[CAR_LINK_MAX_PAYLOAD];
} CarLink_Message_t;

void CarLink_Init(void);
void CarLink_Update(uint8_t elapsedTicks);

/* 通用发送：任何类型 + 任意载荷。载荷可为 NULL（长度 0）。
 * 成功返回 1；参数非法或 TX 缓冲满返回 0（不阻塞）。 */
uint8_t CarLink_Send(uint8_t type, const uint8_t *payload, uint8_t length);

/* 通用接收：从收帧队列取一条业务消息（心跳不入队）。有则填入 out 返回 1。 */
uint8_t CarLink_PopMessage(CarLink_Message_t *out);

/* 便捷封装 -------------------------------------------------------------- */

/* 把一条 ASCII 运动命令（如 "J100/100"）打包成 RELAY_CMD 发给对端。
 * 主机转发网页命令时用；命令串不含 0xAA 起头字节即可，超长返回 0。 */
uint8_t CarLink_SendCommand(const char *ascii);

/* 发一个事件：eventId + 可选参数。主机在“满足某条件”时同步给从机用。 */
uint8_t CarLink_SendEvent(uint8_t eventId, const uint8_t *args, uint8_t argLength);

/* 链路是否有效：最近 CAR_LINK_PEER_TIMEOUT_TICKS 内收到过对端任意帧。 */
uint8_t CarLink_IsPeerAlive(void);

/* TX 帧因缓冲满被丢弃的次数，用于诊断链路是否过载。 */
uint16_t CarLink_GetTxDropCount(void);

#endif
