/*
 * 宿主机（MinGW gcc）单元测试用的硬件桩。
 * 目的是把 K230Link / MotionLane 的纯逻辑从 MSPM0 外设里剥出来跑，
 * 符号错误、超时算术、限幅这些最容易出错的地方不用烧板子就能验。
 */
#include "Application/Control/MotionWheel.h"
#include "Hardware/Comms/Serial.h"
#include <stdint.h>
#include <string.h>

/* ---- Serial3：可喂入的 RX 队列 + 捕获的 TX 缓冲 ---- */
static uint8_t s_rx[512];
static uint16_t s_rxHead;
static uint16_t s_rxTail;
uint8_t g_txBuffer[512];
uint16_t g_txLength;

void Stub_ResetSerial(void)
{
    s_rxHead = 0U;
    s_rxTail = 0U;
    g_txLength = 0U;
}

void Stub_FeedRx(const uint8_t *data, uint16_t length)
{
    uint16_t index;
    for (index = 0U; index < length; index++)
    {
        s_rx[s_rxTail++] = data[index];
    }
}

void Serial3_Init(void) { Stub_ResetSerial(); }

uint8_t Serial3_SendArray(const uint8_t *data, uint16_t length)
{
    uint16_t index;
    for (index = 0U; index < length; index++)
    {
        g_txBuffer[g_txLength++] = data[index];
    }
    return 1U;
}

uint8_t Serial3_ReadByte(uint8_t *out)
{
    if (s_rxHead >= s_rxTail) { return 0U; }
    *out = s_rx[s_rxHead++];
    return 1U;
}

/* ---- MPU6050：可设定的 Z 轴角速度 ---- */
static int16_t s_gyroZ;
static uint8_t s_mpuReady = 1U;

void Stub_SetGyroZ(int16_t value) { s_gyroZ = value; }
void Stub_SetMpuReady(uint8_t ready) { s_mpuReady = ready; }

void MPU6050_Init(void) {}
uint8_t MPU6050_IsReady(void) { return s_mpuReady; }
uint8_t MPU6050_GetID(void) { return 0x68U; }
int16_t MPU6050_GetGyroZ(void) { return s_gyroZ; }
void MPU6050_GetData(int16_t *ax, int16_t *ay, int16_t *az,
                     int16_t *gx, int16_t *gy, int16_t *gz)
{
    if (ax) { *ax = 0; } if (ay) { *ay = 0; } if (az) { *az = 0; }
    if (gx) { *gx = 0; } if (gy) { *gy = 0; } if (gz) { *gz = s_gyroZ; }
}

void Delay_us(uint32_t us) { (void)us; }
void Delay_ms(uint32_t ms) { (void)ms; }
void Delay_s(uint32_t s) { (void)s; }

/* ---- MotionWheel：记录最后一次下发的左右轮目标速度 ---- */
float g_lastLeftMMps;
float g_lastRightMMps;
uint16_t g_wheelUpdateCount;
uint16_t g_wheelStopCount;

MotionWheel_Result_t MotionWheel_Init(void) { return MOTION_WHEEL_RESULT_OK; }

MotionWheel_Result_t MotionWheel_Update(
    const MotionWheel_Command_t *command, float dt)
{
    (void)dt;
    g_lastLeftMMps = command->targetSpeedLMMps;
    g_lastRightMMps = command->targetSpeedRMMps;
    g_wheelUpdateCount++;
    return MOTION_WHEEL_RESULT_OK;
}

void MotionWheel_Stop(void) { g_wheelStopCount++; }
