#ifndef APPLICATION_COMMS_K230_LINK_H
#define APPLICATION_COMMS_K230_LINK_H

#include <stdint.h>

/* 与 K230 uart_io.py 完全一致的帧参数。 */
#define K230_LINK_FRAME_MAGIC_0          0xAAU
#define K230_LINK_FRAME_MAGIC_1          0x55U
#define K230_LINK_FRAME_VERSION          0x01U
#define K230_LINK_MAX_PAYLOAD_LENGTH     32U
#define K230_LINK_READY_RETRY_TICKS      10U /* 100 Hz 下每 100 ms 重发。 */

#define K230_LINK_MESSAGE_READY          0x01U
#define K230_LINK_MESSAGE_READY_ACK      0x02U
#define K230_LINK_MESSAGE_TARGET         0x10U

#define K230_LINK_MESSAGE_CAPTURE        0x20U
#define K230_LINK_MESSAGE_CAPTURE_ACK    0x21U

/* 拍照请求的等待上限；100 Hz 下为 1 秒。 */
#define K230_LINK_CAPTURE_TIMEOUT_TICKS  100U
#define K230_LINK_CAPTURE_MAX_COUNT      20U

typedef struct
{
    uint8_t valid;
    int16_t offsetX;
    int16_t offsetY;
    uint8_t sequence;
} K230Link_Target_t;

void K230Link_Init(void);
void K230Link_Update(uint8_t elapsedTicks);
uint8_t K230Link_IsReady(void);
uint8_t K230Link_GetTarget(K230Link_Target_t *target);
uint8_t K230Link_RequestCapture(uint8_t count);
uint8_t K230Link_IsCapturePending(void);
uint8_t K230Link_PopCaptureAck(uint8_t *ok, uint16_t *index);

#endif
