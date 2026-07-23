#include "Hardware/Comms/Serial.h"
#include "ti_msp_dl_config.h"
#include <stdarg.h>
#include <stdio.h>

volatile uint8_t Serial1_RxFlag;

static volatile uint32_t s_writeIndex;
static volatile uint32_t s_readIndex;
static uint8_t s_rxBuffer[SERIAL1_RX_BUFFER_SIZE];

/*
 * TX 环形缓冲 + DMA 非阻塞发送。
 *
 * 单生产者（主循环，通过 Serial1_SendArray 等）单消费者（DMA 完成中断）模型：
 *   - s_txHead 只被主循环推进，s_txTail 只被 DMA 中断推进，各自天然无锁。
 *   - DMA 一次只能搬一段连续内存，所以环绕时分两次：先搬到缓冲尾，
 *     完成中断里再搬环绕后的头部。s_txDmaActive 标记 DMA 正忙。
 *   - 缓冲满时丢弃并计数，绝不阻塞等待——否则就退化回原来的阻塞发送。
 */
static uint8_t s_txBuffer[SERIAL1_TX_BUFFER_SIZE];
static volatile uint32_t s_txHead;      /* 主循环写入位置 */
static volatile uint32_t s_txTail;      /* DMA 已发送到的位置 */
static volatile uint8_t s_txDmaActive;  /* DMA 是否正在搬运 */
static volatile uint32_t s_txDmaLength; /* 当前 DMA 段长度 */
static volatile uint32_t s_txDropCount;

/* Serial1 输出捕获（从机回传用）：捕获期间输出改写入下面的缓冲而非 UART。 */
static uint8_t *s_captureBuffer;
static uint16_t s_captureCapacity;
static uint16_t s_captureLength;
static uint8_t s_captureActive;

static volatile uint32_t s_serial2WriteIndex;
static volatile uint32_t s_serial2ReadIndex;
static uint8_t s_serial2RxBuffer[SERIAL2_RX_BUFFER_SIZE];

/* Serial2 TX 环形缓冲 + DMA，模型与 Serial1 完全一致（单生产者主循环 /
 * 单消费者 DMA 完成中断）。UART2 现承载 HC05 主从链路，需非阻塞发送。 */
static uint8_t s_serial2TxBuffer[SERIAL2_TX_BUFFER_SIZE];
static volatile uint32_t s_serial2TxHead;
static volatile uint32_t s_serial2TxTail;
static volatile uint8_t s_serial2TxDmaActive;
static volatile uint32_t s_serial2TxDmaLength;
static volatile uint32_t s_serial2TxDropCount;

