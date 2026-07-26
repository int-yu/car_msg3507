#include "Application/Control/MotionLane.h"
#include "Application/Comms/K230Link.h"
#include "Application/State/Heading.h"
#include "tests/host/test_assert.h"
#include <string.h>

void Stub_ResetSerial(void);
void Stub_FeedRx(const uint8_t *data, uint16_t length);
void Stub_SetGyroZ(int16_t value);
void Stub_SetMpuReady(uint8_t ready);
extern float g_lastLeftMMps;
extern float g_lastRightMMps;
extern uint16_t g_wheelStopCount;

static uint8_t Crc8Local(const uint8_t *data, uint8_t length)
{
    uint8_t crc = 0U; uint8_t i; uint8_t bit;
    for (i = 0U; i < length; i++) {
        crc ^= data[i];
        for (bit = 0U; bit < 8U; bit++) {
            crc = ((crc & 0x80U) != 0U) ?
                (uint8_t)((crc << 1U) ^ 0x07U) : (uint8_t)(crc << 1U);
        }
    }
    return crc;
}

static void FeedFrameLocal(uint8_t type, uint8_t sequence,
                           const uint8_t *payload, uint8_t length)
{
    uint8_t frame[64];
    frame[0] = 0xAAU; frame[1] = 0x55U; frame[2] = 0x01U;
    frame[3] = type; frame[4] = sequence; frame[5] = length;
    if (length > 0U) { memcpy(&frame[6], payload, length); }
    frame[6U + length] = Crc8Local(&frame[2], (uint8_t)(4U + length));
    Stub_FeedRx(frame, (uint16_t)(7U + length));
}

/* 把一帧 LANE 送进 K230Link，五个带用同一个偏差。 */
static void FeedLane(int16_t offset, uint8_t confidence, uint8_t sequence)
{
    uint8_t payload[12];
    uint8_t band;
    uint16_t raw = (uint16_t)offset;

    payload[0] = 1U;
    for (band = 0U; band < 5U; band++) {
        payload[1U + band * 2U] = (uint8_t)(raw & 0xFFU);
        payload[2U + band * 2U] = (uint8_t)((raw >> 8U) & 0xFFU);
    }
    payload[11] = confidence;
    FeedFrameLocal(K230_LINK_MESSAGE_LANE, sequence, payload, 12U);
}

/* 复位到「握手完成、陀螺仪静止、巡道以 300 mm/s 运行中」。 */
static void SetupRunning(void)
{
    uint8_t ackPayload = 0x00U;

    Stub_ResetSerial();
    K230Link_Init();
    FeedFrameLocal(0x01U, 0x77U, NULL, 0U);
    FeedFrameLocal(0x02U, 0x78U, &ackPayload, 1U);
    K230Link_Update(1U);

    Stub_SetMpuReady(1U);
    Heading_Init();
    Stub_SetGyroZ(0);
    Heading_Update(0.01f);

    MotionLane_TuneKp = 0.20f;
    MotionLane_TuneKdYaw = 0.30f;
    MotionLane_TuneMaxAdjustRatio = 0.35f;
    MotionLane_Init();
    MotionLane_Start(300.0f);
}

static void test_centered_lane_drives_both_wheels_equally(void)
{
    SetupRunning();
    FeedLane(0, 90U, 0x30U);
    K230Link_Update(1U);
    MotionLane_Update(0.01f);
    CHECK_NEAR(g_lastLeftMMps, 300.0f, 0.01f);
    CHECK_NEAR(g_lastRightMMps, 300.0f, 0.01f);
}

/* K230 发的是「画面中心 - 车道中心」。车道偏右 → 偏差为负 →
 * 车必须右转 → 左轮快、右轮慢。这是整条链最容易搞反的地方。 */
static void test_lane_right_makes_car_turn_right(void)
{
    SetupRunning();
    FeedLane(-100, 90U, 0x31U);
    K230Link_Update(1U);
    MotionLane_Update(0.01f);
    CHECK(g_lastLeftMMps > g_lastRightMMps);
    CHECK_NEAR(MotionLane_GetLaneError(), 100.0f, 0.01f);
    CHECK_NEAR(MotionLane_GetAdjustMMps(), 20.0f, 0.01f);  /* 0.20 * 100 */
}

static void test_lane_left_makes_car_turn_left(void)
{
    SetupRunning();
    FeedLane(100, 90U, 0x32U);
    K230Link_Update(1U);
    MotionLane_Update(0.01f);
    CHECK(g_lastLeftMMps < g_lastRightMMps);
    CHECK_NEAR(MotionLane_GetLaneError(), -100.0f, 0.01f);
}

