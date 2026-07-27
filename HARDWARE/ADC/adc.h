/*
 * ADC驱动头文件 - 通道定义/分压系数/采样参数配置
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
#define ADC_SCAN_CYCLES         16

#define	ADC_SAMP_COUNT_PER_AVE		10		// 计算平均值的采集次数，去掉最大值和最小值，除以8
#define ADC_SAMP_CHANNEL_COUNT		8		// 采集通道数
#define ADC_SAMP_BUFF_SIZE			(ADC_SAMP_CHANNEL_COUNT * ADC_SAMP_COUNT_PER_AVE)
#define	ADC_COMPUTE_TIME_SPAN_MS	100		// 默认100ms计算一次平均数

#define	ADC_VREF					2.0		// ADC Vref值，硬件用稳压二极管供给Vref实现

// ---ADC测量电压的连接关系图----
//		被测电压
// 		  |
//		 | | RT_UP
//		 | |
//		  |
//		  +---ADC值 = 4096/VREF * 被测电压 * RT_DOWN /（RT_UP + RT_DOWN) 
//		  |		=> 被测电压 = ADC值*（1 + RT_UP/RT_DOWN) *  ADC_VREF/4096
// 		 | | RT_DOWN
// 		 | |
// 		  |
// 		  |
// 		 GND

// VDD_FBACK 分压电阻及计算系数定义，VDD输出电压检测
#define	ADC_VDD_FBACK_RT_DOWN		2.2
#define	ADC_VDD_FBACK_RT_UP			3.9
#define	ADC_VDD_FBACK_COEF			((1 + ADC_VDD_FBACK_RT_UP/ADC_VDD_FBACK_RT_DOWN) * ADC_VREF/4096)
// VPP_FBACK 分压电阻及计算系数定义，输入5V转12V电压检测----需要调节电压点
#define	ADC_VPP_FBACK_RT_DOWN		2.2
#define	ADC_VPP_FBACK_RT_UP			20.0
#define	ADC_VPP_FBACK_COEF			((1 + ADC_VPP_FBACK_RT_UP/ADC_VPP_FBACK_RT_DOWN) * ADC_VREF/4096 )
// USB_GOOD 分压电阻及计算系数定义，输入5V电压检测
#define	ADC_USB_GOOD_RT_DOWN		2.2
#define	ADC_USB_GOOD_RT_UP			3.9
#define	ADC_USB_GOOD_COEF			((1 + ADC_USB_GOOD_RT_UP/ADC_USB_GOOD_RT_DOWN) * ADC_VREF/4096 )
// MCU_ADC4 分压电阻及计算系数定义，VDD电流测量值
#define	ADC_MCU_ADC4_R_LOAD			0.33
#define	ADC_MCU_ADC4_COEF			(ADC_VREF * 1000 / 4096 / ADC_MCU_ADC4_R_LOAD/20/10)	// 电流检测器有20X放大，分辨率0.1mA
// MCU_ADC5 分压电阻及计算系数定义，VPP电流测量值
#define	ADC_MCU_ADC5_R_LOAD			0.33
#define	ADC_MCU_ADC5_COEF			(ADC_VREF * 1000 / 4096 / ADC_MCU_ADC5_R_LOAD/20/10)	// 电流检测器有20X放大，分辨率0.1mA
// POWER_GOOD  分压电阻及计算系数定义
#define	ADC_POWER_GOOD_RT_DOWN		2.2
#define	ADC_POWER_GOOD_RT_UP		3.9
#define	ADC_POWER_GOOD_COEF			((1 + ADC_POWER_GOOD_RT_UP/ADC_POWER_GOOD_RT_DOWN) * ADC_VREF/4096)
// MCU_PA7  分压电阻及计算系数定义,VPP输出电压检测
#define	ADC_MCU_PA7_RT_DOWN			2.2
#define	ADC_MCU_PA7_RT_UP			20.0
#define	ADC_MCU_PA7_COEF			((1 + ADC_MCU_PA7_RT_UP/ADC_MCU_PA7_RT_DOWN)*ADC_VREF/4096)
// MCU_PC4  分压电阻及计算系数定义，12V转DUT5V电压检测，PCB V2无连接-----需要调节电压点
#define	ADC_MCU_PC4_RT_DOWN			2.2
#define	ADC_MCU_PC4_RT_UP			3.9
#define	ADC_MCU_PC4_COEF			((1 + ADC_MCU_PC4_RT_UP/ADC_MCU_PC4_RT_DOWN) * ADC_VREF/4096)

extern u16 adcScanRecodeBuff[ADC_SCAN_BUF_SIZE];

void Adc_Init(void);
u16  Adc_GetChannel(u8 ch);

#endif

