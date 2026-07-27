/*
 * 外部中断驱动实现
 */

#include "exti.h"
#include "led.h"
#include "beep.h"
#include "key.h"
#include "delay.h"
#include "usart.h"

void EXTI2_IRQHandler(void)
{
	delay_ms(10);
	LED_HALT = !LED_HALT;
	EXTI->PR = 1 << 2;
}

void EXTI3_IRQHandler(void)
{
	delay_ms(10);
	LED_HALT = !LED_HALT;
	EXTI->PR = 1 << 3;
}

void EXTI4_IRQHandler(void)
{
	delay_ms(10);
	LED_HALT = !LED_HALT;
	LED_ACTIVE = !LED_ACTIVE;
	EXTI->PR = 1 << 4;
}

/*
 * 默认只初始化 PA0、PE2、PE3。
 * PE4 在当前硬件中与 DUT_PIN7_CTRL 共用，默认不启用 EXTI4。
 */
void EXTIX_Init(void)
{
#if EXTI_ENABLE_EXTI
	Ex_NVIC_Config(GPIO_A, 0, RTIR);
	Ex_NVIC_Config(GPIO_E, 2, FTIR);
	Ex_NVIC_Config(GPIO_E, 3, FTIR);
	Ex_NVIC_Config(GPIO_E, 4, FTIR);

	MY_NVIC_Init(2, 3, EXTI0_IRQn, 2);
	MY_NVIC_Init(2, 2, EXTI2_IRQn, 2);
	MY_NVIC_Init(2, 1, EXTI3_IRQn, 2);
	MY_NVIC_Init(2, 0, EXTI4_IRQn, 2);
#endif
}
