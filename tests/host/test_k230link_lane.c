#include "Application/Comms/K230Link.h"
#include <stdio.h>
#include <string.h>

void Stub_ResetSerial(void);
void Stub_FeedRx(const uint8_t *data, uint16_t length);

static int s_failures;

#define CHECK(cond) do {                                            \
    if (!(cond)) {                                                  \
        printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);      \
        s_failures++;                                               \
    }                                                               \
} while (0)

static uint8_t Crc8(const uint8_t *data, uint8_t length)
{
    uint8_t crc = 0U;
    uint8_t i;
    uint8_t bit;
    for (i = 0U; i < length; i++)
    {
        crc ^= data[i];
        for (bit = 0U; bit < 8U; bit++)
        {
            crc = ((crc & 0x80U) != 0U) ?
                (uint8_t)((crc << 1U) ^ 0x07U) : (uint8_t)(crc << 1U);
        }
    }
    return crc;
}

/* 组一帧塞进 RX 队列。payload 传 NULL 表示零长度。 */
static void FeedFrame(uint8_t type, uint8_t sequence,
                      const uint8_t *payload, uint8_t length)
{
    uint8_t frame[64];
    frame[0] = 0xAAU;
    frame[1] = 0x55U;
    frame[2] = 0x01U;
    frame[3] = type;
    frame[4] = sequence;
    frame[5] = length;
    if (length > 0U) { memcpy(&frame[6], payload, length); }
    frame[6U + length] = Crc8(&frame[2], (uint8_t)(4U + length));
    Stub_FeedRx(frame, (uint16_t)(7U + length));
}

static void PutI16(uint8_t *payload, uint8_t index, int16_t value)
{
    uint16_t raw = (uint16_t)value;
    payload[index] = (uint8_t)(raw & 0xFFU);
    payload[index + 1U] = (uint8_t)((raw >> 8U) & 0xFFU);
}

/* 让 K230Link 进入 ready：先收对端 READY，再收对端对我方 READY 的 ACK。
 * 我方 READY 的 sequence 由 K230Link_Init() 分配，固定是 0。 */
static void CompleteHandshake(void)
{
    uint8_t ackPayload = 0x00U;
    FeedFrame(0x01U, 0x77U, NULL, 0U);          /* 对端 READY */
    FeedFrame(0x02U, 0x78U, &ackPayload, 1U);   /* 对端 READY_ACK(seq=0) */
    K230Link_Update(1U);
}

static void BuildLanePayload(uint8_t *payload, uint8_t valid,
                             const int16_t *offsets, uint8_t confidence)
{
    uint8_t band;
    payload[0] = valid;
    for (band = 0U; band < K230_LINK_LANE_BAND_COUNT; band++)
    {
        PutI16(payload, (uint8_t)(1U + band * 2U), offsets[band]);
    }
    payload[11] = confidence;
}

static void Setup(void)
{
    Stub_ResetSerial();
    K230Link_Init();
    CompleteHandshake();
}

static void test_handshake_completes(void)
{
    Setup();
    CHECK(K230Link_IsReady() == 1U);
}

static void test_lane_frame_is_decoded(void)
{
    uint8_t payload[12];
    int16_t offsets[5] = { 10, -20, 30, -40, 50 };
    K230Link_Lane_t lane;

    Setup();
    BuildLanePayload(payload, 1U, offsets, 88U);
    FeedFrame(K230_LINK_MESSAGE_LANE, 0x21U, payload, 12U);
    K230Link_Update(1U);

    CHECK(K230Link_GetLane(&lane) == 1U);
    CHECK(lane.valid == 1U);
    CHECK(lane.offsetPermille[0] == 10);
    CHECK(lane.offsetPermille[1] == -20);
    CHECK(lane.offsetPermille[4] == 50);
    CHECK(lane.confidence == 88U);
    CHECK(lane.sequence == 0x21U);
    CHECK(lane.bandValid == 0x1FU);
}

static void test_sentinel_band_is_marked_invalid(void)
{
    uint8_t payload[12];
    int16_t offsets[5] = { 10, 20, 30, 40, K230_LINK_LANE_OFFSET_INVALID };
    K230Link_Lane_t lane;

    Setup();
    BuildLanePayload(payload, 1U, offsets, 60U);
    FeedFrame(K230_LINK_MESSAGE_LANE, 0x22U, payload, 12U);
    K230Link_Update(1U);

    CHECK(K230Link_GetLane(&lane) == 1U);
    CHECK(lane.bandValid == 0x0FU);       /* bit4 清零 */
    CHECK((lane.bandValid & 0x01U) != 0U);
}

