#include "Hardware/Comms/Serial.h"
#include "ti_msp_dl_config.h"
#include <stddef.h>

volatile uint8_t Serial1_RxFlag;

static volatile uint32_t s_writeIndex;
static volatile uint32_t s_readIndex;
static uint8_t s_rxBuffer[SERIAL1_RX_BUFFER_SIZE];

static volatile uint32_t s_txWriteIndex;
static volatile uint32_t s_txReadIndex;
static uint8_t s_txBuffer[SERIAL1_TX_BUFFER_SIZE];

static volatile uint32_t s_serial2WriteIndex;
static volatile uint32_t s_serial2ReadIndex;
static uint8_t s_serial2RxBuffer[SERIAL2_RX_BUFFER_SIZE];

void Serial1_Init(void)
{
    s_writeIndex = 0U;
    s_readIndex = 0U;
    s_txWriteIndex = 0U;
    s_txReadIndex = 0U;
    Serial1_RxFlag = 0U;

    /* 蓝牙未连接时使用上拉保持 RX 为确定的高电平。 */
    DL_GPIO_initPeripheralInputFunctionFeatures(
        GPIO_BLUETOOTH_UART_IOMUX_RX, GPIO_BLUETOOTH_UART_IOMUX_RX_FUNC,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_UART_Main_disableInterrupt(
        BLUETOOTH_UART_INST, DL_UART_MAIN_INTERRUPT_TX);
    NVIC_ClearPendingIRQ(BLUETOOTH_UART_INST_INT_IRQN);
    NVIC_EnableIRQ(BLUETOOTH_UART_INST_INT_IRQN);
}

uint32_t Serial1_Available(void)
{
    return s_writeIndex - s_readIndex;
}

uint8_t Serial1_ReadByte(uint8_t *byte)
{
    if ((byte == NULL) || (s_readIndex == s_writeIndex))
    {
        return 0U;
    }

    *byte = s_rxBuffer[s_readIndex % SERIAL1_RX_BUFFER_SIZE];
    s_readIndex++;
    if (s_readIndex == s_writeIndex)
    {
        Serial1_RxFlag = 0U;
    }
    return 1U;
}

uint8_t Serial1_QueueArray(const uint8_t *array, uint16_t length)
{
    uint16_t index;

    if ((array == NULL) || (length == 0U) ||
        (length > SERIAL1_TX_BUFFER_SIZE))
    {
        return 0U;
    }
    if ((s_txWriteIndex - s_txReadIndex) >
        (SERIAL1_TX_BUFFER_SIZE - length))
    {
        return 0U;
    }

    /* 先写数据再发布写指针，避免 ISR 读到尚未填好的字节。 */
    for (index = 0U; index < length; index++)
    {
        s_txBuffer[(s_txWriteIndex + index) % SERIAL1_TX_BUFFER_SIZE] =
            array[index];
    }
    s_txWriteIndex += length;
    DL_UART_Main_enableInterrupt(
        BLUETOOTH_UART_INST, DL_UART_MAIN_INTERRUPT_TX);
    return 1U;
}

void Serial2_Init(void)
{
    s_serial2WriteIndex = 0U;
    s_serial2ReadIndex = 0U;
    /* F32C 未上电时保持 RX 为确定的高电平。 */
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
    DL_UART_IIDX interruptIndex;

    do
    {
        interruptIndex = DL_UART_Main_getPendingInterrupt(
            BLUETOOTH_UART_INST);
        if (interruptIndex == DL_UART_MAIN_IIDX_RX)
        {
            while (!DL_UART_Main_isRXFIFOEmpty(BLUETOOTH_UART_INST))
            {
                uint8_t data = DL_UART_Main_receiveData(
                    BLUETOOTH_UART_INST);
                s_rxBuffer[s_writeIndex % SERIAL1_RX_BUFFER_SIZE] = data;
                s_writeIndex++;
                if ((s_writeIndex - s_readIndex) >
                    SERIAL1_RX_BUFFER_SIZE)
                {
                    s_readIndex = s_writeIndex - SERIAL1_RX_BUFFER_SIZE;
                }
                Serial1_RxFlag = 1U;
            }
        }
        else if (interruptIndex == DL_UART_MAIN_IIDX_TX)
        {
            while ((s_txReadIndex != s_txWriteIndex) &&
                   (!DL_UART_Main_isTXFIFOFull(BLUETOOTH_UART_INST)))
            {
                DL_UART_Main_transmitData(
                    BLUETOOTH_UART_INST,
                    s_txBuffer[s_txReadIndex % SERIAL1_TX_BUFFER_SIZE]);
                s_txReadIndex++;
            }
            if (s_txReadIndex == s_txWriteIndex)
            {
                DL_UART_Main_disableInterrupt(
                    BLUETOOTH_UART_INST, DL_UART_MAIN_INTERRUPT_TX);
            }
        }
    } while (interruptIndex != DL_UART_MAIN_IIDX_NO_INTERRUPT);
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
