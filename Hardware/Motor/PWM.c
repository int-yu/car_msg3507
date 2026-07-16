#include "Hardware/Motor/PWM.h"
#include "ti_msp_dl_config.h"

#define MOTOR_PWM_PERIOD_TICKS 1600U

static uint16_t PWM_DutyToCompare(uint16_t duty)
{
    uint32_t highTicks;
    if (duty > PWM_MAX_DUTY) duty = PWM_MAX_DUTY;
    highTicks = ((uint32_t)duty * MOTOR_PWM_PERIOD_TICKS) / PWM_MAX_DUTY;
    return (uint16_t)(MOTOR_PWM_PERIOD_TICKS - highTicks);
}

void PWM_Init(void)
{
    PWM_SetCompareA(0U);
    PWM_SetCompareB(0U);
    DL_TimerG_startCounter(MOTOR_PWM_INST);
}

void PWM_SetCompareA(uint16_t compare)
{
    DL_TimerG_setCaptureCompareValue(
        MOTOR_PWM_INST, PWM_DutyToCompare(compare), GPIO_MOTOR_PWM_C0_IDX);
}

void PWM_SetCompareB(uint16_t compare)
{
    DL_TimerG_setCaptureCompareValue(
        MOTOR_PWM_INST, PWM_DutyToCompare(compare), GPIO_MOTOR_PWM_C1_IDX);
}
