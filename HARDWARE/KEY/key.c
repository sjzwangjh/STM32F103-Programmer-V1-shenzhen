#include "key.h"
#include "delay.h"
//按键驱动代码	   
								    
//按键初始化函数
void KEY_Init(void)
{
	// 四个按键引脚在 Hardware_Config.h 中定义：HW_BTN_UP/DOWN/ENTER/BACK
	// 均配置为上拉输入（按下为低电平）
	PORT_RCC_CLK(HW_BTN_ENTER);
	PORT_RCC_CLK(HW_BTN_DOWN);
	PORT_RCC_CLK(HW_BTN_BACK);
	PORT_RCC_CLK(HW_BTN_UP);

	PORT_SET_DIR_IN_PU(HW_BTN_ENTER);
	PORT_SET_DIR_IN_PU(HW_BTN_DOWN);
	PORT_SET_DIR_IN_PU(HW_BTN_BACK);
	PORT_SET_DIR_IN_PU(HW_BTN_UP);
} 
//按键处理函数
//返回按键值
//mode:0,不支持连续按;1,支持连续按;
//注意此函数有响应优先级,KEY0>KEY1>KEY2>KEY3!!
u8 KEY_Scan(u8 mode)
{	 
	static u8 key_up=1;//按键按松开标志

	if(mode)key_up=1;  //支持连按
	if(key_up && (KEY_ENTER==0||KEY_DOWN==0||KEY_BACK==0||KEY_UP==0))
	{
		delay_ms(10);//去抖动 
		key_up=0;
		if(KEY_ENTER==0)return KEY_VALUE_ENTER;
		else if(KEY_DOWN==0)return KEY_VALUE_DOWN;
		else if(KEY_BACK==0)return KEY_VALUE_BACK;
		else if(KEY_UP==0)return KEY_VALUE_UP;
	}else if(KEY_ENTER==1&&KEY_DOWN==1&&KEY_BACK==1&&KEY_UP==1)key_up=1; 	    
 	return 0;// 无按键按下
}

