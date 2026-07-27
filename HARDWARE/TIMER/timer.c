/*
 * 定时器驱动实现 - 定时器初始化/PWM生成/计时功能
 */

/*
 * Timer module - STM32 port of AVR-Doper timer.c (C. Starkjohann)
 *
 * AVR Timer0 -> STM32 TIM6 (basic timer, no I/O channels)
 * TIM6 clock = APB1 * 2 = 72MHz (PCLK1=36MHz, APB1 prescaler != 1)
 * PSC = 71 -> CK_CNT = 72MHz / 72 = 1MHz (1us per count)
 * ARR = 999 -> overflow every 1000 counts = 1ms
 *
 * Short timeout:  decremented every 1ms (timerTimeoutCnt)
 * Long timeout:   decremented every 100ms (timerLongTimeoutCnt)
 */

#include "timer.h"

volatile uint8_t  timerTimeoutCnt;
volatile uint8_t  timerLongTimeoutCnt;

/* prescaler for long timeout: 1ms * 100 = 100ms */
static uint8_t g_timLongPrescaler = 100;

void TIM6_IRQHandler(void)
{
    if (TIM6->SR & TIM_SR_UIF)
    {
        TIM6->SR = ~TIM_SR_UIF;  /* clear interrupt flag */

        if (timerTimeoutCnt != 0)
            timerTimeoutCnt--;

        if (--g_timLongPrescaler == 0)
        {
            g_timLongPrescaler = 100;     /* reload 100ms prescaler */
            if (timerLongTimeoutCnt != 0)
                timerLongTimeoutCnt--;
        }
    }
}

void timerInit(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;    /* enable TIM6 clock */

    TIM6->PSC = 71;                         /* 72MHz / 72 = 1MHz */
    TIM6->ARR = 999;                        /* 1MHz / 1000 = 1kHz = 1ms */
    TIM6->DIER |= TIM_DIER_UIE;             /* enable update interrupt */
    TIM6->CR1 |= TIM_CR1_CEN;               /* enable counter */

    NVIC_SetPriority(TIM6_IRQn, 2);
    NVIC_EnableIRQ(TIM6_IRQn);
}

void timerMsDelay(uint8_t ms)
{
    timerSetupTimeout(ms);
    while (!timerTimeoutOccurred());
}

void timerSetupTimeout(uint8_t msDuration)
{
    /* Add 1 unit to compensate for almost-zero delays with
     * ~1ms resolution */
    timerTimeoutCnt = msDuration + 1;
}

/*
 * timerTicksDelay(ticks) - blocking delay in "ticks".
 * In AVR-Doper firmware: one tick = 64/F_CPU seconds (~5.333us @12MHz).
 * On STM32 (TIM6 @1MHz), one tick = 5us.
 * AVR impl uses "cli(); reset TCNT0 prescaler; until = TCNT0 + ticks; sei(); poll TCNT0".
 * STM32 impl uses busy-loop reading TIM6->CNT.
 */
void timerTicksDelay(uint8_t ticks)
{
    uint16_t start, end;

    /* Disable interrupts to prevent timing disturbance (same as AVR cli()) */
    __disable_irq();

    /* Reset TIM6 counter to get a clean reference */
    TIM6->CNT = 0;
    start = 0;
    end = start + (uint16_t)ticks;
    /* Handle 16-bit wrap: if end > 65535, we need to wait for wrap */
    if (end < start)
        end = 65535;  /* wait until near-wrap, then handle remainder */

    __enable_irq();

    /* Busy-wait until TIM6->CNT reaches 'end'.
     * TIM6 CNT runs at 1MHz, but TIMER_TICK_US=5 means we multiply ticks by 5.
     * So actual wait = ticks * 5 microseconds. */
    {
        uint32_t goal = (uint32_t)ticks * TIMER_TICK_US;
        while ((uint32_t)TIM6->CNT < goal);
    }
}


