#ifndef MAIN_STEPPER_TEST_MODE
#define MAIN_STEPPER_TEST_MODE 0U
#endif

#if MAIN_STEPPER_TEST_MODE

#include "Application/Comms/K230Link.h"
#include "Hardware/Board/Key.h"
#include "Hardware/Display/OLED.h"
#include "Hardware/Motor/Encoder.h"
#include "Hardware/Motor/Stepper.h"
#include "System/Interrupt.h"
#include "System/Tick.h"
#include "ti_msp_dl_config.h"

#include <stdbool.h>
#include <stdint.h>

#define TEST_KEY_ENABLE_MASK         0x01U
#define TEST_KEY_UP_MASK             0x02U
#define TEST_KEY_DOWN_MASK           0x04U
#define TEST_KEY_STOP_MASK           0x08U
#define TEST_KEY_EMERGENCY_MASK      (TEST_KEY_ENABLE_MASK | TEST_KEY_STOP_MASK)
#define TEST_KEY_DEBOUNCE_TICKS      3U
#define TEST_DISPLAY_REFRESH_TICKS   10U

static const Stepper_Profile_t s_testProfile = {
    .startStepRateHz = STEPPER_STARTUP_START_RATE_HZ,
    .maxStepRateHz = STEPPER_STARTUP_MAX_RATE_HZ,
    .accelerationStepsPerSec2 =
        STEPPER_STARTUP_ACCELERATION_STEPS_S2
};

static const DL_DMA_Config s_k230TxDmaConfig = {
    .transferMode = DL_DMA_SINGLE_TRANSFER_MODE,
    .extendedMode = DL_DMA_NORMAL_MODE,
    .destIncrement = DL_DMA_ADDR_UNCHANGED,
    .srcIncrement = DL_DMA_ADDR_INCREMENT,
    .destWidth = DL_DMA_WIDTH_BYTE,
    .srcWidth = DL_DMA_WIDTH_BYTE,
    .trigger = K230_UART_INST_DMA_TRIGGER,
    .triggerType = DL_DMA_TRIGGER_TYPE_EXTERNAL
};

static uint8_t s_keyCandidate;
static uint8_t s_keyStable;
static uint8_t s_keyDebounceTicks;
static uint8_t s_displayTicks;
static int32_t s_leftEncoderCount;
static int32_t s_rightEncoderCount;

static void Test_InitPower(void)
{
    DL_GPIO_reset(GPIOA);
    DL_GPIO_reset(GPIOB);
    DL_TimerA_reset(STEPPER_PULSE_INST);
    DL_TimerG_reset(STEPPER_ABS_CAPTURE_INST);
    DL_I2C_reset(OLED_I2C_INST);
    DL_UART_Main_reset(K230_UART_INST);

    DL_GPIO_enablePower(GPIOA);
    DL_GPIO_enablePower(GPIOB);
    DL_TimerA_enablePower(STEPPER_PULSE_INST);
    DL_TimerG_enablePower(STEPPER_ABS_CAPTURE_INST);
    DL_I2C_enablePower(OLED_I2C_INST);
    DL_UART_Main_enablePower(K230_UART_INST);
    delay_cycles(POWER_STARTUP_DELAY);
}

