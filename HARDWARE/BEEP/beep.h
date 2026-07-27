/*
 * 蜂鸣器驱动头文件 - 定义蜂鸣器控制引脚和初始化接口
 */

#ifndef __BEEP_H
#define __BEEP_H	 
#include "sys.h"
#include "Hardware_Config.h"

// 如果没有定义HW_BEEP，提示编译错误
#ifndef HW_BEEP
    #error "Config.h中必须定义BEEP引脚宏定义 HW_BEEP
#endif

//蜂鸣器驱动代码	   
//蜂鸣器端口定义：引脚定义在 Config.h -> HW_BEEP
#define BEEP PORT_OUT(HW_BEEP)	 // BEEP,蜂鸣器接口		   

void BEEP_Init(void);	//初始化


#endif

