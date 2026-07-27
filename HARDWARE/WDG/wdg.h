#ifndef __WDG_H
#define __WDG_H
#include "sys.h"
//看门狗 驱动代码	   

void IWDG_Init(u8 prer,u16 rlr);
void IWDG_Feed(void);
void WWDG_Init(u8 tr,u8 wr,u8 fprer);
void WWDG_Set_Counter(u8 cnt);
#endif