static void Test_InitPins(void)
{
    DL_GPIO_initPeripheralOutputFunctionFeatures(
        GPIO_STEPPER_PULSE_C0_IOMUX, GPIO_STEPPER_PULSE_C0_IOMUX_FUNC,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_DRIVE_STRENGTH_LOW, DL_GPIO_HIZ_DISABLE);
    DL_GPIO_enableOutput(
        GPIO_STEPPER_PULSE_C0_PORT, GPIO_STEPPER_PULSE_C0_PIN);

    DL_GPIO_initPeripheralInputFunction(
        GPIO_STEPPER_ABS_CAPTURE_C0_IOMUX,
        GPIO_STEPPER_ABS_CAPTURE_C0_IOMUX_FUNC);

    DL_GPIO_initPeripheralInputFunctionFeatures(
        GPIO_OLED_I2C_IOMUX_SDA, GPIO_OLED_I2C_IOMUX_SDA_FUNC,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initPeripheralInputFunctionFeatures(
        GPIO_OLED_I2C_IOMUX_SCL, GPIO_OLED_I2C_IOMUX_SCL_FUNC,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_enableHiZ(GPIO_OLED_I2C_IOMUX_SDA);
    DL_GPIO_enableHiZ(GPIO_OLED_I2C_IOMUX_SCL);

    DL_GPIO_initPeripheralOutputFunction(
        GPIO_K230_UART_IOMUX_TX, GPIO_K230_UART_IOMUX_TX_FUNC);
    DL_GPIO_initPeripheralInputFunctionFeatures(
        GPIO_K230_UART_IOMUX_RX, GPIO_K230_UART_IOMUX_RX_FUNC,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(ENCODER_INPUTS_RIGHT_A_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(ENCODER_INPUTS_RIGHT_B_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(ENCODER_INPUTS_LEFT_A_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(ENCODER_INPUTS_LEFT_B_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_setLowerPinsPolarity(
        ENCODER_INPUTS_PORT, DL_GPIO_PIN_15_EDGE_RISE_FALL);
    DL_GPIO_setUpperPinsPolarity(
        ENCODER_INPUTS_PORT,
        DL_GPIO_PIN_16_EDGE_RISE_FALL |
        DL_GPIO_PIN_17_EDGE_RISE_FALL |
        DL_GPIO_PIN_24_EDGE_RISE_FALL);

    DL_GPIO_initDigitalOutput(BOARD_OUTPUTS_STEPPER_DIR_IOMUX);
    DL_GPIO_initDigitalOutput(BOARD_OUTPUTS_STEPPER_EN_IOMUX);
    DL_GPIO_clearPins(BOARD_OUTPUTS_STEPPER_DIR_PORT,
        BOARD_OUTPUTS_STEPPER_DIR_PIN | BOARD_OUTPUTS_STEPPER_EN_PIN);
    DL_GPIO_enableOutput(BOARD_OUTPUTS_STEPPER_DIR_PORT,
        BOARD_OUTPUTS_STEPPER_DIR_PIN | BOARD_OUTPUTS_STEPPER_EN_PIN);
}

static uint8_t Test_GetPressedEdges(void)
{
    uint8_t raw = Key_GetPressedMask();
    uint8_t edges = 0U;

    if (raw != s_keyCandidate)
    {
        s_keyCandidate = raw;
        s_keyDebounceTicks = 1U;
    }
    else
    {
        if (s_keyDebounceTicks < TEST_KEY_DEBOUNCE_TICKS)
        {
            s_keyDebounceTicks++;
        }
        if ((s_keyDebounceTicks >= TEST_KEY_DEBOUNCE_TICKS) &&
            (s_keyStable != s_keyCandidate))
        {
            edges = (uint8_t)(s_keyCandidate & (uint8_t)~s_keyStable);
            s_keyStable = s_keyCandidate;
        }
    }

    return edges;
}

static void Test_HandleKeys(uint8_t pressedEdges)
{
    uint8_t pressedMask = s_keyStable;
    Stepper_Status_t status;

    if ((pressedMask & TEST_KEY_EMERGENCY_MASK) ==
        TEST_KEY_EMERGENCY_MASK)
    {
        if ((pressedEdges & TEST_KEY_EMERGENCY_MASK) != 0U)
        {
            Stepper_EmergencyStop();
        }
        return;
    }

    Stepper_GetStatus(&status);
    if ((pressedEdges & TEST_KEY_ENABLE_MASK) != 0U)
    {
        (void)Stepper_Enable(!status.enabled);
    }
    else if ((pressedEdges & TEST_KEY_UP_MASK) != 0U)
    {
        (void)Stepper_MoveToAngle(
            STEPPER_MAX_ANGLE_DEG, &s_testProfile);
    }
    else if ((pressedEdges & TEST_KEY_DOWN_MASK) != 0U)
    {
        (void)Stepper_MoveToAngle(
            STEPPER_MIN_ANGLE_DEG, &s_testProfile);
    }
    else if ((pressedEdges & TEST_KEY_STOP_MASK) != 0U)
    {
        Stepper_Stop();
    }
}

static void Test_ShowBallPosition(void)
{
    K230Link_BallPosition_t position;

    OLED_ShowString(0, 24, "BALL:", OLED_6X8);
    OLED_ShowString(72, 24, "H", OLED_6X8);
    OLED_ShowNum(
        78, 24, K230Link_IsReady() != 0U ? 1U : 0U, 1U, OLED_6X8);
    if (K230Link_GetBallPosition(&position) == 0U)
    {
        OLED_ShowString(30, 24, "WAIT", OLED_6X8);
    }
    else if (position.valid == 0U)
    {
        OLED_ShowString(30, 24, "LOST", OLED_6X8);
    }
    else
    {
        int32_t value = position.positionX100;
        uint32_t magnitude = (value < 0) ?
            (uint32_t)(-value) : (uint32_t)value;

        OLED_ShowChar(30, 24, (value < 0) ? '-' : '+', OLED_6X8);
        OLED_ShowNum(36, 24, magnitude / 100U, 2U, OLED_6X8);
        OLED_ShowChar(48, 24, '.', OLED_6X8);
        OLED_ShowNum(54, 24, magnitude % 100U, 2U, OLED_6X8);
    }
}

static void Test_UpdateDisplay(void)
{
    Stepper_Status_t status;
    uint32_t angleTenths;
    uint8_t index;

    if (OLED_IsReady() == 0U)
    {
        return;
    }

    Stepper_GetStatus(&status);
    angleTenths =
        ((uint32_t)status.absoluteCode * 3600U + 2048U) / 4096U;

    OLED_Clear();
    OLED_ShowString(0, 0, "KEY:", OLED_6X8);
    for (index = 0U; index < 4U; index++)
    {
        OLED_ShowChar((int16_t)(24 + index * 6U), 0,
            ((s_keyStable & (1U << index)) != 0U) ? '1' : '0',
            OLED_6X8);
    }
    OLED_ShowString(0, 8, "SAB:", OLED_6X8);
    OLED_ShowSignedNum(24, 8, status.encoderCounts, 8U, OLED_6X8);
    OLED_ShowString(0, 16, "ST:", OLED_6X8);
    OLED_ShowSignedNum(18, 16, status.emittedSteps, 8U, OLED_6X8);
    Test_ShowBallPosition();
    OLED_ShowString(0, 32, "LENC:", OLED_6X8);
    OLED_ShowSignedNum(30, 32, s_leftEncoderCount, 8U, OLED_6X8);
    OLED_ShowString(0, 40, "RENC:", OLED_6X8);
    OLED_ShowSignedNum(30, 40, s_rightEncoderCount, 8U, OLED_6X8);
    OLED_ShowString(0, 48, "PWM:", OLED_6X8);
    if (!status.pwmValid)
    {
        OLED_ShowString(24, 48, "LOST", OLED_6X8);
    }
    else
    {
        OLED_ShowNum(24, 48, angleTenths / 10U, 3U, OLED_6X8);
        OLED_ShowChar(42, 48, '.', OLED_6X8);
        OLED_ShowNum(48, 48, angleTenths % 10U, 1U, OLED_6X8);
    }
    OLED_Update();
}

int main(void)
{
    uint8_t elapsedTicks;

    __disable_irq();

    Test_InitPower();
    Test_InitPins();
    SYSCFG_DL_SYSCTL_init();
    SYSCFG_DL_STEPPER_PULSE_init();
    SYSCFG_DL_STEPPER_ABS_CAPTURE_init();
    SYSCFG_DL_OLED_I2C_init();
    SYSCFG_DL_K230_UART_init();
    DL_DMA_initChannel(
        DMA, DMA_K230_TX_CHAN_ID, &s_k230TxDmaConfig);
    SYSCFG_DL_SYSTICK_init();

    /* Graydetect is intentionally disabled; its former pins drive Stepper. */
    Tick_Init();
    Key_Init();
    Encoder_Init();
    K230Link_Init();
    OLED_Init();
    Stepper_Init();

    s_keyCandidate = Key_GetPressedMask();
    s_keyStable = s_keyCandidate;
    s_keyDebounceTicks = TEST_KEY_DEBOUNCE_TICKS;
    s_displayTicks = TEST_DISPLAY_REFRESH_TICKS;
    s_leftEncoderCount = 0;
    s_rightEncoderCount = 0;

    Interrupt_Enable();
    Test_UpdateDisplay();

    for (;;)
    {
        elapsedTicks = Tick_PollCount();
        if (elapsedTicks == 0U)
        {
            __WFI();
            continue;
        }

        Stepper_Update(elapsedTicks);
        K230Link_Update(elapsedTicks);
        s_leftEncoderCount += Encoder_Get(1U);
        s_rightEncoderCount += Encoder_Get(2U);
        Test_HandleKeys(Test_GetPressedEdges());

        if ((uint16_t)s_displayTicks + elapsedTicks >=
            TEST_DISPLAY_REFRESH_TICKS)
        {
            s_displayTicks = 0U;
            Test_UpdateDisplay();
        }
        else
        {
            s_displayTicks = (uint8_t)(s_displayTicks + elapsedTicks);
        }
    }
}

#else

#include "Application/Core/Main26H.h"

int main(void)
{
    Main26H_Run();
    return 0;
}

#endif
