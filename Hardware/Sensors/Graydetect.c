#include "Hardware/Sensors/Graydetect.h"
#include "ti_msp_dl_config.h"
/* NULL 来自 stddef.h、UINT32_MAX 来自 stdint.h：不依赖 SDK 头的传递包含。 */
#include <stddef.h>
#include <stdint.h>

#if GRAYDETECT_ENABLED

#define GRAY_MASK_ALL ((uint8_t)((1U << GRAY_CHANNEL_COUNT) - 1U))

/* 32 MHz 下约 5 us；软件 I2C 选择保守的约 100 kHz 时序。 */
#define GRAYDETECT_I2C_HALF_PERIOD_CYCLES 160U
#define GRAYDETECT_I2C_CLOCK_WAIT_LOOPS   100U

#if GRAYDETECT_CHANNEL1_IS_RIGHT
#define GRAY_MASK_LEFT  0x38U /* CH4~CH6 */
#define GRAY_MASK_RIGHT 0x07U /* CH1~CH3 */
#else
#define GRAY_MASK_LEFT  0x07U
#define GRAY_MASK_RIGHT 0x38U
#endif

static uint8_t s_state;
static uint8_t s_online;
static uint8_t s_consecutiveReadFailures;
static uint32_t s_readErrorCount;

static void Graydetect_DelayHalfPeriod(void)
{
    DL_Common_delayCycles(GRAYDETECT_I2C_HALF_PERIOD_CYCLES);
}

/* 软件 I2C 只主动拉低；释放后由传感器侧的上拉电阻把线恢复高电平。 */
static void Graydetect_DriveSDALow(void)
{
    DL_GPIO_clearPins(LINE_FOLLOWER_I2C_PORT,
                      LINE_FOLLOWER_I2C_LINE_SDA_PIN);
    DL_GPIO_enableOutput(LINE_FOLLOWER_I2C_PORT,
                         LINE_FOLLOWER_I2C_LINE_SDA_PIN);
}

static void Graydetect_ReleaseSDA(void)
{
    DL_GPIO_disableOutput(LINE_FOLLOWER_I2C_PORT,
                          LINE_FOLLOWER_I2C_LINE_SDA_PIN);
}

static void Graydetect_DriveSCLLow(void)
{
    DL_GPIO_clearPins(LINE_FOLLOWER_I2C_PORT,
                      LINE_FOLLOWER_I2C_LINE_SCL_PIN);
    DL_GPIO_enableOutput(LINE_FOLLOWER_I2C_PORT,
                         LINE_FOLLOWER_I2C_LINE_SCL_PIN);
}

static void Graydetect_ReleaseSCL(void)
{
    DL_GPIO_disableOutput(LINE_FOLLOWER_I2C_PORT,
                          LINE_FOLLOWER_I2C_LINE_SCL_PIN);
}

static uint8_t Graydetect_ReadSDA(void)
{
    return (DL_GPIO_readPins(LINE_FOLLOWER_I2C_PORT,
                             LINE_FOLLOWER_I2C_LINE_SDA_PIN) != 0U) ?
        1U : 0U;
}

static uint8_t Graydetect_ReadSCL(void)
{
    return (DL_GPIO_readPins(LINE_FOLLOWER_I2C_PORT,
                             LINE_FOLLOWER_I2C_LINE_SCL_PIN) != 0U) ?
        1U : 0U;
}

static uint8_t Graydetect_ReleaseSCLAndWait(void)
{
    uint16_t attempt;

    Graydetect_ReleaseSCL();
    for (attempt = 0U; attempt < GRAYDETECT_I2C_CLOCK_WAIT_LOOPS; attempt++)
    {
        if (Graydetect_ReadSCL() != 0U)
        {
            return 1U;
        }
        Graydetect_DelayHalfPeriod();
    }
    return 0U;
}

static uint8_t Graydetect_Start(void)
{
    Graydetect_ReleaseSDA();
    if (Graydetect_ReleaseSCLAndWait() == 0U)
    {
        return 0U;
    }
    Graydetect_DelayHalfPeriod();
    if (Graydetect_ReadSDA() == 0U)
    {
        return 0U;
    }

    Graydetect_DriveSDALow();
    Graydetect_DelayHalfPeriod();
    Graydetect_DriveSCLLow();
    Graydetect_DelayHalfPeriod();
    return 1U;
}

static void Graydetect_Stop(void)
{
    Graydetect_DriveSDALow();
    Graydetect_DelayHalfPeriod();
    (void)Graydetect_ReleaseSCLAndWait();
    Graydetect_DelayHalfPeriod();
    Graydetect_ReleaseSDA();
    Graydetect_DelayHalfPeriod();
}

static uint8_t Graydetect_WriteByte(uint8_t value)
{
    uint8_t mask;

    for (mask = 0x80U; mask != 0U; mask >>= 1U)
    {
        if ((value & mask) != 0U)
        {
            Graydetect_ReleaseSDA();
        }
        else
        {
            Graydetect_DriveSDALow();
        }
        Graydetect_DelayHalfPeriod();
        if (Graydetect_ReleaseSCLAndWait() == 0U)
        {
            return 0U;
        }
        Graydetect_DelayHalfPeriod();
        Graydetect_DriveSCLLow();
        Graydetect_DelayHalfPeriod();
    }

    Graydetect_ReleaseSDA();
    Graydetect_DelayHalfPeriod();
    if (Graydetect_ReleaseSCLAndWait() == 0U)
    {
        return 0U;
    }
    Graydetect_DelayHalfPeriod();
    value = (Graydetect_ReadSDA() == 0U) ? 1U : 0U;
    Graydetect_DriveSCLLow();
    Graydetect_DelayHalfPeriod();
    return value;
}

