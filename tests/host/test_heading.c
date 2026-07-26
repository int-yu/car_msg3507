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
    /* 上面这次调用只走到了 Heading_Update() 中段的
     * `if (MPU6050_IsReady() == 0U)` 早退分支——此时 s_ready 还是 1，
     * 是这次调用内部才把它置 0 的。再调用一次，这次进函数时 s_ready
     * 已经是 0 了，走的是顶部 `if (s_ready == 0U)` 那条早退，
     * 把两条早退路径都覆盖到。 */
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

/* 尺度因子默认是 1.0，之前所有用例都没显式设置过 s_scale，所以
 * "s_yaw += s_yawRate * dt" 和漏乘尺度的 "s_yaw += rate * dt" 在
 * 那些用例下数值完全一样，测试根本分辨不出来。这里把尺度设成 2.0，
 * 强制让"乘了尺度"和"没乘尺度"产生不同的结果。 */
static void test_scale_applies_to_yaw_and_yawrate(void)
{
    int tick;
    Stub_SetMpuReady(1U);
    Heading_Init();
    Heading_SetScale(2.0f);              /* 必须在 Heading_Init() 之后调用，
                                           * 否则会被 Init 里的复位覆盖掉 */
    Stub_SetGyroZ(-328);                 /* 未乘尺度前是 +10 °/s */
    Heading_Update(0.01f);
    CHECK_NEAR(Heading_GetYawRate(), 20.0f, 0.4f);   /* 10 * 2.0 */
    for (tick = 0; tick < 100; tick++) { Heading_Update(0.01f); }
    CHECK_NEAR(Heading_GetYaw(), 20.0f, 0.4f);       /* 1 秒 * 20°/s */
}

/* s_calibAngle 是用来解算 s_scale 本身的，如果标定过程里也乘了
 * s_scale，标定就变成自我引用、永远收敛不到正确值。这里用 scale=2.0
 * 同时检查 Heading_GetCalibAngle()（必须不乘尺度）和
 * Heading_GetYaw()（必须乘尺度），两个断言缺一不可：只看标定角
 * 发现不了"漏乘尺度到 yaw"，只看 yaw 发现不了"标定角被误乘尺度"。 */
static void test_calib_angle_ignores_scale(void)
{
    int tick;
    Stub_SetMpuReady(1U);
    Heading_Init();
    Heading_SetScale(2.0f);
    Heading_ScaleCalibStart();
    Stub_SetGyroZ(-328);                 /* 未乘尺度前是 +10 °/s */
    for (tick = 0; tick < 100; tick++) { Heading_Update(0.01f); }
    CHECK_NEAR(Heading_GetCalibAngle(), 10.0f, 0.2f);  /* 未乘尺度 */
    CHECK_NEAR(Heading_GetYaw(), 20.0f, 0.4f);          /* 乘了尺度 */
}

int main(void)
{
    test_yaw_rate_matches_gyro();
    test_yaw_rate_sign_flips_with_gyro();
    test_yaw_rate_is_zero_when_offline();
    test_yaw_integral_still_matches_rate();
    test_scale_applies_to_yaw_and_yawrate();
    test_calib_angle_ignores_scale();

    if (s_failures == 0) { printf("test_heading: ALL PASS\n"); return 0; }
    printf("test_heading: %d FAILURE(S)\n", s_failures);
    return 1;
}
