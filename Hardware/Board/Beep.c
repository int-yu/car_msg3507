#include "Hardware/Board/Beep.h"
#include "Hardware/Board/LED.h"
#include "ti_msp_dl_config.h"

#define BEEP_SHORT_TICKS 10U
#define BEEP_GAP_TICKS   8U
#define BEEP_LONG_TICKS  60U

typedef enum { BEEP_IDLE = 0, BEEP_ON, BEEP_GAP } BeepState_t;

static BeepState_t s_state;
static uint8_t s_remaining;
static uint16_t s_ticks;
static uint16_t s_onTicks;

void Beep_On(void)
{
    DL_GPIO_clearPins(BOARD_OUTPUTS_BEEP_PORT, BOARD_OUTPUTS_BEEP_PIN);
    LED2_ON();
}

void Beep_Off(void)
{
    DL_GPIO_setPins(BOARD_OUTPUTS_BEEP_PORT, BOARD_OUTPUTS_BEEP_PIN);
    LED2_OFF();
}

void Beep_Init(void)
{
    s_state = BEEP_IDLE;
    s_remaining = 0U;
    s_ticks = 0U;
    s_onTicks = BEEP_SHORT_TICKS;
    Beep_Off();
}

void Beep_Notify(uint8_t times)
{
    if (times == 0U) return;
    s_remaining = times;
    s_onTicks = BEEP_SHORT_TICKS;
    s_ticks = s_onTicks;
    s_state = BEEP_ON;
    Beep_On();
}

void Beep_Long(void)
{
    s_remaining = 1U;
    s_onTicks = BEEP_LONG_TICKS;
    s_ticks = s_onTicks;
    s_state = BEEP_ON;
    Beep_On();
}

void Beep_Tick(void)
{
    if (s_state == BEEP_IDLE) return;
    if ((s_ticks > 0U) && (--s_ticks > 0U)) return;

    if (s_state == BEEP_ON)
    {
        Beep_Off();
        if (s_remaining > 0U) s_remaining--;
        if (s_remaining == 0U) s_state = BEEP_IDLE;
        else { s_ticks = BEEP_GAP_TICKS; s_state = BEEP_GAP; }
    }
    else
    {
        s_ticks = s_onTicks;
        s_state = BEEP_ON;
        Beep_On();
    }
}
