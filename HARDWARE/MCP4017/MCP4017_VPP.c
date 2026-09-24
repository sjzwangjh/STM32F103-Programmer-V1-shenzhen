/*
 * MCP4017 数字电位器 VPP 控制模块
 *
 * 移植自 Digital POT.cpp / DigitalPot.h（STM32F103VET6_DFM_Burner_Developer_V5）
 * 适配本项目 iicSoftware.h 中的统一 IIC 接口。
 *
 * 本模块内部建立了自己的 IIC 总线实例 g_iicSoftware_VPP，
 * 使用 Hardware_Config.h 中定义的硬件引脚：
 *   SCL = HW_DVR_VPP_IIC_SCL  (D,5)
 *   SDA = HW_DVR_VPP_IIC_SDA  (D,6)
 *
 * MCP4017 是 7位数字电位器（128抽头），I2C器件地址 0x2F（7位）。
 */

#include "MCP4017_VPP.h"
#include "MCP4017_Calibration.h"
#include "eeprom.h"
#include "Hardware_Config.h"
#include "iicSoftware.h"
#include "delay.h"
#include <stdio.h>

/* ==================== 建立 VPP 专属 IIC 总线实例 ==================== */

// 使用 iicSoftware.h 中的宏生成所有引脚操作函数（static）
IIC_BUS_FUNCS(HW_DVR_VPP_IIC_SCL, HW_DVR_VPP_IIC_SDA, vpp)

// VPP 的 IIC 总线实例（本文件内部使用）
static const IIC_IO_t g_iicSoftware_VPP = {
    .sda_out  = vpp_sda_out,
    .sda_in   = vpp_sda_in,
    .sda_high = vpp_sda_high,
    .sda_low  = vpp_sda_low,
    .read_sda = vpp_read_sda,
    .scl_high = vpp_scl_high,
    .scl_low  = vpp_scl_low,
    .init     = vpp_init,
};

/* ==================== 初始化函数 ==================== */

// 初始化 VPP 的 IIC 总线引脚，在 main 初始化阶段调用一次
void MCP4017_VPP_Init(void)
{
    g_iicSoftware_VPP.init();
}

/* ==================== MCP4017 I2C 读写操作 ==================== */

/*
 * MCP4017 I2C 写电阻值
 *
 * 协议：Start | SendAddr(写) | WaitAck | SendData(value) | WaitAck | Stop
 *
 * value : 电阻数字值 0~127
 * 返回值: 0=成功；0xFF=失败（无应答）
 */
uint8_t MCP4017_VPP_SetResistor(uint8_t value)
{
    uint8_t error;

    if (value > 127U)
    {
        return 0xFFU;
    }

#if DEBUG_HARDWARE_CONFIG
    printf("MCP4017_VPP_SetResistor: value=%d\r\n", value);
#endif
    IIC_Start(&g_iicSoftware_VPP);
    IIC_Send_Byte(&g_iicSoftware_VPP, (MCP4017_ADDR << 1) | 0);   // address + W(0)
    error = IIC_Wait_Ack(&g_iicSoftware_VPP);
    if (error == 0U)
    {
        IIC_Send_Byte(&g_iicSoftware_VPP, value);
        error = IIC_Wait_Ack(&g_iicSoftware_VPP);
    }
    IIC_Stop(&g_iicSoftware_VPP);
    return error;
}

/*
 * MCP4017 I2C 读电阻值
 *
 * 协议：Start | SendAddr(读) | WaitAck | ReadData(NACK) | Stop
 *
 * 返回值: 0~127（有效值）；0xFF（读取失败）
 */
uint8_t MCP4017_VPP_ReadResistor(void)
{
    uint8_t value = 0;
    uint8_t error = 0xFF;

    IIC_Start(&g_iicSoftware_VPP);
    IIC_Send_Byte(&g_iicSoftware_VPP, (MCP4017_ADDR << 1) | 1);   // address + R(1)
    error = IIC_Wait_Ack(&g_iicSoftware_VPP);
    if (error == 0U)
    {
        value = IIC_Read_Byte(&g_iicSoftware_VPP, 0);              // final byte sends NACK
        IIC_Stop(&g_iicSoftware_VPP);
        return value;
    }

    IIC_Stop(&g_iicSoftware_VPP);
    return 0xFFU;
}

/* ==================== 对外接口 ==================== */

