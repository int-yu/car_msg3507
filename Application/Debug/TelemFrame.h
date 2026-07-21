#ifndef APPLICATION_DEBUG_TELEM_FRAME_H
#define APPLICATION_DEBUG_TELEM_FRAME_H

#include <stdint.h>

/*
 * 二进制遥测帧编码。协议规范见 docs/superpowers/specs/2026-07-21-二进制DMA架构-design.md。
 *
 * 帧格式（与 K230Link 同范式，可靠地与 ASCII 命令回应共存于同一 UART）：
 *   0xAA 0x55 | VER | TYPE | SEQ | LEN | PAYLOAD(LEN 字节) | CRC8
 *
 * 0xAA 是非 ASCII 可打印字符，绝不出现在文本回应里，网页据此区分二进制帧与文本行。
 * CRC8 为 CRC-8/ATM（多项式 0x07，初值 0），覆盖 VER 到 PAYLOAD 末字节。
 */

#define TELEM_FRAME_MAGIC_0   0xAAU
#define TELEM_FRAME_MAGIC_1   0x55U
#define TELEM_FRAME_VERSION   0x01U

/* 帧头 6 字节 + CRC 1 字节。整帧最大 = 头 + 255 payload + CRC。 */
#define TELEM_FRAME_HEADER_BYTES 6U
#define TELEM_FRAME_OVERHEAD     (TELEM_FRAME_HEADER_BYTES + 1U)
#define TELEM_FRAME_MAX_PAYLOAD  255U
#define TELEM_FRAME_MAX_BYTES    (TELEM_FRAME_OVERHEAD + TELEM_FRAME_MAX_PAYLOAD)

/* 帧类型。MCU→PC 单向；命令与回应仍走 ASCII，不在此列。 */
#define TELEM_FRAME_TYPE_SCHEMA     0x30U /* 通道表，掩码变化时先发 */
#define TELEM_FRAME_TYPE_SAMPLE     0x31U /* 实时流样本 */
#define TELEM_FRAME_TYPE_CAP_META   0x32U /* 捕获 dump 开始 */
#define TELEM_FRAME_TYPE_CAP_SAMPLE 0x33U /* 捕获样本 */
#define TELEM_FRAME_TYPE_CAP_END    0x34U /* 捕获 dump 结束 */

/* 单位码：网页据此把同量纲通道画在共享刻度上。 */
#define TELEM_UNIT_RAW  0U
#define TELEM_UNIT_MMPS 1U
#define TELEM_UNIT_PWM  2U
#define TELEM_UNIT_DEG  3U
#define TELEM_UNIT_MM   4U
#define TELEM_UNIT_BITS 5U

/* CRC-8/ATM 单字节更新；与 K230Link_Crc8Update 完全一致。 */
uint8_t TelemFrame_Crc8Update(uint8_t crc, uint8_t data);

/*
 * 把一帧组装进 out（至少 TELEM_FRAME_MAX_BYTES 字节），返回总字节数。
 * payload 为 NULL 且 len>0，或 len 超上限时返回 0。
 */
uint16_t TelemFrame_Build(uint8_t *out, uint8_t type, uint8_t seq,
                          const uint8_t *payload, uint8_t len);

/* 把 float 数组按 IEEE754 小端写进 out，返回写入字节数（count*4）。 */
uint16_t TelemFrame_PackFloats(uint8_t *out, const float *values,
                               uint8_t count);

/* 小端写入基础类型，返回写入字节数。供组 payload 用。 */
uint16_t TelemFrame_PackU16(uint8_t *out, uint16_t value);
uint16_t TelemFrame_PackU32(uint8_t *out, uint32_t value);
uint16_t TelemFrame_PackFloat(uint8_t *out, float value);

#endif
