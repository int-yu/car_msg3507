#include "Hardware/Sensors/MPU6050.h"
#include "System/Delay.h"
#include "ti_msp_dl_config.h"

#define MPU_ADDRESS       0x68U
#define MPU_SMPLRT_DIV    0x19U
#define MPU_CONFIG        0x1AU
#define MPU_GYRO_CONFIG   0x1BU
#define MPU_ACCEL_CONFIG  0x1CU
#define MPU_ACCEL_XOUT_H  0x3BU
#define MPU_GYRO_XOUT_H   0x43U
#define MPU_GYRO_ZOUT_H   0x47U
#define MPU_PWR_MGMT_1    0x6BU
#define MPU_WHO_AM_I      0x75U

static uint8_t s_ready;

static void MPU_DriveLow(uint32_t pin)
{
    DL_GPIO_clearPins(MPU_GPIO_PORT, pin);
    DL_GPIO_enableOutput(MPU_GPIO_PORT, pin);
    Delay_us(2U);
}

static void MPU_Release(uint32_t pin)
{
    DL_GPIO_disableOutput(MPU_GPIO_PORT, pin);
    Delay_us(2U);
}

static void MPU_WriteSCL(uint8_t high)
{
    if (high != 0U) MPU_Release(MPU_GPIO_SCL_PIN);
    else MPU_DriveLow(MPU_GPIO_SCL_PIN);
}

static void MPU_WriteSDA(uint8_t high)
{
    if (high != 0U) MPU_Release(MPU_GPIO_SDA_PIN);
    else MPU_DriveLow(MPU_GPIO_SDA_PIN);
}

static uint8_t MPU_ReadSDA(void)
{
    MPU_Release(MPU_GPIO_SDA_PIN);
    return (DL_GPIO_readPins(MPU_GPIO_PORT, MPU_GPIO_SDA_PIN) != 0U) ? 1U : 0U;
}

static void MPU_Start(void)
{
    MPU_WriteSDA(1U); MPU_WriteSCL(1U); MPU_WriteSDA(0U); MPU_WriteSCL(0U);
}

static void MPU_Stop(void)
{
    MPU_WriteSDA(0U); MPU_WriteSCL(1U); MPU_WriteSDA(1U);
}

static uint8_t MPU_SendByte(uint8_t value)
{
    uint8_t i;
    uint8_t ack;
    for (i = 0U; i < 8U; i++)
    {
        MPU_WriteSDA((value & 0x80U) != 0U);
        MPU_WriteSCL(1U);
        MPU_WriteSCL(0U);
        value <<= 1;
    }
    MPU_WriteSDA(1U);
    MPU_WriteSCL(1U);
    ack = (uint8_t)!MPU_ReadSDA();
    MPU_WriteSCL(0U);
    return ack;
}

static uint8_t MPU_ReceiveByte(uint8_t nack)
{
    uint8_t i;
    uint8_t value = 0U;
    MPU_WriteSDA(1U);
    for (i = 0U; i < 8U; i++)
    {
        value <<= 1;
        MPU_WriteSCL(1U);
        if (MPU_ReadSDA() != 0U) value |= 1U;
        MPU_WriteSCL(0U);
    }
    MPU_WriteSDA(nack);
    MPU_WriteSCL(1U);
    MPU_WriteSCL(0U);
    MPU_WriteSDA(1U);
    return value;
}

static uint8_t MPU_WriteReg(uint8_t reg, uint8_t value)
{
    uint8_t ok;
    MPU_Start();
    ok = MPU_SendByte((uint8_t)(MPU_ADDRESS << 1));
    ok = (uint8_t)(ok && MPU_SendByte(reg));
    ok = (uint8_t)(ok && MPU_SendByte(value));
    MPU_Stop();
    return ok;
}

static uint8_t MPU_ReadReg(uint8_t reg, uint8_t *value)
{
    uint8_t ok;
    if (value == NULL) return 0U;
    MPU_Start();
    ok = MPU_SendByte((uint8_t)(MPU_ADDRESS << 1));
    ok = (uint8_t)(ok && MPU_SendByte(reg));
    if (ok != 0U)
    {
        MPU_Start();
        ok = MPU_SendByte((uint8_t)((MPU_ADDRESS << 1) | 1U));
        if (ok != 0U) *value = MPU_ReceiveByte(1U);
    }
    MPU_Stop();
    return ok;
}

void MPU6050_Init(void)
{
    uint8_t id = 0U;
    MPU_Release(MPU_GPIO_SCL_PIN | MPU_GPIO_SDA_PIN);
    Delay_ms(20U);
    s_ready = MPU_WriteReg(MPU_PWR_MGMT_1, 0x01U);
    s_ready = (uint8_t)(s_ready && MPU_WriteReg(MPU_SMPLRT_DIV, 0x09U));
    s_ready = (uint8_t)(s_ready && MPU_WriteReg(MPU_CONFIG, 0x06U));
    s_ready = (uint8_t)(s_ready && MPU_WriteReg(MPU_GYRO_CONFIG, 0x10U));
    s_ready = (uint8_t)(s_ready && MPU_WriteReg(MPU_ACCEL_CONFIG, 0x18U));
    s_ready = (uint8_t)(s_ready && MPU_ReadReg(MPU_WHO_AM_I, &id) && (id == 0x68U));
}

uint8_t MPU6050_IsReady(void) { return s_ready; }

uint8_t MPU6050_GetID(void)
{
    uint8_t id = 0U;
    if (MPU_ReadReg(MPU_WHO_AM_I, &id) == 0U) s_ready = 0U;
    return id;
}

static int16_t MPU_ReadWord(uint8_t reg)
{
    uint8_t high = 0U;
    uint8_t low = 0U;
    if ((MPU_ReadReg(reg, &high) == 0U) || (MPU_ReadReg((uint8_t)(reg + 1U), &low) == 0U))
    {
        s_ready = 0U;
        return 0;
    }
    return (int16_t)(((uint16_t)high << 8) | low);
}

void MPU6050_GetData(int16_t *ax, int16_t *ay, int16_t *az,
                     int16_t *gx, int16_t *gy, int16_t *gz)
{
    if (ax != NULL) *ax = MPU_ReadWord(MPU_ACCEL_XOUT_H);
    if (ay != NULL) *ay = MPU_ReadWord(MPU_ACCEL_XOUT_H + 2U);
    if (az != NULL) *az = MPU_ReadWord(MPU_ACCEL_XOUT_H + 4U);
    if (gx != NULL) *gx = MPU_ReadWord(MPU_GYRO_XOUT_H);
    if (gy != NULL) *gy = MPU_ReadWord(MPU_GYRO_XOUT_H + 2U);
    if (gz != NULL) *gz = MPU_ReadWord(MPU_GYRO_XOUT_H + 4U);
}

int16_t MPU6050_GetGyroZ(void) { return MPU_ReadWord(MPU_GYRO_ZOUT_H); }
