/*
 * Name: hvproc.h
 * Project: STM32F103VET6_Programmer
 * Author: Ported from AVR-Doper by Christian Starkjohann <cs@obdev.at>
 *
 * General Description:
 * This module implements the STK500v2 primitives for High Voltage serial
 * programming. On this STM32 board, the DUT bus exposes the HVSER signals,
 * so PP entry points are kept for protocol compatibility and reuse the same
 * serial execution backend.
 */

#ifndef __HVPROC_H_INCLUDED__
#define __HVPROC_H_INCLUDED__

#include "Stk500Protocol.h"
#include "Hardware_Config.h"
#include "dutBus.h"

// 扩展宏定义-- AVR DOPER HVSP编程序
#define HWPIN_HVSP_SUPPLY       HW_DUT_VDD_VH_ON        // 控制5V电源打开
#define HWPIN_HVSP_HVRESET      HW_DUT_VPP_VH_ON        // 控制12V打开
#define HWPIN_HVSP_RESET        HW_DUT_VPP_VL_ON        // 控制12V接地
#define HWPIN_HVSP_SCI          HW_DUT_PIN5_DAT
#define HWPIN_HVSP_SII          HW_DUT_PIN7_DAT
#define HWPIN_HVSP_SDI          HW_DUT_PIN6_DAT
#define HWPIN_HVSP_SDO          HW_DUT_PIN4_DAT

/* ---- Power (5V VDD) ---- */
#define HVSP_VDD_ON()   do { PORT_RCC_CLK(HW_DUT_VDD_VH_ON); PORT_RCC_CLK(HW_DUT_VDD_VL_ON); DUT_VDD_SET_VDD; } while(0)
#define HVSP_VDD_OFF()  DUT_VDD_SET_FLOAT
#define HVSP_VDD_GND()  DUT_VDD_SET_GND

/* ---- HV/RESET line (VPP, single wire; 12V entry) ---- */
#define HVSP_HVON()     do { PORT_RCC_CLK(HW_DUT_VPP_VH_ON); PORT_RCC_CLK(HW_DUT_VPP_VL_ON); DUT_VPP_SET_VPP; } while(0)
#define HVSP_HVOFF()    DUT_VPP_SET_FLOAT
#define HVSP_HVGND()    DUT_VPP_SET_GND

/* ---- Serial lines ---- */
#define HVSP_SCI_H()    PORT_OUT(HWPIN_HVSP_SCI) = 1
#define HVSP_SCI_L()    PORT_OUT(HWPIN_HVSP_SCI) = 0
#define HVSP_SCI_OUT(v) PORT_OUT(HWPIN_HVSP_SCI) = ((v) ? 1 : 0)
#define HVSP_SII_OUT(v) PORT_OUT(HWPIN_HVSP_SII) = ((v) ? 1 : 0)
#define HVSP_SDI_OUT(v) PORT_OUT(HWPIN_HVSP_SDI) = ((v) ? 1 : 0)
#define HVSP_SDO_OUT(v) PORT_OUT(HWPIN_HVSP_SDO) = ((v) ? 1 : 0)
#define HVSP_SDO_IN()   (PORT_IN(HWPIN_HVSP_SDO) ? 1 : 0)

#define HVSP_SCI_OUTPUT()       DUT_PIN5_SET_OUTPUT
#define HVSP_SCI_INPUT()        DUT_PIN5_SET_INPUT
#define HVSP_SII_OUTPUT()       DUT_PIN7_SET_OUTPUT
#define HVSP_SII_INPUT()        DUT_PIN7_SET_INPUT
#define HVSP_SDI_OUTPUT()       DUT_PIN6_SET_OUTPUT
#define HVSP_SDI_INPUT()        DUT_PIN6_SET_INPUT
#define HVSP_SDO_OUTPUT()       DUT_PIN4_SET_OUTPUT
#define HVSP_SDO_INPUT()        DUT_PIN4_SET_INPUT

#define HVSP_BUS_IDLE()         do { HVSP_SCI_L(); HVSP_SII_OUT(0); HVSP_SDI_OUT(0); } while(0)

/* Microsecond delay for HVSP serial timing */
void hvspDelayUs(uint32_t us);
#define HVSP_DELAY_US(n)        hvspDelayUs((uint32_t)(n))


void     hvspEnterProgmode(stkEnterProgHvsp_t *param);
void     hvspLeaveProgmode(stkLeaveProgHvsp_t *param);
uint8_t  hvspChipErase(stkChipEraseHvsp_t *param);
uint8_t  hvspProgramMemory(stkProgramFlashHvsp_t *param, uint8_t isEeprom);
uint8_t  hvspVerifyMemory(stkProgramFlashHvsp_t *param, uint8_t isEeprom);
uint16_t hvspReadMemory(stkReadFlashHvsp_t *param, stkReadFlashHvspResult_t *result, uint8_t isEeprom);
uint8_t  hvspProgramFuse(stkProgramFuseHvsp_t *param);
uint8_t  hvspVerifyFuse(stkProgramFuseHvsp_t *param);
uint8_t  hvspProgramLock(stkProgramFuseHvsp_t *param);
uint8_t  hvspReadFuse(stkReadFuseHvsp_t *param);
uint8_t  hvspReadLock(void);
uint8_t  hvspReadSignature(stkReadFuseHvsp_t *param);
uint8_t  hvspReadOsccal(void);

void     ppEnterProgmode(stkEnterProgPp_t *param);
void     ppLeaveProgmode(stkLeaveProgPp_t *param);
uint8_t  ppChipErase(stkChipErasePp_t *param);
#define ppProgramMemory(param, isEeprom)        hvspProgramMemory((stkProgramFlashHvsp_t *)(param), (isEeprom))
#define ppReadMemory(param, result, isEeprom)   hvspReadMemory((stkReadFlashHvsp_t *)(param), (stkReadFlashHvspResult_t *)(result), (isEeprom))
uint8_t  ppProgramFuse(stkProgramFusePp_t *param);
uint8_t  ppVerifyMemory(stkProgramFlashHvsp_t *param, uint8_t isEeprom);
uint8_t  ppVerifyFuse(stkProgramFusePp_t *param);
uint8_t  ppProgramLock(stkProgramFusePp_t *param);
#define ppReadFuse(param)                       hvspReadFuse((stkReadFuseHvsp_t *)(param))
#define ppReadLock()                            hvspReadLock()
#define ppReadSignature(param)                  hvspReadSignature((stkReadFuseHvsp_t *)(param))
#define ppReadOsccal()                          hvspReadOsccal()

#endif