/* 车已经在右转（yawRate > 0）时，阻尼项必须减小右转指令。 */
static void test_yaw_rate_damps_the_command(void)
{
    float withoutDamping;
    float withDamping;

    SetupRunning();
    FeedLane(-100, 90U, 0x33U);
    K230Link_Update(1U);
    MotionLane_Update(0.01f);
    withoutDamping = MotionLane_GetAdjustMMps();

    Stub_SetGyroZ(-328);             /* +10 °/s，右转 */
    Heading_Update(0.01f);
    MotionLane_Update(0.01f);
    withDamping = MotionLane_GetAdjustMMps();

    CHECK(withDamping < withoutDamping);
    CHECK_NEAR(withDamping, withoutDamping - 3.0f, 0.2f);  /* 0.30 * 10 */
}

static void test_adjust_is_clamped_by_ratio(void)
{
    SetupRunning();
    /* Kp 调大到必然越限：1.0 * 500 = 500，远超 300 * 0.35 = 105。
     * 用默认 Kp=0.20 时 adjust 只有 100 < 105，限幅根本不触发，
     * 断言会白白通过——那样的用例等于什么都没测。 */
    MotionLane_TuneKp = 1.0f;
    FeedLane(-500, 90U, 0x34U);
    K230Link_Update(1U);
    MotionLane_Update(0.01f);
    CHECK_NEAR(MotionLane_GetAdjustMMps(), 300.0f * 0.35f, 0.01f);
    CHECK_NEAR(g_lastLeftMMps, 300.0f * 1.35f, 0.01f);
    CHECK_NEAR(g_lastRightMMps, 300.0f * 0.65f, 0.01f);
}

static void test_low_confidence_is_treated_as_lost(void)
{
    SetupRunning();
    FeedLane(-300, 5U, 0x35U);       /* 置信度低于阈值 */
    K230Link_Update(1U);
    MotionLane_Update(0.01f);
    /* 丢失时保持上一拍轮速；上一拍从未成功过，因此是 0。 */
    CHECK_NEAR(g_lastLeftMMps, 0.0f, 0.01f);
    CHECK_NEAR(g_lastRightMMps, 0.0f, 0.01f);
}

static void test_invalid_near_band_is_treated_as_lost(void)
{
    uint8_t payload[12];
    uint8_t band;

    SetupRunning();
    payload[0] = 1U;
    for (band = 0U; band < 5U; band++) {
        payload[1U + band * 2U] = 0x00U;   /* -32768 的低字节 */
        payload[2U + band * 2U] = 0x80U;   /* 高字节 */
    }
    payload[11] = 95U;
    FeedFrameLocal(K230_LINK_MESSAGE_LANE, 0x36U, payload, 12U);
    K230Link_Update(1U);
    MotionLane_Update(0.01f);
    CHECK_NEAR(g_lastLeftMMps, 0.0f, 0.01f);
}

/* 丢失先保持上一拍轮速，超过保持窗口才结束任务。 */
static void test_lost_holds_then_finishes(void)
{
    uint16_t tick;

    SetupRunning();
    FeedLane(0, 90U, 0x37U);
    K230Link_Update(1U);
    MotionLane_Update(0.01f);
    CHECK_NEAR(g_lastLeftMMps, 300.0f, 0.01f);

    /* 之后不再喂帧：K230Link 超时 → MotionLane 判丢失。 */
    for (tick = 0U; tick < K230_LINK_LANE_TIMEOUT_TICKS; tick++) {
        K230Link_Update(1U);
        MotionLane_Update(0.01f);
    }
    CHECK(MotionLane_IsBusy() == 1U);          /* 还在保持窗口内 */
    CHECK_NEAR(g_lastLeftMMps, 300.0f, 0.01f); /* 保持上一拍 */

    for (tick = 0U; tick < MOTION_LANE_LOST_HOLD_TICKS; tick++) {
        K230Link_Update(1U);
        MotionLane_Update(0.01f);
    }
    CHECK(MotionLane_IsFinished() == 1U);
    CHECK(MotionLane_IsBusy() == 0U);
}

static void test_start_rejects_invalid_speed(void)
{
    SetupRunning();
    MotionLane_Stop();
    CHECK(MotionLane_Start(0.0f) == MOTION_LANE_RESULT_INVALID_ARGUMENT);
    CHECK(MotionLane_Start(-5.0f) == MOTION_LANE_RESULT_INVALID_ARGUMENT);
}

static void test_start_rejects_when_busy(void)
{
    SetupRunning();
    CHECK(MotionLane_Start(200.0f) == MOTION_LANE_RESULT_BUSY);
}

int main(void)
{
    test_centered_lane_drives_both_wheels_equally();
    test_lane_right_makes_car_turn_right();
    test_lane_left_makes_car_turn_left();
    test_yaw_rate_damps_the_command();
    test_adjust_is_clamped_by_ratio();
    test_low_confidence_is_treated_as_lost();
    test_invalid_near_band_is_treated_as_lost();
    test_lost_holds_then_finishes();
    test_start_rejects_invalid_speed();
    test_start_rejects_when_busy();

    if (s_failures == 0) { printf("test_motionlane: ALL PASS\n"); return 0; }
    printf("test_motionlane: %d FAILURE(S)\n", s_failures);
    return 1;
}
