/*
 * 定时器驱动实�?- 定时器初始化/PWM生成/计时功能
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

#include "led.h"
volatile uint8_t  timerTimeoutCnt;
volatile uint8_t  timerLongTimeoutCnt;
volatile uint32_t timerMsTick;

/* prescaler for long timeout: 1ms * 100 = 100ms */
static uint8_t g_timLongPrescaler = 100;
static uint8_t g_tim7Initialized;
static uint32_t g_tim7RemainingUs;

void TIM6_IRQHandler(void)
{
    if (TIM6->SR & TIM_SR_UIF)
    {
        TIM6->SR = ~TIM_SR_UIF;  /* clear interrupt flag */
        timerMsTick++;

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
static uint32_t timerLedPwmNormalizeInterval(uint32_t intervalUs)
{
    if (intervalUs == 0U)
        return 1U;

    return intervalUs;
}

static void timerLedPwmLoadChunk(void)
{
    uint32_t chunkUs = g_tim7RemainingUs;

    if (chunkUs > 65536U)
        chunkUs = 65536U;
    g_tim7RemainingUs -= chunkUs;
    TIM7->ARR = (uint16_t)(chunkUs - 1U);
    TIM7->CNT = 0U;
    TIM7->SR = 0U;
}

void TIM7_IRQHandler(void)
{
    if ((TIM7->SR & TIM_SR_UIF) != 0U)
    {
        TIM7->SR = 0U;
        if (g_tim7RemainingUs != 0U)
        {
            timerLedPwmLoadChunk();
            return;
        }
        LED_PWM_TimerIRQ();
    }
}

void timerLedPwmStart(uint32_t intervalUs)
{
    intervalUs = timerLedPwmNormalizeInterval(intervalUs);

    if (g_tim7Initialized == 0U)
    {
        RCC->APB1ENR |= RCC_APB1ENR_TIM7EN;
        TIM7->PSC = 71U;
        TIM7->CR1 = 0U;
        TIM7->DIER = 0U;
        TIM7->SR = 0U;
        NVIC_SetPriority(TIM7_IRQn, 15U);
        NVIC_EnableIRQ(TIM7_IRQn);
        g_tim7Initialized = 1U;
    }

    TIM7->CR1 &= ~TIM_CR1_CEN;
    g_tim7RemainingUs = intervalUs;
    timerLedPwmLoadChunk();
    TIM7->DIER |= TIM_DIER_UIE;
    TIM7->CR1 |= TIM_CR1_CEN;
}

void timerLedPwmSchedule(uint32_t intervalUs)
{
    g_tim7RemainingUs = timerLedPwmNormalizeInterval(intervalUs);
    timerLedPwmLoadChunk();
    TIM7->CR1 |= TIM_CR1_CEN;
}

void timerLedPwmStop(void)
{
    TIM7->DIER &= ~TIM_DIER_UIE;
    TIM7->CR1 &= ~TIM_CR1_CEN;
    TIM7->SR = 0U;
    g_tim7RemainingUs = 0U;
    NVIC_ClearPendingIRQ(TIM7_IRQn);
}
void timerTicksDelay(uint8_t ticks)
{
    if (ticks == 0U)
        return;

    delay_us((u32)ticks * (u32)TIMER_TICK_US);
}


