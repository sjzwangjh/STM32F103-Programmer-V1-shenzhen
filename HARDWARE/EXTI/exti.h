#ifndef __EXTI_H
#define __EXIT_H	 
#include "sys.h"

/*
 * PE4 在当前硬件中同时被分配为 DUT_PIN7_CTRL。
 * 默认关闭 EXTI4 对 PE4 的占用，避免与 DUT 总线控制冲突。
 * 若后续确实需要恢复 PE4 外部中断，再将该宏改为 1。
 */
#ifndef EXTI_ENABLE_EXTI
#define EXTI_ENABLE_EXTI     0
#endif

//外部中断 驱动代码	   
void EXTIX_Init(void);//外部中断初始化		 					    
#endif

