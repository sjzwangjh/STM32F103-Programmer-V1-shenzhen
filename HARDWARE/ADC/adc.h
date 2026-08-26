/*
 * ADC驱动头文件 - 通道定义/分压系数/采样参数配置,硬件连接
 * 通道1：PA1 -> DUT的VDD -> VDD_FBACK，采集电压；
 * 通道2：PA2 -> 主电源VPP -> VPP_FBACK， 采集电压；
 * 通道3：PA3 -> VUSB开关后 -> USB_GOOF，采集电压；
 * 通道4：PA4 -> DUT-VDD电流 -> MCU_ADC4，VDD_FBACK采集电流；
 * 通道5：PA5 -> DUT-VPP电流 -> MCU_ADC5，采集电流；
 * 通道6：PA6 -> MCU电源3.3V -> POWER_GOOD，采集电压；
 * 通道7：PA7 -> DUT-VPP电压 ->MCU_PA7，采集电压；
 * 通道8：PC4 -> 主电源VDD -> MAIN_VDD, 采集电压；
 */

#ifndef __ADC_H
#define __ADC_H

#include "sys.h"

#define ADC_CH_VDD_FBACK        0
#define ADC_CH_VPP_MAIN_FBACK   1
#define ADC_CH_USB_GOOD         2
#define ADC_CH_DUT_IVDD         3
#define ADC_CH_DUT_IVPP         4
#define ADC_CH_3V3_POWER_GOOD   5
#define ADC_CH_DUT_UVPP         6
#define ADC_CH_VDD_MAIN_FBACK   7

#define ADC_SCAN_CHANNELS       8
#define ADC_SCAN_BUF_SIZE       128
#define ADC_FILTER_WINDOW_SIZE  10
#define ADC_FILTER_AVG_COUNT    8

#define ADC_VREF_MV             2000U
#define ADC_ADC_MAX_COUNTS      4096U
#define ADC_CURRENT_GAIN        20U
#define ADC_CURRENT_R_MOHM      330U

#define	ADC_VREF					2.0		// ADC Vref值，由硬件分压网络能够确保Vref稳定

// ---ADC输入分压采样关系图----
//		被测电压
// 		  |
//		 | | RT_UP
//		 | |
// 		  |
// 		  +---ADC值 = 4096/VREF * 被测电压 * RT_DOWN /(RT_UP + RT_DOWN) 
// 		  |		=> 被测电压 = ADC值*(1 + RT_UP/RT_DOWN) *  ADC_VREF/4096
// 		 | | RT_DOWN
// 		 | |
// 		  |
// 		  |
// 		 GND

// VDD_FBACK 分压电阻及换算关系定义，VDD反馈电压检测
#define	ADC_VDD_FBACK_RT_DOWN		2.2
#define	ADC_VDD_FBACK_RT_UP			3.9
// VPP_FBACK 分压电阻及换算关系定义，主板5V转12V升压输出----需要监测电压值
#define	ADC_VPP_FBACK_RT_DOWN		2.2
#define	ADC_VPP_FBACK_RT_UP			20.0
// USB_GOOD 分压电阻及换算关系定义，主板5V电压检测
#define	ADC_USB_GOOD_RT_DOWN		2.2
#define	ADC_USB_GOOD_RT_UP			3.9
// MCU_ADC4 采样电阻及换算关系定义，VDD输出电流值
#define	ADC_MCU_ADC4_R_LOAD			0.33
// MCU_ADC5 采样电阻及换算关系定义，VPP输出电流值
#define	ADC_MCU_ADC5_R_LOAD			0.33
// POWER_GOOD  分压电阻及换算关系定义
#define	ADC_POWER_GOOD_RT_DOWN		2.2
#define	ADC_POWER_GOOD_RT_UP		3.9
// MCU_PA7  分压电阻及换算关系定义,VPP输出电压检测
#define	ADC_MCU_PA7_RT_DOWN			2.2
#define	ADC_MCU_PA7_RT_UP			20.0
// MCU_PC4  分压电阻及换算关系定义，12V转DUT5V分压检测，PCB V2新增电路-----需要监测电压值
#define	ADC_MCU_PC4_RT_DOWN			2.2
#define	ADC_MCU_PC4_RT_UP			3.9

extern u16 adcScanRecodeBuff[ADC_SCAN_BUF_SIZE];

typedef enum
{
    ADC_REAL_UNIT_NONE = 0,
    ADC_REAL_UNIT_MV   = 1,
    ADC_REAL_UNIT_MA   = 2
} adc_real_unit_t;

void Adc_Init(void);
u16  Adc_GetChannel(u8 ch);
u16  Adc_GetChannelAverage(u8 ch);
u32  Adc_GetChannelRealValue(u8 ch);
adc_real_unit_t Adc_GetChannelRealUnit(u8 ch);
void Adc_GetAllChannelAverage(u16 *buff, u8 maxCount);
void Adc_GetAllChannelRealValue(u32 *buff, u8 maxCount);

#endif
