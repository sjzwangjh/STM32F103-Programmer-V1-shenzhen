#ifndef __KEY_H
#define __KEY_H	 
#include "sys.h"
#include "Hardware_Config.h"
//按键驱动代码	   


#if !defined(HW_BTN_UP) || !defined(HW_BTN_DOWN) || !defined(HW_BTN_ENTER) || !defined(HW_BTN_BACK)
    #error "Config.h必须定义四个按键引脚(HW_BTN_UP HW_BTN_DOWN HW_BTN_ENTER HW_BTN_BACK)!"
#endif

// 按键引脚读取宏（通过 Config.h 定义，低电平有效）
#define KEY_ENTER   PORT_IN(HW_BTN_ENTER)   // 确认键：按下为0
#define KEY_DOWN    PORT_IN(HW_BTN_DOWN)    // 下键：按下为0
#define KEY_BACK    PORT_IN(HW_BTN_BACK)    // 返回键：按下为0
#define KEY_UP      PORT_IN(HW_BTN_UP)      // 上键：按下为0

// 按键返回值编号（与按键物理引脚顺序无关）
#define KEY_VALUE_UP       4
#define KEY_VALUE_DOWN     2
#define KEY_VALUE_ENTER    1
#define KEY_VALUE_BACK     3

void KEY_Init(void);//IO初始化
u8 KEY_Scan(u8);  	//按键扫描函数


#endif

