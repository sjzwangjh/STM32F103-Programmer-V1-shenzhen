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
#include "delay.h"

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
 *
 * TIM6 is also the 1ms timeout base used by timerTimeoutOccurred().
 * Resetting TIM6->CNT for each ISP bit-bang gap can indefinitely delay the
 * update interrupt during heavy page programming, which then breaks the
 * ready/busy timeout path in ispProgramMemory().
 *
 * Keep TIM6 dedicated to timeout bookkeeping and use SysTick-based delay_us()
 * for the fine-grained ISP clock spacing.
 */
void timerTicksDelay(uint8_t ticks)
{
    if (ticks == 0U)
        return;

    delay_us((u32)ticks * (u32)TIMER_TICK_US);
}


