#include "Hardware/Motor/Encoder.h"
#include "Hardware/Motor/EncoderStepper.h"
#include "Hardware/Motor/Stepper.h"
#include "ti_msp_dl_config.h"

#define LEFT_ENCODER_SIGN  (-1)
#define RIGHT_ENCODER_SIGN (+1)

#ifndef STEPPER_ENCODER_SIGN
#define STEPPER_ENCODER_SIGN (+1)
#endif

static volatile int32_t s_leftCount;
static volatile int32_t s_rightCount;
#if STEPPER_ENABLED
static volatile int32_t s_stepperCount;
static volatile uint32_t s_stepperTransitionErrors;
#endif
static uint8_t s_leftState;
static uint8_t s_rightState;
#if STEPPER_ENABLED
static uint8_t s_stepperState;
#endif

static const int8_t s_quadratureDelta[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
};

static uint8_t Encoder_ReadState(
    GPIO_Regs *portA, uint32_t pinA, GPIO_Regs *portB, uint32_t pinB)
{
    uint32_t levelA = DL_GPIO_readPins(portA, pinA);
    uint32_t levelB = DL_GPIO_readPins(portB, pinB);

    return (uint8_t)(((levelA & pinA) != 0U ? 2U : 0U) |
                     ((levelB & pinB) != 0U ? 1U : 0U));
}

void Encoder_Init(void)
{
    const uint32_t interruptMask = ENCODER_INPUTS_LEFT_A_PIN | ENCODER_INPUTS_LEFT_B_PIN |
                                   ENCODER_INPUTS_RIGHT_A_PIN | ENCODER_INPUTS_RIGHT_B_PIN;

    s_leftCount = 0;
    s_rightCount = 0;
    s_leftState = Encoder_ReadState(
        ENCODER_INPUTS_PORT,
        ENCODER_INPUTS_LEFT_A_PIN,
        ENCODER_INPUTS_PORT,
        ENCODER_INPUTS_LEFT_B_PIN);
    s_rightState = Encoder_ReadState(
        ENCODER_INPUTS_PORT,
        ENCODER_INPUTS_RIGHT_A_PIN,
        ENCODER_INPUTS_PORT,
        ENCODER_INPUTS_RIGHT_B_PIN);
    DL_GPIO_clearInterruptStatus(ENCODER_INPUTS_PORT, interruptMask);
    DL_GPIO_enableInterrupt(ENCODER_INPUTS_PORT, interruptMask);
    NVIC_ClearPendingIRQ(GPIOA_INT_IRQn);
    NVIC_EnableIRQ(GPIOA_INT_IRQn);
}

