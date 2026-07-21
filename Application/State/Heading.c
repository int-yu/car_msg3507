#include "Application/State/Heading.h"
#include "Hardware/Sensors/MPU6050.h"
#include "System/Delay.h"
#include <math.h>

/* 航向角 —— 对 MPU6050 的 Z 轴角速度积分得到连续累计偏航角（度）。
 * yaw(°) += (gz_raw - bias) / GYRO_LSB_PER_DPS * s_scale * dt
 * 角度不限制在 ±180°，因此可直接记录多圈旋转。
 * 漂移处理：开机静止取零偏；检测到外部地标时可用 Heading_SetYaw() 重置基准。
 * 尺度修正：标称 32.8 LSB/(°/s) 未必匹配本芯片，可旋转 N 圈标定 s_scale。
 */

/* GYRO_CONFIG=0x10 -> ±1000 dps, 灵敏度 32.8 LSB/(°/s) */
#define GYRO_LSB_PER_DPS 32.8f
#define GYRO_Z_DIR_SIGN  (-1.0f)

static float s_yaw;
static float s_bias;
static float s_scale = 1.0f;      /* 陀螺仪尺度修正因子 */
static uint8_t s_ready;
static uint8_t s_calibActive;     /* 1 表示正在进行尺度标定 */
static float s_calibAngle;        /* 标定期间按标称灵敏度积分的原始角度 */

void Heading_Init(void)
{
    MPU6050_Init();
    s_yaw = 0.0f;
    s_bias = 0.0f;
    s_scale = 1.0f;
    s_calibActive = 0U;
    s_calibAngle = 0.0f;
    s_ready = MPU6050_IsReady();
}

void Heading_Calibrate(void)
{
    uint16_t i;
    float sum = 0.0f;
    if (s_ready == 0U) return;

    for (i = 0U; i < HEADING_CALIBRATION_SAMPLES; i++)
    {
        sum += (float)MPU6050_GetGyroZ();
        if (MPU6050_IsReady() == 0U)
        {
            s_ready = 0U;
            s_bias = 0.0f;
            return;
        }
        Delay_ms(HEADING_CALIBRATION_INTERVAL_MS);
    }
    s_bias = sum / (float)HEADING_CALIBRATION_SAMPLES;
    s_yaw = 0.0f;
}

void Heading_Update(float dt)
{
    float gz;
    float rate;
    if (s_ready == 0U) return;
    gz = (float)MPU6050_GetGyroZ() - s_bias;   /* 去零偏后的原始值 */
    if (MPU6050_IsReady() == 0U) { s_ready = 0U; return; }
    rate = GYRO_Z_DIR_SIGN * gz / GYRO_LSB_PER_DPS; /* 统一到工程约定的偏航角正方向。 */
    s_yaw += rate * s_scale * dt;              /* 积分(应用标定尺度) */
    if (s_calibActive != 0U)
    {
        s_calibAngle += rate * dt;             /* 累计标称角度，用于解算真实尺度 */
    }
}

uint8_t Heading_IsReady(void) { return s_ready; }
float Heading_GetYaw(void) { return s_yaw; }
void Heading_SetYaw(float yaw) { s_yaw = yaw; }

/* ===== 陀螺仪尺度标定：原地旋转 N 圈 ===== */

void Heading_ScaleCalibStart(void)
{
    s_calibAngle = 0.0f;
    s_calibActive = 1U;
}

float Heading_ScaleCalibFinish(uint16_t turns)
{
    float truth = (float)turns * 360.0f;             /* N 整圈对应的真实角度 */
    s_calibActive = 0U;
    /* 旋转角度过小时不更新尺度因子，避免除零或误标定。 */
    if ((s_calibAngle > 1.0f) || (s_calibAngle < -1.0f))
    {
        s_scale = truth / fabsf(s_calibAngle);
    }
    return s_scale;
}

void Heading_ScaleCalibCancel(void)
{
    s_calibActive = 0U;                              /* 中止: 不改 s_scale */
}

float Heading_GetCalibAngle(void) { return s_calibAngle; }

uint8_t Heading_IsScaleCalibActive(void) { return s_calibActive; }

float Heading_GetScale(void) { return s_scale; }

void Heading_SetScale(float scale)
{
    if (scale > 0.0f) { s_scale = scale; }           /* 掉电恢复/手动填已知标定值 */
}