static void test_lane_goes_stale_after_timeout(void)
{
    uint8_t payload[12];
    int16_t offsets[5] = { 1, 2, 3, 4, 5 };
    K230Link_Lane_t lane;
    uint8_t tick;

    Setup();
    BuildLanePayload(payload, 1U, offsets, 90U);
    FeedFrame(K230_LINK_MESSAGE_LANE, 0x23U, payload, 12U);
    K230Link_Update(1U);
    CHECK(K230Link_GetLane(&lane) == 1U);

    for (tick = 0U; tick < K230_LINK_LANE_TIMEOUT_TICKS; tick++)
    {
        K230Link_Update(1U);
    }
    CHECK(K230Link_GetLane(&lane) == 0U);
}

static void test_new_frame_refreshes_age(void)
{
    uint8_t payload[12];
    int16_t offsets[5] = { 1, 2, 3, 4, 5 };
    K230Link_Lane_t lane;
    uint8_t tick;

    Setup();
    BuildLanePayload(payload, 1U, offsets, 90U);
    FeedFrame(K230_LINK_MESSAGE_LANE, 0x24U, payload, 12U);
    K230Link_Update(1U);

    for (tick = 0U; tick < K230_LINK_LANE_TIMEOUT_TICKS - 2U; tick++)
    {
        K230Link_Update(1U);
    }
    FeedFrame(K230_LINK_MESSAGE_LANE, 0x25U, payload, 12U);
    K230Link_Update(1U);
    CHECK(K230Link_GetLane(&lane) == 1U);
    CHECK(lane.sequence == 0x25U);
}

/* TARGET 承载钢球位置。钢球平衡是双积分对象，拿冻结的球位算 PD 会让
 * 倾角锁死、球一路加速滚到挡片，所以它和 LANE 一样必须有新鲜度。 */
static void BuildTargetPayload(
    uint8_t *payload, uint8_t valid, int16_t x, int16_t y)
{
    payload[0] = valid;
    PutI16(payload, 1U, x);
    PutI16(payload, 3U, y);
}

static void test_target_frame_is_decoded(void)
{
    uint8_t payload[5];
    K230Link_Target_t target;

    Setup();
    BuildTargetPayload(payload, 1U, 400, -120);
    FeedFrame(K230_LINK_MESSAGE_TARGET, 0x31U, payload, 5U);
    K230Link_Update(1U);

    CHECK(K230Link_GetTarget(&target) == 1U);
    CHECK(target.valid == 1U);
    CHECK(target.offsetX == 400);
    CHECK(target.offsetY == -120);
    CHECK(target.sequence == 0x31U);
    /* Update() 先解析再累加年龄，所以同拍解析的帧出来就是 1 拍，不是 0。 */
    CHECK(target.ageTicks == 1U);
}

static void test_target_goes_stale_after_timeout(void)
{
    uint8_t payload[5];
    K230Link_Target_t target;
    uint8_t tick;

    Setup();
    BuildTargetPayload(payload, 1U, 250, 0);
    FeedFrame(K230_LINK_MESSAGE_TARGET, 0x32U, payload, 5U);
    K230Link_Update(1U);
    CHECK(K230Link_GetTarget(&target) == 1U);

    for (tick = 0U; tick < K230_LINK_TARGET_TIMEOUT_TICKS; tick++)
    {
        K230Link_Update(1U);
    }
    CHECK(K230Link_GetTarget(&target) == 0U);
}

static void test_new_target_frame_refreshes_age(void)
{
    uint8_t payload[5];
    K230Link_Target_t target;
    uint8_t tick;

    Setup();
    BuildTargetPayload(payload, 1U, 250, 0);
    FeedFrame(K230_LINK_MESSAGE_TARGET, 0x33U, payload, 5U);
    K230Link_Update(1U);

    for (tick = 0U; tick < K230_LINK_TARGET_TIMEOUT_TICKS - 2U; tick++)
    {
        K230Link_Update(1U);
    }
    BuildTargetPayload(payload, 1U, -300, 40);
    FeedFrame(K230_LINK_MESSAGE_TARGET, 0x34U, payload, 5U);
    K230Link_Update(1U);

    CHECK(K230Link_GetTarget(&target) == 1U);
    CHECK(target.sequence == 0x34U);
    CHECK(target.offsetX == -300);
    CHECK(target.ageTicks == 1U);
}

