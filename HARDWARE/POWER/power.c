#include "power.h"
#include "Hardware_Config.h"
#include "MCP4017_VPP.h"
#include "MCP4017_VDD.h"
#include "delay.h"

/// @brief 电源字节上电函数，由于上电产生大电流，导致USB端口，已淘汰
///         被powerSoftInit函数代替
/// @param  
void power_init(void)
{
    // 电源总输入“VUSB”控制端口初始化
    PORT_RCC_CLK(HW_USB_ON);
    PORT_SET_DIR_PP(HW_USB_ON);
    PORT_OUT(HW_USB_ON) = 0;

    // 初始化IIC总线-------------
    MCP4017_VDD_Init();
	MCP4017_VPP_Init();
    // 设置电源
	MCP4017_VPP_SetVoltage(1120);
	MCP4017_VDD_SetVoltage(500);
    delay_ms(10);
    // 打开DUT电源供给
    PORT_OUT(HW_USB_ON) = 1;
}

/// @brief 慢上电函数
/// @param stopV ：终止VPP电压
/// @param delaymsPerCycle ：每次循环的延迟时长，单位ms
void powerSoftInit(u16 stopV, u16 delaymsPerCycle)
{
    u16 startV = 330;
    // 电源总输入“VUSB”控制端口初始化
    PORT_RCC_CLK(HW_USB_ON);
    PORT_SET_DIR_PP(HW_USB_ON);
    PORT_OUT(HW_USB_ON) = 0;
    
    // 初始化IIC总线-------------
    MCP4017_VDD_Init();
	MCP4017_VPP_Init();

	MCP4017_VDD_SetVoltage(330);
    MCP4017_VPP_SetVoltage(startV);
    delay_ms(delaymsPerCycle);
    startV += 100;
    PORT_OUT(HW_USB_ON) = 1;
    while(startV<stopV)
    {
        MCP4017_VPP_SetVoltage(startV);
        delay_ms(delaymsPerCycle);
        startV += 100;
    }
	MCP4017_VDD_SetVoltage(520);
    delay_ms(50);
}


