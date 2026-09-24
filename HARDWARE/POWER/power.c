#include "power.h"
#include "Hardware_Config.h"
#include "MCP4017_VPP.h"
#include "MCP4017_VDD.h"
#include "dutBus.h"
#include "adc.h"
#include "delay.h"
#include <stdio.h>

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
    u16 startV = 500;
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

/// @brief Vpp电压扫描函数
/// @param start ：数控电阻起始值
/// @param stop ：数控电阻终止值
/// @param step ：步进值
/// @param delaymsPerCycle ：每次循环的延迟时长，单位ms
u8 powerVppScan(u8 start, u8 stop, u8 step, u16 delaymsPerCycle)
{
    u8 tap;
    u8 originalTap;
    u8 result = 0U;
    u16 nextTap;
    u32 nowVppMain;
    u32 nowVppDut;

    if ((step == 0U) || (start > 127U) || (stop > 127U) || (start > stop))
    {
        printf("VPP scan invalid: start=%u stop=%u step=%u\r\n", start, stop, step);
        return 0xFFU;
    }

    if (delaymsPerCycle < 20U)
    {
        delaymsPerCycle = 20U;
    }

    originalTap = MCP4017_VPP_ReadResistor();
    if (originalTap > 127U)
    {
        printf("VPP scan read tap failed\r\n");
        return 0xFFU;
    }

    DUT_VDD_SET_VDD;
    DUT_VPP_SET_VPP;
    delay_ms(100U);

    printf("VPP scan: tap=%u..%u step=%u settle=%ums\r\n",
           start, stop, step, delaymsPerCycle);
    for (tap = start; ; )
    {
        if (MCP4017_VPP_SetResistor(tap) != 0U)
        {
            printf("VPP scan write failed: tap=%u\r\n", tap);
            result = 0xFFU;
            break;
        }

        delay_ms(delaymsPerCycle);
        nowVppMain = Adc_GetChannelRealValue(ADC_CH_VPP_MAIN_FBACK);
        nowVppDut = Adc_GetChannelRealValue(ADC_CH_DUT_UVPP);
        printf("VPP scan: tap=%u, pa2=%lumV, pa7=%lumV\r\n",
               tap, (unsigned long)nowVppMain, (unsigned long)nowVppDut);

        if (tap == stop)
        {
            break;
        }

        nextTap = (u16)tap + (u16)step;
        tap = (nextTap >= (u16)stop) ? stop : (u8)nextTap;
    }

    if (MCP4017_VPP_SetResistor(originalTap) != 0U)
    {
        printf("VPP scan restore tap failed\r\n");
        result = 0xFFU;
    }

    DUT_VPP_SET_FLOAT;
    DUT_VDD_SET_FLOAT;
    return result;
}
u8 powerVddScan(u8 start, u8 stop, u8 step, u16 delaymsPerCycle)
{
    u8 tap;
    u8 originalTap;
    u8 result = 0U;
    u16 nextTap;
    u32 nowVddMain;
    u32 nowVddDut;

    if ((step == 0U) || (start > 127U) || (stop > 127U) || (start > stop))
    {
        printf("VDD scan invalid: start=%u stop=%u step=%u\r\n", start, stop, step);
        return 0xFFU;
    }

    if (delaymsPerCycle < 20U)
    {
        delaymsPerCycle = 20U;
    }

    if (MCP4017_VDD_GetCachedResistor(&originalTap) != 0U)
    {
        printf("VDD scan cached tap unavailable\r\n");
        return 0xFFU;
    }

    DUT_VPP_SET_FLOAT;
    DUT_VDD_SET_VDD;
    delay_ms(100U);

    printf("VDD scan: tap=%u..%u step=%u settle=%ums\r\n",
           start, stop, step, delaymsPerCycle);
    for (tap = start; ; )
    {
        if (MCP4017_VDD_SetResistor(tap) != 0U)
        {
            printf("VDD scan write failed: tap=%u\r\n", tap);
            result = 0xFFU;
            break;
        }

        delay_ms(delaymsPerCycle);
        nowVddMain = Adc_GetChannelRealValue(ADC_CH_VDD_MAIN_FBACK);
        nowVddDut = Adc_GetChannelRealValue(ADC_CH_VDD_FBACK);
        printf("VDD scan: tap=%u, pc4=%lumV, pa1=%lumV\r\n",
               tap, (unsigned long)nowVddMain, (unsigned long)nowVddDut);

        if (tap == stop)
        {
            break;
        }

        nextTap = (u16)tap + (u16)step;
        tap = (nextTap >= (u16)stop) ? stop : (u8)nextTap;
    }

    if (MCP4017_VDD_SetResistor(originalTap) != 0U)
    {
        printf("VDD scan restore tap failed\r\n");
        result = 0xFFU;
    }

    DUT_VDD_SET_FLOAT;
    return result;
}
