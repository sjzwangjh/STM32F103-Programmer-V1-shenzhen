#include "beep.h"
//蜂鸣器驱动代码	   

//蜂鸣器初始化
//引脚定义在 Hardware_Config.h -> HW_BEEP
void BEEP_Init(void)
{
	PORT_RCC_CLK(HW_BEEP);           //使能GPIO时钟	   	  
	PORT_SET_DIR_PP(HW_BEEP);         //推挽输出
	BEEP = 0;                         //关闭蜂鸣器输出
}

