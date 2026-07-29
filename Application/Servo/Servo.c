#include "Application/Servo/Servo.h"
#if SERVO_ENABLED
#include "ti_msp_dl_config.h"

#if (SERVO_PWM_INST_CLK_FREQ != 1000000)
#error "Servo pulse conversion requires a 1 MHz TIMG7 clock"
#endif
_Static_assert((GPIO_SERVO_PWM_C0_PIN == DL_GPIO_PIN_26) &&
                   (GPIO_SERVO_PWM_C0_IOMUX == IOMUX_PINCM59) &&
                   (GPIO_SERVO_PWM_C0_IOMUX_FUNC ==
                    IOMUX_PINCM59_PF_TIMG7_CCP0),
               "Servo horizontal PWM must use TIMG7 CCP0/PA26");
_Static_assert((GPIO_SERVO_PWM_C1_PIN == DL_GPIO_PIN_27) &&
                   (GPIO_SERVO_PWM_C1_IOMUX == IOMUX_PINCM60) &&
                   (GPIO_SERVO_PWM_C1_IOMUX_FUNC ==
                    IOMUX_PINCM60_PF_TIMG7_CCP1),
               "Servo vertical PWM must use TIMG7 CCP1/PA27");
#endif

static uint16_t s_verticalAngle = SERVO_VERTICAL_DEFAULT_ANGLE;
static uint16_t s_horizontalAngle = SERVO_HORIZONTAL_DEFAULT_ANGLE;

static uint16_t Servo_ClampAngle(uint16_t angle,
                                 uint16_t minimum,
                                 uint16_t maximum)
{
    if (angle < minimum)
    {
        return minimum;
    }
    if (angle > maximum)
    {
        return maximum;
    }
    return angle;
}

#if SERVO_ENABLED
static uint16_t Servo_AngleToCompare(uint16_t angle)
{
    uint32_t pulseWidthUs = SERVO_MIN_PULSE_US;

    pulseWidthUs += ((uint32_t)angle *
                     (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US)) /
                    SERVO_PHYSICAL_RANGE_DEG;

    return (uint16_t)(SERVO_FRAME_US - pulseWidthUs);
}

static void Servo_WriteCompare(uint16_t angle, DL_TIMER_CC_INDEX channel)
{
    DL_TimerG_setCaptureCompareValue(
        SERVO_PWM_INST, Servo_AngleToCompare(angle), channel);
}
#endif

void Servo_Init(void)
{
    Servo_Reset();
#if SERVO_ENABLED
    DL_TimerG_startCounter(SERVO_PWM_INST);
#endif
}

void Servo_SetVerticalAngle(uint16_t angle)
{
    s_verticalAngle = Servo_ClampAngle(angle,
                                       SERVO_VERTICAL_MIN_ANGLE,
                                       SERVO_VERTICAL_MAX_ANGLE);
#if SERVO_ENABLED
    Servo_WriteCompare(s_verticalAngle, GPIO_SERVO_PWM_C1_IDX);
#endif
}

void Servo_SetHorizontalAngle(uint16_t angle)
{
    s_horizontalAngle = Servo_ClampAngle(angle,
                                         SERVO_HORIZONTAL_MIN_ANGLE,
                                         SERVO_HORIZONTAL_MAX_ANGLE);
#if SERVO_ENABLED
    Servo_WriteCompare(s_horizontalAngle, GPIO_SERVO_PWM_C0_IDX);
#endif
}

uint16_t Servo_GetVerticalAngle(void)
{
    return s_verticalAngle;
}

uint16_t Servo_GetHorizontalAngle(void)
{
    return s_horizontalAngle;
}

void Servo_Reset(void)
{
    Servo_SetVerticalAngle(SERVO_VERTICAL_DEFAULT_ANGLE);
    Servo_SetHorizontalAngle(SERVO_HORIZONTAL_DEFAULT_ANGLE);
}
