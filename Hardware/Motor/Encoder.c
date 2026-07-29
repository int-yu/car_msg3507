#include "Hardware/Motor/Encoder.h"
#include "ti_msp_dl_config.h"

#define LEFT_ENCODER_SIGN  (-1)
#define RIGHT_ENCODER_SIGN (+1)

static volatile int32_t s_leftCount;
static volatile int32_t s_rightCount;
static uint8_t s_leftState;
static uint8_t s_rightState;

static const int8_t s_quadratureDelta[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
};

static uint8_t Encoder_ReadState(uint32_t pinA, uint32_t pinB)
{
    uint32_t pins = DL_GPIO_readPins(ENCODER_INPUTS_PORT, pinA | pinB);
    return (uint8_t)(((pins & pinA) != 0U ? 2U : 0U) | ((pins & pinB) != 0U ? 1U : 0U));
}

void Encoder_Init(void)
{
    const uint32_t interruptMask = ENCODER_INPUTS_LEFT_A_PIN | ENCODER_INPUTS_LEFT_B_PIN |
                                   ENCODER_INPUTS_RIGHT_A_PIN | ENCODER_INPUTS_RIGHT_B_PIN;

    s_leftCount = 0;
    s_rightCount = 0;
    s_leftState = Encoder_ReadState(ENCODER_INPUTS_LEFT_A_PIN, ENCODER_INPUTS_LEFT_B_PIN);
    s_rightState = Encoder_ReadState(ENCODER_INPUTS_RIGHT_A_PIN, ENCODER_INPUTS_RIGHT_B_PIN);
    DL_GPIO_clearInterruptStatus(ENCODER_INPUTS_PORT, interruptMask);
    DL_GPIO_enableInterrupt(ENCODER_INPUTS_PORT, interruptMask);
    NVIC_ClearPendingIRQ(ENCODER_INPUTS_INT_IRQN);
    NVIC_EnableIRQ(ENCODER_INPUTS_INT_IRQN);
}

int16_t Encoder_Get(uint8_t n)
{
    int32_t value = 0;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    if (n == 1U) { value = s_leftCount; s_leftCount = 0; }
    else if (n == 2U) { value = s_rightCount; s_rightCount = 0; }
    __set_PRIMASK(primask);
    if (value > 32767) value = 32767;
    if (value < -32768) value = -32768;
    return (int16_t)value;
}

/*
 * GROUP1 是 GPIOA、GPIOB、COMP0/1/2、TRNG 共用的一个中断向量，本函数是它
 * 唯一的入口。下面只处理编码器所在的 GPIOA 四个引脚，但**必须**把同组其它
 * 已使能的挂起中断也清掉：组内任何一个源没被清，中断线就一直拉着，而
 * GROUP1 是优先级 0，会把主循环彻底饿死（现象即 OLED 冻结、车不动）。
 *
 * 现在 GPIOB 没有任何引脚开中断，这段兜底是空转；但只要以后给 GPIOB 加边沿
 * 中断（按键改中断触发、超声波 echo 捕获、对射传感器、比较器过零等），没有
 * 它就是一个必然踩到的死机。刻意不改动上面的 GPIOA 分派逻辑——组 IIDX 寄存器
 * 读一次只返回并清一个源，按它重写分派容易漏掉编码器脉冲、让里程和速度失真。
 */
void GROUP1_IRQHandler(void)
{
    const uint32_t mask = ENCODER_INPUTS_LEFT_A_PIN | ENCODER_INPUTS_LEFT_B_PIN |
                          ENCODER_INPUTS_RIGHT_A_PIN | ENCODER_INPUTS_RIGHT_B_PIN;
    uint32_t pending = DL_GPIO_getEnabledInterruptStatus(ENCODER_INPUTS_PORT, mask);

    if ((pending & (ENCODER_INPUTS_LEFT_A_PIN | ENCODER_INPUTS_LEFT_B_PIN)) != 0U)
    {
        uint8_t next = Encoder_ReadState(ENCODER_INPUTS_LEFT_A_PIN, ENCODER_INPUTS_LEFT_B_PIN);
        s_leftCount += (int32_t)(LEFT_ENCODER_SIGN * s_quadratureDelta[(s_leftState << 2) | next]);
        s_leftState = next;
    }
    if ((pending & (ENCODER_INPUTS_RIGHT_A_PIN | ENCODER_INPUTS_RIGHT_B_PIN)) != 0U)
    {
        uint8_t next = Encoder_ReadState(ENCODER_INPUTS_RIGHT_A_PIN, ENCODER_INPUTS_RIGHT_B_PIN);
        s_rightCount += (int32_t)(
            RIGHT_ENCODER_SIGN *
            s_quadratureDelta[(s_rightState << 2) | next]);
        s_rightState = next;
    }
    DL_GPIO_clearInterruptStatus(ENCODER_INPUTS_PORT, pending & mask);

    /* 兜底：清掉本组其余已使能的挂起中断（GPIOA 非编码器引脚 + 整个 GPIOB），
     * 避免将来新增的中断源在这里清不掉而把中断线永久拉住。 */
    {
        uint32_t others =
            DL_GPIO_getEnabledInterruptStatus(ENCODER_INPUTS_PORT, ~mask);

        if (others != 0U)
        {
            DL_GPIO_clearInterruptStatus(ENCODER_INPUTS_PORT, others);
        }
        others = DL_GPIO_getEnabledInterruptStatus(GPIOB, 0xFFFFFFFFU);
        if (others != 0U)
        {
            DL_GPIO_clearInterruptStatus(GPIOB, others);
        }
    }
}