/*
 * VPP 电压设置函数
 *
 * 根据目标电压计算 MCP4017 应设置的电阻数字值。
 *
 * 电路原理（Buck-Boost稳压）：
 *   VOUT = ( RUP / RDN + 1 ) x VFB
 *   RDN = RUP / ( VOUT / VFB - 1 )
 *   DigitalValue = RDN / RALL x 127
 * 最低电压 ((22000/5000) + 1) * VFB = 3.24 V
 * voltageInt : 电压值x100，例如 1120 = 11.20V
 * 返回值    : 成功=计算出的电阻数字值(0~127)；失败=0xFF
 */
static uint8_t MCP4017_VPP_LegacySetVoltage(uint16_t voltageInt)
{
    float rx;
    uint8_t rt;
    float vol;
    uint8_t error;

    // VPP 最大限制 15.00V -> 1500
    if (voltageInt > 1500)
    {
        voltageInt = 1500;
    }
    if(voltageInt < 330)  // VPP 最小限制 3.24V -> 324
    {
        voltageInt = 330;
    }

    vol = voltageInt / 100.0f;                     // 转换为浮点电压值（V）
    rx  = VPP_RUP / (vol / VPP_REF - 1);           // 计算 RDN 理论值（ohm）
    rt  = (uint8_t)(rx / VPP_4017_RALL * 127);     // 计算数字电位器比例值（0~127）

    error = MCP4017_VPP_SetResistor(rt);              // 通过IIC写入

    if (error == 0)
    {
        return rt;                                 // 成功，返回设定值
    }
    else
    {
        return 0xFF;                               // 失败
    }
}
/* CRC-16/CCITT over the record, with the stored CRC field excluded. */
static uint16_t MCP4017_VPP_CalibrationCrc(const mcp4017_calibration_t *record)
{
    const uint8_t *bytes = (const uint8_t *)record;
    uint16_t crc = 0xFFFFU;
    uint16_t i;
    uint8_t bit;

    for (i = 0U; i < sizeof(*record); i++)
    {
        if (i == 22U || i == 23U)
            continue;
        crc ^= (uint16_t)bytes[i] << 8;
        for (bit = 0U; bit < 8U; bit++)
            crc = (crc & 0x8000U) ? (uint16_t)((crc << 1) ^ 0x1021U) : (uint16_t)(crc << 1);
    }
    return crc;
}

static uint8_t MCP4017_VPP_LoadCalibration(mcp4017_calibration_t *record)
{
    SPI_EEPROM_Read(MCP4017_CALIBRATION_EEPROM_ADDR,
                    (uint8_t *)record,
                    sizeof(*record));

    if (record->magic != MCP4017_CALIBRATION_MAGIC ||
        record->version != MCP4017_CALIBRATION_VERSION ||
        record->record_size != sizeof(*record) ||
        record->crc16 != MCP4017_VPP_CalibrationCrc(record))
        return 0U;

    if (record->vpp_base_mv < 400U || record->vpp_base_mv > 1200U ||
        record->vpp_gain < 100000UL || record->vpp_gain > 500000UL ||
        record->vpp_offset_milli > 10000U)
        return 0U;

    return 1U;
}

static uint8_t MCP4017_VPP_FittedSetVoltage(uint16_t voltageInt,
                                                  const mcp4017_calibration_t *record)
{
    uint32_t targetMv;
    uint32_t denominator;
    uint32_t tapMilli;
    uint32_t tap;

    if (voltageInt < 330U)
        voltageInt = 330U;
    if (voltageInt > 1500U)
        voltageInt = 1500U;

    targetMv = (uint32_t)voltageInt * 10UL;
    if (targetMv <= record->vpp_base_mv)
        return MCP4017_VPP_SetResistor(127U);

    denominator = targetMv - record->vpp_base_mv;
    tapMilli = ((record->vpp_gain * 1000UL) + (denominator / 2UL)) / denominator;
    if (tapMilli <= record->vpp_offset_milli)
        tap = 0U;
    else
        tap = (tapMilli - record->vpp_offset_milli + 500UL) / 1000UL;
    if (tap > 127U)
        tap = 127U;

    return MCP4017_VPP_SetResistor((uint8_t)tap);
}

uint8_t MCP4017_VPP_SimSetVoltage(uint16_t voltageInt)
{
    mcp4017_calibration_t record;

    if (MCP4017_VPP_LoadCalibration(&record) == 0U)
        return 0xFFU;

    return MCP4017_VPP_FittedSetVoltage(voltageInt, &record);
}

uint8_t MCP4017_VPP_SetVoltage(uint16_t voltageInt)
{
    mcp4017_calibration_t record;

    if (MCP4017_VPP_LoadCalibration(&record) != 0U)
        return MCP4017_VPP_FittedSetVoltage(voltageInt, &record);

    return MCP4017_VPP_LegacySetVoltage(voltageInt);
}
