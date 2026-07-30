#ifndef APPLICATION_COMMS_K230_LINK_H
#define APPLICATION_COMMS_K230_LINK_H

#include <stdint.h>

/* 与 K230 uart_io.py 完全一致的帧参数。 */
#define K230_LINK_FRAME_MAGIC_0          0xAAU
#define K230_LINK_FRAME_MAGIC_1          0x55U
#define K230_LINK_FRAME_VERSION          0x01U
#define K230_LINK_MAX_PAYLOAD_LENGTH     32U
#define K230_LINK_READY_RETRY_TICKS      10U /* 100 Hz 下每 100 ms 重发。 */
/* 每个 10 ms 控制拍最多解析 128 字节，覆盖 115200 baud 满线速且保证执行有界。 */
#define K230_LINK_RX_BUDGET_BYTES        128U

#define K230_LINK_MESSAGE_READY          0x01U
#define K230_LINK_MESSAGE_READY_ACK      0x02U
#define K230_LINK_MESSAGE_TARGET         0x10U
#define K230_LINK_MESSAGE_LANE           0x13U

/* TARGET 的新鲜度上限：100 Hz 下 200 ms。钢球平衡是双积分对象，拿冻结
 * 的球位算 PD 会让倾角锁死、球一路加速滚到挡片，因此比 LANE 的 300 ms
 * 更紧。BallBalance 另有一道更严的判据，两者层次不同。 */
#define K230_LINK_TARGET_TIMEOUT_TICKS   20U

/* LANE 的 PAYLOAD：valid:u8 | b0..b4:int16_LE | confidence:u8。
 * b0 最近、b4 最远，单位是千分比（相对画面宽度），符号与 TARGET 一致：
 * 画面中心 - 车道中心，车道偏右时为负。 */
#define K230_LINK_LANE_BAND_COUNT        5U
#define K230_LINK_LANE_PAYLOAD_LENGTH    12U

/* 某一带没有被中心线覆盖时 K230 填这个哨兵。不能用 0——0 与「车道中心
 * 正好位于画面中央」无法区分。 */
#define K230_LINK_LANE_OFFSET_INVALID    ((int16_t)-32768)

/* 链路层的新鲜度上限：100 Hz 下 300 ms，K230 约 25 fps，允许连续丢
 * 7 帧左右再判整条链路失效（此后 K230Link_GetLane 返回 0）。这只管
 * "数据还在不在"，不代表控制层可以放心用到 300 ms 这么旧的数据——
 * MotionLane 自己另有一道更紧的 MOTION_LANE_MAX_LANE_AGE_TICKS 把关，
 * 两者是不同层次的超时，不要因为数值一样就以为总盲开时间是 300 ms。 */
#define K230_LINK_LANE_TIMEOUT_TICKS     30U

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
    uint8_t ageTicks;    /* 距最近一帧的控制拍数，饱和在超时阈值。 */
} K230Link_Target_t;

typedef struct
{
    uint8_t valid;
    int16_t offsetPermille[K230_LINK_LANE_BAND_COUNT];
    uint8_t bandValid;   /* bit0..bit4，1 表示该带不是哨兵。 */
    uint8_t confidence;  /* 0..100。 */
    uint8_t sequence;
    uint8_t ageTicks;    /* 距最近一帧的控制拍数，饱和在超时阈值。 */
} K230Link_Lane_t;

void K230Link_Init(void);
void K230Link_Update(uint8_t elapsedTicks);
uint8_t K230Link_IsReady(void);
/* 从未收到或已超时返回 0，此时不写 *target。调用方必须按视觉失效处理，
 * 绝不能把上一帧的球位当成当前有效数据继续做闭环。 */
uint8_t K230Link_GetTarget(K230Link_Target_t *target);
/* 从未收到或已超时返回 0，此时不写 *lane。调用方必须按视觉失效处理，
 * 绝不能把上一帧的偏差当成当前有效数据继续用。 */
uint8_t K230Link_GetLane(K230Link_Lane_t *lane);
uint8_t K230Link_RequestCapture(uint8_t count);
uint8_t K230Link_IsCapturePending(void);
uint8_t K230Link_PopCaptureAck(uint8_t *ok, uint16_t *index);

#endif
