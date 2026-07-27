/*
 * 定时器驱动头文件 - 定时器配置和参数定义
 */

#ifndef __TIMER_H
#define __TIMER_H
#include "sys.h"

/*
 * Timer module - STM32 port of AVR-Doper timer.c (C. Starkjohann)
 *
 * Based on TIM6 (16-bit basic timer, no I/O channels).
 * TIM6 clock: APB1 * 2 = 72MHz at standard PCLK1=36MHz config.
 * PSC = 72-1 -> CK_CNT = 1MHz (1us per count)
 * ARR = 1000-1 -> overflow at 1ms intervals
 *
 * Public functions:
 *   timerSetupTimeout()          - start short (~1ms resolution) timeout
 *   timerSetupLongTimeout()      - start long (~100ms resolution) timeout
 *   timerTimeoutOccurred()       - true when short timeout elapsed
 *   timerLongTimeoutOccurred()   - true when long timeout elapsed
 *   timerMsDelay()              - blocking delay (milliseconds)
 */

/* 
 * TIMER_TICK_US: base timer tick period in microseconds.
 * AVR-Doper: TCNT0 @ F_CPU/64, one tick = 64 / F_CPU * 1e6 = 5.333us @12MHz.
 * STM32: TIM6 @1MHz (1us/tick), one "tick" is defined as ~5us to match AVR timing.
 */
#define TIMER_TICK_US       5   /* ~5.333us equivalent of AVR TCNT0 tick */

extern volatile uint8_t  timerTimeoutCnt;
extern volatile uint8_t  timerLongTimeoutCnt;

void timerInit(void);
void timerMsDelay(uint8_t ms);
void timerSetupTimeout(uint8_t msDuration);
void timerTicksDelay(uint8_t ticks);    /* bit-bang ISP timing delay */

static inline uint8_t timerTimeoutOccurred(void)
{
    return timerTimeoutCnt == 0;
}

static inline void timerSetupLongTimeout(uint8_t ms100Duration)
{
    timerLongTimeoutCnt = ms100Duration;
}

static inline uint8_t timerLongTimeoutOccurred(void)
{
    return timerLongTimeoutCnt == 0;
}

#endif

