#include "Application/State/Heading.h"
#include "tests/host/test_assert.h"

void Stub_SetGyroZ(int16_t value);
void Stub_SetMpuReady(uint8_t ready);

/* GYRO_Z_DIR_SIGN = -1，灵敏度 32.8 LSB/(°/s)。
 * 原始值 -328 → rate = -1 * (-328) / 32.8 = +10 °/s。 */
static void test_yaw_rate_matches_gyro(void)
{
    Stub_SetMpuReady(1U);
    Heading_Init();
    Stub_SetGyroZ(-328);
    Heading_Update(0.01f);
    CHECK_NEAR(Heading_GetYawRate(), 10.0f, 0.2f);
}

static void test_yaw_rate_sign_flips_with_gyro(void)
{
    Stub_SetMpuReady(1U);
    Heading_Init();
    Stub_SetGyroZ(328);
    Heading_Update(0.01f);
    CHECK_NEAR(Heading_GetYawRate(), -10.0f, 0.2f);
}

static void test_yaw_rate_is_zero_when_offline(void)
{
    Stub_SetMpuReady(1U);
    Heading_Init();
    Stub_SetGyroZ(-328);
    Heading_Update(0.01f);
    Stub_SetMpuReady(0U);
    Heading_Update(0.01f);
    CHECK_NEAR(Heading_GetYawRate(), 0.0f, 0.001f);
}

static void test_yaw_integral_still_matches_rate(void)
{
    int tick;
    Stub_SetMpuReady(1U);
    Heading_Init();
    Stub_SetGyroZ(-328);                 /* +10 °/s */
    for (tick = 0; tick < 100; tick++) { Heading_Update(0.01f); }
    CHECK_NEAR(Heading_GetYaw(), 10.0f, 0.2f);  /* 1 秒 → 10 度 */
}

int main(void)
{
    test_yaw_rate_matches_gyro();
    test_yaw_rate_sign_flips_with_gyro();
    test_yaw_rate_is_zero_when_offline();
    test_yaw_integral_still_matches_rate();

    if (s_failures == 0) { printf("test_heading: ALL PASS\n"); return 0; }
    printf("test_heading: %d FAILURE(S)\n", s_failures);
    return 1;
}