/* 超时后不写调用方缓冲：调用方必须靠返回值判失效，而不是读到旧值。 */
static void test_stale_target_does_not_overwrite_caller(void)
{
    uint8_t payload[5];
    K230Link_Target_t target;
    uint8_t tick;

    Setup();
    BuildTargetPayload(payload, 1U, 777, 0);
    FeedFrame(K230_LINK_MESSAGE_TARGET, 0x35U, payload, 5U);
    K230Link_Update(1U);
    CHECK(K230Link_GetTarget(&target) == 1U);

    for (tick = 0U; tick < K230_LINK_TARGET_TIMEOUT_TICKS; tick++)
    {
        K230Link_Update(1U);
    }

    target.offsetX = 12345;
    CHECK(K230Link_GetTarget(&target) == 0U);
    CHECK(target.offsetX == 12345);
}

static void test_legacy_ball_position_has_no_speed(void)
{
    uint8_t payload[2];
    K230Link_BallPosition_t ball;

    Setup();
    PutI16(payload, 0U, 1200);
    FeedFrame(K230_LINK_MESSAGE_BALL_POSITION, 0x41U, payload, 2U);
    K230Link_Update(1U);

    CHECK(K230Link_GetBallPosition(&ball) == 1U);
    CHECK(ball.positionX100 == 1200);
    CHECK(ball.speedValid == 0U);
}

static void test_extended_ball_frame_decodes_speed(void)
{
    uint8_t payload[4];
    K230Link_BallPosition_t ball;

    Setup();
    PutI16(payload, 0U, -800);
    PutI16(payload, 2U, 2500);
    FeedFrame(K230_LINK_MESSAGE_BALL_POSITION, 0x42U, payload, 4U);
    K230Link_Update(1U);

    CHECK(K230Link_GetBallPosition(&ball) == 1U);
    CHECK(ball.positionX100 == -800);
    CHECK(ball.speedValid == 1U);
    CHECK(ball.speedX100 == 2500);
}

static void test_bad_crc_is_rejected(void)
{
    uint8_t frame[19];
    uint8_t payload[12];
    int16_t offsets[5] = { 7, 7, 7, 7, 7 };
    K230Link_Lane_t lane;

    Setup();
    BuildLanePayload(payload, 1U, offsets, 90U);
    frame[0] = 0xAAU; frame[1] = 0x55U; frame[2] = 0x01U;
    frame[3] = K230_LINK_MESSAGE_LANE; frame[4] = 0x26U; frame[5] = 12U;
    memcpy(&frame[6], payload, 12U);
    frame[18] = (uint8_t)(Crc8(&frame[2], 16U) ^ 0xFFU);  /* 故意错 */
    Stub_FeedRx(frame, 19U);
    K230Link_Update(1U);

    CHECK(K230Link_GetLane(&lane) == 0U);
}

static void test_wrong_length_is_rejected(void)
{
    uint8_t payload[11] = {0};
    K230Link_Lane_t lane;

    Setup();
    payload[0] = 1U;
    FeedFrame(K230_LINK_MESSAGE_LANE, 0x27U, payload, 11U);
    K230Link_Update(1U);
    CHECK(K230Link_GetLane(&lane) == 0U);
}

static void test_lane_before_handshake_is_ignored(void)
{
    uint8_t payload[12];
    int16_t offsets[5] = { 9, 9, 9, 9, 9 };
    K230Link_Lane_t lane;

    Stub_ResetSerial();
    K230Link_Init();
    BuildLanePayload(payload, 1U, offsets, 90U);
    FeedFrame(K230_LINK_MESSAGE_LANE, 0x28U, payload, 12U);
    K230Link_Update(1U);
    CHECK(K230Link_GetLane(&lane) == 0U);
}

int main(void)
{
    test_handshake_completes();
    test_lane_frame_is_decoded();
    test_sentinel_band_is_marked_invalid();
    test_lane_goes_stale_after_timeout();
    test_new_frame_refreshes_age();
    test_target_frame_is_decoded();
    test_target_goes_stale_after_timeout();
    test_new_target_frame_refreshes_age();
    test_stale_target_does_not_overwrite_caller();
    test_legacy_ball_position_has_no_speed();
    test_extended_ball_frame_decodes_speed();
    test_bad_crc_is_rejected();
    test_wrong_length_is_rejected();
    test_lane_before_handshake_is_ignored();

    if (s_failures == 0) { printf("test_k230link_lane: ALL PASS\n"); return 0; }
    printf("test_k230link_lane: %d FAILURE(S)\n", s_failures);
    return 1;
}
