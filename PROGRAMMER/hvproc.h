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

// 扩展宏定义-- AVR DOPER HVSP编程序
#define HWPIN_HVSP_SUPPLY       HW_DUT_VDD_VH_ON        // 控制5V电源打开
#define HWPIN_HVSP_HVRESET      HW_DUT_VPP_VH_ON        // 控制12V打开
#define HWPIN_HVSP_RESET        HW_DUT_VPP_VL_ON        // 控制12V接地
#define HWPIN_HVSP_SCI          HW_DUT_PIN5_DAT
#define HWPIN_HVSP_SII          HW_DUT_PIN7_DAT
#define HWPIN_HVSP_SDI          HW_DUT_PIN6_DAT
#define HWPIN_HVSP_SDO          HW_DUT_PIN4_DAT

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