static uint8_t Graydetect_ReadByteNack(uint8_t *value)
{
    uint8_t mask;
    uint8_t data = 0U;

    if (value == NULL)
    {
        return 0U;
    }

    Graydetect_ReleaseSDA();
    for (mask = 0x80U; mask != 0U; mask >>= 1U)
    {
        Graydetect_DelayHalfPeriod();
        if (Graydetect_ReleaseSCLAndWait() == 0U)
        {
            return 0U;
        }
        Graydetect_DelayHalfPeriod();
        if (Graydetect_ReadSDA() != 0U)
        {
            data |= mask;
        }
        Graydetect_DriveSCLLow();
        Graydetect_DelayHalfPeriod();
    }

    /* 单字节读取的最后一个时钟发送 NACK（SDA 保持释放）。 */
    Graydetect_ReleaseSDA();
    Graydetect_DelayHalfPeriod();
    if (Graydetect_ReleaseSCLAndWait() == 0U)
    {
        return 0U;
    }
    Graydetect_DelayHalfPeriod();
    Graydetect_DriveSCLLow();
    Graydetect_DelayHalfPeriod();
    *value = data;
    return 1U;
}

static uint8_t Graydetect_ReadStateRegister(uint8_t *state)
{
    uint8_t success = 0U;

    if ((state == NULL) || (Graydetect_Start() == 0U))
    {
        Graydetect_Stop();
        return 0U;
    }

    if ((Graydetect_WriteByte((uint8_t)(GRAYDETECT_I2C_ADDRESS << 1U)) != 0U) &&
        (Graydetect_WriteByte(GRAYDETECT_STATE_REGISTER) != 0U) &&
        (Graydetect_Start() != 0U) &&
        (Graydetect_WriteByte((uint8_t)((GRAYDETECT_I2C_ADDRESS << 1U) | 1U)) != 0U) &&
        (Graydetect_ReadByteNack(state) != 0U))
    {
        success = 1U;
    }

    Graydetect_Stop();
    return success;
}

static uint8_t Graydetect_GetMask(uint8_t side)
{
    if (side == GRAY_SIDE_LEFT)
    {
        return GRAY_MASK_LEFT;
    }
    if (side == GRAY_SIDE_RIGHT)
    {
        return GRAY_MASK_RIGHT;
    }
    return GRAY_MASK_ALL;
}

void Graydetect_Init(void)
{
    s_state = 0U;
    s_online = 0U;
    s_consecutiveReadFailures = 0U;
    s_readErrorCount = 0U;

    DL_GPIO_initDigitalInputFeatures(
        LINE_FOLLOWER_I2C_LINE_SDA_IOMUX, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE,
        DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(
        LINE_FOLLOWER_I2C_LINE_SCL_IOMUX, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE,
        DL_GPIO_WAKEUP_DISABLE);
    Graydetect_ReleaseSDA();
    Graydetect_ReleaseSCL();
}

void Graydetect_Update(void)
{
    uint8_t state;

    if (Graydetect_ReadStateRegister(&state) != 0U)
    {
        s_state = state & GRAY_MASK_ALL;
        s_online = 1U;
        s_consecutiveReadFailures = 0U;
    }
    else
    {
        /* 软件 I2C 的单次 NACK 不能立刻把车辆从有效循迹切到急停：
         * 保留最近一帧最多 20 ms，第三次连续失败才宣布传感器离线。 */
        if (s_consecutiveReadFailures < UINT8_MAX)
        {
            s_consecutiveReadFailures++;
        }
        if (s_consecutiveReadFailures >= GRAYDETECT_OFFLINE_CONFIRM_FAILURES)
        {
            s_state = 0U;
            s_online = 0U;
        }
        if (s_readErrorCount < UINT32_MAX)
        {
            s_readErrorCount++;
        }
    }
}

uint8_t Graydetect_IsOnline(void)
{
    return s_online;
}

uint32_t Graydetect_GetReadErrorCount(void)
{
    return s_readErrorCount;
}

uint8_t Graydetect_GetState(void)
{
    return s_state;
}

uint8_t Graydetect_GetBit(uint8_t index)
{
    return (index < GRAY_CHANNEL_COUNT) ?
        (uint8_t)((s_state >> index) & 1U) : 0U;
}

float Graydetect_GetError(uint8_t side)
{
    uint8_t state = s_state & Graydetect_GetMask(side);
    float center = (float)(GRAY_CHANNEL_COUNT - 1U) / 2.0f;
    float sum = 0.0f;
    float count = 0.0f;
    uint8_t index;

    for (index = 0U; index < GRAY_CHANNEL_COUNT; index++)
    {
        if ((state & (uint8_t)(1U << index)) != 0U)
        {
#if GRAYDETECT_CHANNEL1_IS_RIGHT
            sum += center - (float)index;
#else
            sum += (float)index - center;
#endif
            count += 1.0f;
        }
    }
    return (count > 0.0f) ? (sum / count) : 0.0f;
}

uint8_t Graydetect_OnLine(uint8_t side)
{
    return ((s_state & Graydetect_GetMask(side)) != 0U) ? 1U : 0U;
}

#else

void Graydetect_Init(void)
{
}

void Graydetect_Update(void)
{
}

uint8_t Graydetect_IsOnline(void)
{
    return 0U;
}

uint32_t Graydetect_GetReadErrorCount(void)
{
    return 0U;
}

uint8_t Graydetect_GetState(void)
{
    return 0U;
}

uint8_t Graydetect_GetBit(uint8_t index)
{
    (void)index;
    return 0U;
}

float Graydetect_GetError(uint8_t side)
{
    (void)side;
    return 0.0f;
}

uint8_t Graydetect_OnLine(uint8_t side)
{
    (void)side;
    return 0U;
}

#endif
