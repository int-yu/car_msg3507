#include "Hardware/Comms/Serial.h"
#include "ti_msp_dl_config.h"
#include <stdarg.h>
#include <stdio.h>

volatile uint8_t Serial1_RxFlag;

static volatile uint32_t s_writeIndex;
static volatile uint32_t s_readIndex;
static uint8_t s_rxBuffer[SERIAL1_RX_BUFFER_SIZE];

static volatile uint32_t s_serial2WriteIndex;
static volatile uint32_t s_serial2ReadIndex;
static uint8_t s_serial2RxBuffer[SERIAL2_RX_BUFFER_SIZE];

void Serial1_Init(void)
{
    s_writeIndex = 0U;
    s_readIndex = 0U;
    Serial1_RxFlag = 0U;
    /*
     * PB7 使用上拉，避免蓝牙模块断开或未供电时 RX 悬空并产生伪数据。
     * 本配置必须在 SYSCFG_DL_init() 之后执行，以重新应用带上拉的外设输入功能。
     */
    DL_GPIO_initPeripheralInputFunctionFeatures(
        GPIO_BLUETOOTH_UART_IOMUX_RX, GPIO_BLUETOOTH_UART_IOMUX_RX_FUNC,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    NVIC_ClearPendingIRQ(BLUETOOTH_UART_INST_INT_IRQN);
    NVIC_EnableIRQ(BLUETOOTH_UART_INST_INT_IRQN);
}

uint32_t Serial1_Available(void)
{
    return s_writeIndex - s_readIndex;
}

uint8_t Serial1_ReadByte(uint8_t *byte)
{
    if ((byte == NULL) || (s_readIndex == s_writeIndex)) return 0U;
    *byte = s_rxBuffer[s_readIndex % SERIAL1_RX_BUFFER_SIZE];
    s_readIndex++;
    if (s_readIndex == s_writeIndex) Serial1_RxFlag = 0U;
    return 1U;
}

void Serial1_SendByte(uint8_t byte)
{
    DL_UART_Main_transmitDataBlocking(BLUETOOTH_UART_INST, byte);
}

void Serial1_SendArray(const uint8_t *array, uint16_t length)
{
    uint16_t i;
    if (array == NULL) return;
    for (i = 0U; i < length; i++) Serial1_SendByte(array[i]);
}

void Serial1_SendString(const char *string)
{
    if (string == NULL) return;
    while (*string != '\0') Serial1_SendByte((uint8_t)*string++);
}

void Serial1_Printf(const char *format, ...)
{
    char string[192];
    va_list args;
    va_start(args, format);
    (void)vsnprintf(string, sizeof(string), format, args);
    va_end(args);
    Serial1_SendString(string);
}

void Serial2_Init(void)
{
    s_serial2WriteIndex = 0U;
    s_serial2ReadIndex = 0U;
    /* F32C 未上电时保持 RX 为确定的高电平，避免悬空产生伪字节。 */
    DL_GPIO_initPeripheralInputFunctionFeatures(
        GPIO_BRUSHLESS_UART_IOMUX_RX,
        GPIO_BRUSHLESS_UART_IOMUX_RX_FUNC,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    NVIC_ClearPendingIRQ(BRUSHLESS_UART_INST_INT_IRQN);
    NVIC_EnableIRQ(BRUSHLESS_UART_INST_INT_IRQN);
}

uint32_t Serial2_Available(void)
{
    return s_serial2WriteIndex - s_serial2ReadIndex;
}

uint8_t Serial2_ReadByte(uint8_t *byte)
{
    if ((byte == NULL) ||
        (s_serial2ReadIndex == s_serial2WriteIndex))
    {
        return 0U;
    }
    *byte = s_serial2RxBuffer[
        s_serial2ReadIndex % SERIAL2_RX_BUFFER_SIZE];
    s_serial2ReadIndex++;
    return 1U;
}

void Serial2_SendByte(uint8_t byte)
{
    DL_UART_Main_transmitDataBlocking(BRUSHLESS_UART_INST, byte);
}

void Serial2_SendArray(const uint8_t *array, uint16_t length)
{
    uint16_t index;

    if (array == NULL)
    {
        return;
    }
    for (index = 0U; index < length; index++)
    {
        Serial2_SendByte(array[index]);
    }
}

void UART1_IRQHandler(void)
{
    while (!DL_UART_Main_isRXFIFOEmpty(BLUETOOTH_UART_INST))
    {
        uint8_t data = DL_UART_Main_receiveData(BLUETOOTH_UART_INST);
        s_rxBuffer[s_writeIndex % SERIAL1_RX_BUFFER_SIZE] = data;
        s_writeIndex++;
        if ((s_writeIndex - s_readIndex) > SERIAL1_RX_BUFFER_SIZE)
        {
            s_readIndex = s_writeIndex - SERIAL1_RX_BUFFER_SIZE;
        }
        Serial1_RxFlag = 1U;
    }
}

void UART2_IRQHandler(void)
{
    while (!DL_UART_Main_isRXFIFOEmpty(BRUSHLESS_UART_INST))
    {
        uint8_t data = DL_UART_Main_receiveData(BRUSHLESS_UART_INST);
        s_serial2RxBuffer[
            s_serial2WriteIndex % SERIAL2_RX_BUFFER_SIZE] = data;
        s_serial2WriteIndex++;
        if ((s_serial2WriteIndex - s_serial2ReadIndex) >
            SERIAL2_RX_BUFFER_SIZE)
        {
            s_serial2ReadIndex =
                s_serial2WriteIndex - SERIAL2_RX_BUFFER_SIZE;
        }
    }
}
