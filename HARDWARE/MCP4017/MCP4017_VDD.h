#ifndef     __MCP4017_VDD_H__
#define     __MCP4017_VDD_H__

#include "sys.h"

/*
 * MCP4017 数字电位器 VDD 控制模块
 *
 * MCP4017 是 7位 数字电位器（128抽头），I2C接口。
 * 本模块内部建立了自己的 IIC 总线实例：g_iicSoftware_VDD，
 * 使用 Config.h 中定义的硬件引脚：
 *   SCL = HW_DVR_VDD_IIC_SCL
 *   SDA = HW_DVR_VDD_IIC_SDA
 *
 * I2C器件地址：0x2F（7位）
 *
 * 使用方法：
 *   1. 在 main 初始化时调用 MCP4017_VDD_Init() 初始化 IIC 总线引脚
 *   2. 调用 MCP4017_VDD_SetVoltage() 设置电压
 */

// MCP4017 I2C 器件地址（7位）
#define MCP4017_ADDR            0x2F

// VDD电压调节参数（来自 DigitalPot.h）
#define VDD_RUP                 12000.0f    // VDD上分压电阻值（Ω）
#define VDD_4017_RALL           5000.0f     // MCP4017全阻值（Ω）
#define VDD_REF                 0.766f      // FB基准电压值（V）

// 初始化 VDD 的 IIC 总线引脚（在 main 初始化时调用一次）
void MCP4017_VDD_Init(void);

// VDD 电压设置函数
// voltageInt：电压值x100，例如 520 = 5.20V
// 返回值：成功=计算出的电阻数字值(0~127)；失败=0xFF
uint8_t MCP4017_VDD_SetVoltage(uint16_t voltageInt);

// VDD 读取当前电阻值
// 返回值：0~127（有效值）；0xFF（读取失败）
uint8_t MCP4017_VDD_ReadResistor(void);

#endif





