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

/* 把一帧 LANE 送进 K230Link，五个带各自的偏差可以不同——用于构造
 * 「近带是哨兵、远带有效」之类的退带场景。 */
static void FeedLaneBands(const int16_t offsets[5], uint8_t confidence,
                          uint8_t sequence)
{
    uint8_t payload[12];
    uint8_t band;

    payload[0] = 1U;
    for (band = 0U; band < 5U; band++) {
        uint16_t raw = (uint16_t)offsets[band];
        payload[1U + band * 2U] = (uint8_t)(raw & 0xFFU);
        payload[2U + band * 2U] = (uint8_t)((raw >> 8U) & 0xFFU);
    }
    payload[11] = confidence;
    FeedFrameLocal(K230_LINK_MESSAGE_LANE, sequence, payload, 12U);
}

/* 把一帧 LANE 送进 K230Link，五个带用同一个偏差。 */
static void FeedLane(int16_t offset, uint8_t confidence, uint8_t sequence)
{
    int16_t offsets[5];
    uint8_t band;

    for (band = 0U; band < 5U; band++) {
        offsets[band] = offset;
    }
    FeedLaneBands(offsets, confidence, sequence);
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

/* 五个带全部是哨兵——没有任何一条带能提供偏差，只能判丢道。 */
static void test_all_bands_invalid_is_treated_as_lost(void)
{
    int16_t offsets[5];
    uint8_t band;

    SetupRunning();
    for (band = 0U; band < 5U; band++) {
        offsets[band] = K230_LINK_LANE_OFFSET_INVALID;
    }
    FeedLaneBands(offsets, 95U, 0x36U);
    K230Link_Update(1U);
    MotionLane_Update(0.01f);
    CHECK_NEAR(g_lastLeftMMps, 0.0f, 0.01f);
}

/* b0 贴着中心线覆盖范围的边界，车头/阴影/赛道边沿遮住画面底部一点点
 * 就会让它变成哨兵；只要 b1..b4 还有一条有效，就必须退到那条带继续
 * 正常控制，而不是稀里糊涂判丢道。用的偏差必须是 b1 的（-100 → 车道
 * 偏右 → 左轮快、右轮慢），不能是 b0 的（哨兵，如果被当成 0 会得到
 * 完全不同、错误的“居中”结果）。 */
static void test_near_band_invalid_falls_back_to_next_valid_band(void)
{
    int16_t offsets[5] = {
        K230_LINK_LANE_OFFSET_INVALID, -100, -100, -100, -100
    };

    SetupRunning();
    FeedLaneBands(offsets, 96U, 0x3AU);
    K230Link_Update(1U);
    MotionLane_Update(0.01f);

    CHECK(MotionLane_IsBusy() == 1U);           /* 没有判丢道 */
    CHECK(g_lastLeftMMps > g_lastRightMMps);    /* 用 b1 的偏差正常转向 */
    CHECK_NEAR(MotionLane_GetLaneError(), 100.0f, 0.01f);
    CHECK_NEAR(MotionLane_GetAdjustMMps(), 20.0f, 0.01f);  /* 0.20 * 100 */
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

    /* 之后不再喂帧：视觉数据陈旧到一定程度就会被判失效——实际先触发的
     * 是 MotionLane 自己更紧的新鲜度闸（见下面 test_stale_lane_*），
     * 这里跑满 K230_LINK_LANE_TIMEOUT_TICKS 只是确保连链路层的超时也
     * 一并过了，双重覆盖。丢失期间保持窗口还没走完，任务不结束。 */
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

/* 视觉数据本身允许多陈旧，控制层的新鲜度阈值必须比 K230Link 的链路层
 * 300 ms 超时更紧——否则"链路还活着但偶尔丢帧"的 300 ms 里，控制律会
 * 一直拿着同一帧陈旧偏差持续打方向。喂一帧有效数据后只推进
 * K230Link_Update，不再喂新帧：走完 MOTION_LANE_MAX_LANE_AGE_TICKS 之后
 * 丢失保持刚起步（还没结束），再叠加满一个 MOTION_LANE_LOST_HOLD_TICKS
 * 窗口就必须结束——这时链路层的 K230_LINK_LANE_TIMEOUT_TICKS 还差得远
 * （10+30 < 30+30），能在这里结束只能是控制层自己的新鲜度闸在起作用。 */
static void test_stale_lane_starts_lost_hold_before_link_timeout(void)
{
    uint16_t tick;

    SetupRunning();
    FeedLane(-100, 90U, 0x41U);
    K230Link_Update(1U);
    MotionLane_Update(0.01f);
    CHECK(g_lastLeftMMps > g_lastRightMMps);   /* 先确认控制生效 */

    for (tick = 0U; tick < MOTION_LANE_MAX_LANE_AGE_TICKS; tick++) {
        K230Link_Update(1U);
        MotionLane_Update(0.01f);
    }
    CHECK(MotionLane_IsBusy() == 1U);   /* 丢失保持刚起步，还没结束 */

    for (tick = 0U; tick < MOTION_LANE_LOST_HOLD_TICKS; tick++) {
        K230Link_Update(1U);
        MotionLane_Update(0.01f);
    }
    CHECK(MotionLane_IsFinished() == 1U);
}

/* FINISHED 是巡道任务因丢道而正常结束的路径，遥测量必须归零——不能像
 * MotionWheel_Stop() 之外那样冻结在最后一次有效值，否则标定时会被
 * 误读成"车已经停了但视觉还在报误差"。 */
static void test_finished_resets_telemetry(void)
{
    uint16_t tick;

    SetupRunning();
    FeedLane(-100, 90U, 0x42U);
    K230Link_Update(1U);
    MotionLane_Update(0.01f);
    CHECK(MotionLane_GetLaneError() != 0.0f);    /* 先确认遥测量非零 */
    CHECK(MotionLane_GetAdjustMMps() != 0.0f);

    for (tick = 0U; tick < K230_LINK_LANE_TIMEOUT_TICKS; tick++) {
        K230Link_Update(1U);
        MotionLane_Update(0.01f);
    }
    for (tick = 0U; tick < MOTION_LANE_LOST_HOLD_TICKS; tick++) {
        K230Link_Update(1U);
        MotionLane_Update(0.01f);
    }
    CHECK(MotionLane_IsFinished() == 1U);
    CHECK_NEAR(MotionLane_GetLaneError(), 0.0f, 0.001f);
    CHECK_NEAR(MotionLane_GetAdjustMMps(), 0.0f, 0.001f);
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
    test_all_bands_invalid_is_treated_as_lost();
    test_near_band_invalid_falls_back_to_next_valid_band();
    test_lost_holds_then_finishes();
    test_stale_lane_starts_lost_hold_before_link_timeout();
    test_finished_resets_telemetry();
    test_start_rejects_invalid_speed();
    test_start_rejects_when_busy();

    if (s_failures == 0) { printf("test_motionlane: ALL PASS\n"); return 0; }
    printf("test_motionlane: %d FAILURE(S)\n", s_failures);
    return 1;
}