void Serial1_Init(void)
{
    s_writeIndex = 0U;
    s_readIndex = 0U;
    Serial1_RxFlag = 0U;
    s_txHead = 0U;
    s_txTail = 0U;
    s_txDmaActive = 0U;
    s_txDmaLength = 0U;
    s_txDropCount = 0U;
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

/* 启动一次 DMA 搬运：从 s_txTail 到「缓冲尾或 s_txHead」的连续段。
 * 必须在关中断或 DMA 空闲时调用，避免与完成中断竞争 s_txDmaActive。 */
static void Serial1_StartTxDma(void)
{
    uint32_t tail = s_txTail % SERIAL1_TX_BUFFER_SIZE;
    uint32_t pending = s_txHead - s_txTail;
    uint32_t chunk;

    if ((s_txDmaActive != 0U) || (pending == 0U))
    {
        return;
    }

    /* 一次只搬到缓冲物理末尾；跨越环绕的部分留给下一次完成中断。 */
    chunk = SERIAL1_TX_BUFFER_SIZE - tail;
    if (chunk > pending)
    {
        chunk = pending;
    }

    s_txDmaActive = 1U;
    s_txDmaLength = chunk;
    DL_DMA_setSrcAddr(DMA, DMA_BLUETOOTH_TX_CHAN_ID,
                      (uint32_t)&s_txBuffer[tail]);
    DL_DMA_setDestAddr(DMA, DMA_BLUETOOTH_TX_CHAN_ID,
                       (uint32_t)&BLUETOOTH_UART_INST->TXDATA);
    DL_DMA_setTransferSize(DMA, DMA_BLUETOOTH_TX_CHAN_ID, chunk);
    DL_DMA_enableChannel(DMA, DMA_BLUETOOTH_TX_CHAN_ID);
}

void Serial1_OnDmaTxComplete(void)
{
    /* 本段搬完，推进 tail；若还有数据（含环绕后的头部）立即启动下一段。 */
    s_txTail += s_txDmaLength;
    s_txDmaLength = 0U;
    s_txDmaActive = 0U;
    Serial1_StartTxDma();
}

void Serial1_SendArray(const uint8_t *array, uint16_t length)
{
    uint16_t i;
    uint32_t used;
    uint32_t space;

    if (array == NULL)
    {
        return;
    }

    /* 捕获模式：把输出收进捕获缓冲，不发往 UART（从机回传回应用）。 */
    if (s_captureActive != 0U)
    {
        for (i = 0U; i < length; i++)
        {
            if (s_captureLength < s_captureCapacity)
            {
                s_captureBuffer[s_captureLength] = array[i];
                s_captureLength++;
            }
        }
        return;
    }

    /* 读 tail 前先算空间。tail 可能被中断推进，只会让空间变大，
     * 因此这里读到的是保守下界，不会误判为可写而覆盖未发送数据。 */
    used = s_txHead - s_txTail;
    space = SERIAL1_TX_BUFFER_SIZE - used;
    if (length > space)
    {
        /* 整帧要么全进要么全不进——半帧进缓冲会让 PC 收到残帧。 */
        s_txDropCount += length;
        return;
    }

    for (i = 0U; i < length; i++)
    {
        s_txBuffer[s_txHead % SERIAL1_TX_BUFFER_SIZE] = array[i];
        s_txHead++;
    }

    /* 若 DMA 当前空闲就点火；正忙则完成中断会自然接上。
     * 关中断保护这个「检查—启动」，避免与完成中断同时判定 active。 */
    __disable_irq();
    Serial1_StartTxDma();
    __enable_irq();
}

void Serial1_SendByte(uint8_t byte)
{
    Serial1_SendArray(&byte, 1U);
}

void Serial1_SendString(const char *string)
{
    uint16_t length = 0U;

    if (string == NULL)
    {
        return;
    }
    while (string[length] != '\0')
    {
        length++;
    }
    Serial1_SendArray((const uint8_t *)string, length);
}

uint32_t Serial1_GetTxDropCount(void)
{
    return s_txDropCount;
}

void Serial1_BeginCapture(uint8_t *buffer, uint16_t capacity)
{
    if ((buffer == NULL) || (capacity == 0U))
    {
        return;
    }
    s_captureBuffer = buffer;
    s_captureCapacity = capacity;
    s_captureLength = 0U;
    s_captureActive = 1U;
}

uint16_t Serial1_EndCapture(void)
{
    s_captureActive = 0U;
    return s_captureLength;
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
    s_serial2TxHead = 0U;
    s_serial2TxTail = 0U;
    s_serial2TxDmaActive = 0U;
    s_serial2TxDmaLength = 0U;
    s_serial2TxDropCount = 0U;
    /* HC05（或 F32C）未上电时保持 RX 为确定的高电平，避免悬空产生伪字节。 */
    DL_GPIO_initPeripheralInputFunctionFeatures(
        GPIO_BRUSHLESS_UART_IOMUX_RX,
        GPIO_BRUSHLESS_UART_IOMUX_RX_FUNC,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    NVIC_ClearPendingIRQ(BRUSHLESS_UART_INST_INT_IRQN);
    NVIC_EnableIRQ(BRUSHLESS_UART_INST_INT_IRQN);
}

/* 与 Serial1_StartTxDma 同构：搬 s_serial2TxTail 到「缓冲尾或 head」的连续段。 */
static void Serial2_StartTxDma(void)
{
    uint32_t tail = s_serial2TxTail % SERIAL2_TX_BUFFER_SIZE;
    uint32_t pending = s_serial2TxHead - s_serial2TxTail;
    uint32_t chunk;

    if ((s_serial2TxDmaActive != 0U) || (pending == 0U))
    {
        return;
    }

    chunk = SERIAL2_TX_BUFFER_SIZE - tail;
    if (chunk > pending)
    {
        chunk = pending;
    }

    s_serial2TxDmaActive = 1U;
    s_serial2TxDmaLength = chunk;
    DL_DMA_setSrcAddr(DMA, DMA_PEERLINK_TX_CHAN_ID,
                      (uint32_t)&s_serial2TxBuffer[tail]);
    DL_DMA_setDestAddr(DMA, DMA_PEERLINK_TX_CHAN_ID,
                       (uint32_t)&BRUSHLESS_UART_INST->TXDATA);
    DL_DMA_setTransferSize(DMA, DMA_PEERLINK_TX_CHAN_ID, chunk);
    DL_DMA_enableChannel(DMA, DMA_PEERLINK_TX_CHAN_ID);
}

void Serial2_OnDmaTxComplete(void)
{
    s_serial2TxTail += s_serial2TxDmaLength;
    s_serial2TxDmaLength = 0U;
    s_serial2TxDmaActive = 0U;
    Serial2_StartTxDma();
}

uint32_t Serial2_GetTxDropCount(void)
{
    return s_serial2TxDropCount;
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

uint8_t Serial2_SendArray(const uint8_t *array, uint16_t length)
{
    uint16_t i;
    uint32_t used;
    uint32_t space;

    if ((array == NULL) || (length == 0U))
    {
        return 0U;
    }

    /* 保守下界：tail 只会被中断推大，读到的空间不会误判为可写。 */
    used = s_serial2TxHead - s_serial2TxTail;
    space = SERIAL2_TX_BUFFER_SIZE - used;
    if (length > space)
    {
        /* 整段要么全进要么全不进——半帧会让对端收到残帧。 */
        s_serial2TxDropCount += length;
        return 0U;
    }

    for (i = 0U; i < length; i++)
    {
        s_serial2TxBuffer[s_serial2TxHead % SERIAL2_TX_BUFFER_SIZE] = array[i];
        s_serial2TxHead++;
    }

    __disable_irq();
    Serial2_StartTxDma();
    __enable_irq();
    return 1U;
}

void Serial2_SendByte(uint8_t byte)
{
    (void)Serial2_SendArray(&byte, 1U);
}

void UART1_IRQHandler(void)
{
    /* 一次中断可能同时挂起 RX 与 DMA_DONE_TX；用 getPendingInterrupt 逐个取出，
     * 直到没有待处理中断为止，避免漏掉同时到达的事件。 */
    for (;;)
    {
        switch (DL_UART_Main_getPendingInterrupt(BLUETOOTH_UART_INST))
        {
            case DL_UART_MAIN_IIDX_DMA_DONE_TX:
                Serial1_OnDmaTxComplete();
                break;

            case DL_UART_MAIN_IIDX_RX:
                while (!DL_UART_Main_isRXFIFOEmpty(BLUETOOTH_UART_INST))
                {
                    uint8_t data =
                        DL_UART_Main_receiveData(BLUETOOTH_UART_INST);
                    s_rxBuffer[s_writeIndex % SERIAL1_RX_BUFFER_SIZE] = data;
                    s_writeIndex++;
                    if ((s_writeIndex - s_readIndex) > SERIAL1_RX_BUFFER_SIZE)
                    {
                        s_readIndex = s_writeIndex - SERIAL1_RX_BUFFER_SIZE;
                    }
                    Serial1_RxFlag = 1U;
                }
                break;

            case DL_UART_MAIN_IIDX_NO_INTERRUPT:
            default:
                return;
        }
    }
}

void UART2_IRQHandler(void)
{
    /* 与 UART1 同构：一次中断可能同时挂起 RX 与 DMA_DONE_TX，逐个取出直到清空。 */
    for (;;)
    {
        switch (DL_UART_Main_getPendingInterrupt(BRUSHLESS_UART_INST))
        {
            case DL_UART_MAIN_IIDX_DMA_DONE_TX:
                Serial2_OnDmaTxComplete();
                break;

            case DL_UART_MAIN_IIDX_RX:
                while (!DL_UART_Main_isRXFIFOEmpty(BRUSHLESS_UART_INST))
                {
                    uint8_t data =
                        DL_UART_Main_receiveData(BRUSHLESS_UART_INST);
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
                break;

            case DL_UART_MAIN_IIDX_NO_INTERRUPT:
            default:
                return;
        }
    }
}