void Encoder_InitStepper(void)
{
#if STEPPER_ENABLED
    DL_GPIO_initDigitalInputFeatures(STEPPER_ENCODER_A_A_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(STEPPER_ENCODER_B_B_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_setLowerPinsPolarity(
        STEPPER_ENCODER_A_PORT, DL_GPIO_PIN_13_EDGE_RISE_FALL);
    DL_GPIO_setUpperPinsPolarity(
        STEPPER_ENCODER_B_PORT, DL_GPIO_PIN_29_EDGE_RISE_FALL);

    s_stepperCount = 0;
    s_stepperTransitionErrors = 0U;
    s_stepperState = Encoder_ReadState(
        STEPPER_ENCODER_A_PORT,
        STEPPER_ENCODER_A_A_PIN,
        STEPPER_ENCODER_B_PORT,
        STEPPER_ENCODER_B_B_PIN);
    DL_GPIO_clearInterruptStatus(
        STEPPER_ENCODER_A_PORT, STEPPER_ENCODER_A_A_PIN);
    DL_GPIO_clearInterruptStatus(
        STEPPER_ENCODER_B_PORT, STEPPER_ENCODER_B_B_PIN);
    DL_GPIO_enableInterrupt(
        STEPPER_ENCODER_A_PORT, STEPPER_ENCODER_A_A_PIN);
    DL_GPIO_enableInterrupt(
        STEPPER_ENCODER_B_PORT, STEPPER_ENCODER_B_B_PIN);
    NVIC_ClearPendingIRQ(GPIOA_INT_IRQn);
    NVIC_EnableIRQ(GPIOA_INT_IRQn);
#endif
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

int32_t Encoder_GetStepperCount(void)
{
#if STEPPER_ENABLED
    int32_t count;
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    count = s_stepperCount;
    __set_PRIMASK(primask);
    return count;
#else
    return 0;
#endif
}

void Encoder_SetStepperCount(int32_t count)
{
#if STEPPER_ENABLED
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    s_stepperCount = count;
    __set_PRIMASK(primask);
#else
    (void)count;
#endif
}

uint32_t Encoder_GetStepperTransitionErrors(void)
{
#if STEPPER_ENABLED
    uint32_t errors;
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    errors = s_stepperTransitionErrors;
    __set_PRIMASK(primask);
    return errors;
#else
    return 0U;
#endif
}

/*
 * GROUP1 是 GPIOA、GPIOB、COMP0/1/2、TRNG 共用的一个中断向量，本函数负责
 * 其中 GPIOA/GPIOB 的分派。下面处理左右轮与步进编码器所在的 GPIOA 六个引脚，
 * 并把 GPIOA/GPIOB 其它已使能的挂起中断也清掉：GPIO 源没被清，中断线就一直拉着，而
 * GROUP1 是优先级 0，会把主循环彻底饿死（现象即 OLED 冻结、车不动）。
 *
 * 现在 GPIOB 没有任何引脚开中断，这段兜底是空转；但只要以后给 GPIOB 加边沿
 * 中断（按键改中断触发、超声波 echo 捕获、对射传感器、比较器过零等），没有
 * 它就是一个必然踩到的死机。刻意不改动上面的 GPIOA 分派逻辑——组 IIDX 寄存器
 * 读一次只返回并清一个源，按它重写分派容易漏掉编码器脉冲、让里程和速度失真。
 */
void GROUP1_IRQHandler(void)
{
    const uint32_t wheelMask =
        ENCODER_INPUTS_LEFT_A_PIN | ENCODER_INPUTS_LEFT_B_PIN |
        ENCODER_INPUTS_RIGHT_A_PIN | ENCODER_INPUTS_RIGHT_B_PIN;
    uint32_t wheelPending = DL_GPIO_getEnabledInterruptStatus(
        ENCODER_INPUTS_PORT, wheelMask);
#if STEPPER_ENABLED
    const uint32_t handledGpioAMask =
        wheelMask | STEPPER_ENCODER_A_A_PIN | STEPPER_ENCODER_B_B_PIN;
    uint32_t stepperAPending = DL_GPIO_getEnabledInterruptStatus(
        STEPPER_ENCODER_A_PORT, STEPPER_ENCODER_A_A_PIN);
    uint32_t stepperBPending = DL_GPIO_getEnabledInterruptStatus(
        STEPPER_ENCODER_B_PORT, STEPPER_ENCODER_B_B_PIN);
#else
    const uint32_t handledGpioAMask = wheelMask;
#endif

    if ((wheelPending &
         (ENCODER_INPUTS_LEFT_A_PIN | ENCODER_INPUTS_LEFT_B_PIN)) != 0U)
    {
        uint8_t next = Encoder_ReadState(
            ENCODER_INPUTS_PORT,
            ENCODER_INPUTS_LEFT_A_PIN,
            ENCODER_INPUTS_PORT,
            ENCODER_INPUTS_LEFT_B_PIN);
        s_leftCount += (int32_t)(LEFT_ENCODER_SIGN * s_quadratureDelta[(s_leftState << 2) | next]);
        s_leftState = next;
    }
    if ((wheelPending &
         (ENCODER_INPUTS_RIGHT_A_PIN | ENCODER_INPUTS_RIGHT_B_PIN)) != 0U)
    {
        uint8_t next = Encoder_ReadState(
            ENCODER_INPUTS_PORT,
            ENCODER_INPUTS_RIGHT_A_PIN,
            ENCODER_INPUTS_PORT,
            ENCODER_INPUTS_RIGHT_B_PIN);
        s_rightCount += (int32_t)(
            RIGHT_ENCODER_SIGN *
            s_quadratureDelta[(s_rightState << 2) | next]);
        s_rightState = next;
    }
#if STEPPER_ENABLED
    if ((stepperAPending | stepperBPending) != 0U)
    {
        uint8_t next = Encoder_ReadState(
            STEPPER_ENCODER_A_PORT,
            STEPPER_ENCODER_A_A_PIN,
            STEPPER_ENCODER_B_PORT,
            STEPPER_ENCODER_B_B_PIN);
        uint8_t transition = (uint8_t)((s_stepperState << 2) | next);

        if ((uint8_t)(s_stepperState ^ next) == 3U)
        {
            s_stepperTransitionErrors++;
        }
        else
        {
            s_stepperCount +=
                (int32_t)(STEPPER_ENCODER_SIGN * s_quadratureDelta[transition]);
        }
        s_stepperState = next;
    }
#endif
    DL_GPIO_clearInterruptStatus(
        ENCODER_INPUTS_PORT, wheelPending & wheelMask);
#if STEPPER_ENABLED
    DL_GPIO_clearInterruptStatus(
        STEPPER_ENCODER_A_PORT,
        stepperAPending & STEPPER_ENCODER_A_A_PIN);
    DL_GPIO_clearInterruptStatus(
        STEPPER_ENCODER_B_PORT,
        stepperBPending & STEPPER_ENCODER_B_B_PIN);
#endif

    /* 兜底：清掉本组其余已使能的挂起中断（GPIOA 非编码器引脚 + 整个 GPIOB），
     * 避免将来新增的中断源在这里清不掉而把中断线永久拉住。 */
    {
        uint32_t others = DL_GPIO_getEnabledInterruptStatus(
            ENCODER_INPUTS_PORT, ~handledGpioAMask);

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
