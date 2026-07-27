/*
 * ISP在线编程头文件 - 定义ISP编程接口函数原型
 */

/*
 * Name: isp.h
 * Project: STM32F103VET6_Programmer (AVR ISP Programmer)
 * Author: Ported from AVR-Doper by Christian Starkjohann <cs@obdev.at>
 *
 * General Description:
 * This module implements the STK500v2 primitives for In System Programming.
 * Functions accept parameters directly from the input data stream and prepare
 * results for the output data stream, where appropriate.
 *
 * Porting notes:
 *   - Replaced AVR uchar/uint with uint8_t/uint16_t
 *   - Parameter structs defined in Stk500Protocol.h (stkEnterProgIsp_t etc.)
 *   - Hardware pin mapping via HARDWARE/Hardware_Config.h (HWPIN_ISP_*)
 *   - Timing via HARDWARE/TIMER/timer.h (timerMsDelay, timerTicksDelay)
 */

#ifndef __ISP_H_INCLUDED__
#define __ISP_H_INCLUDED__

#include "sys.h"
#include "Hardware_Config.h"
#include <stdint.h>

/* STK500v2 ISP parameter structs - defined in Stk500Protocol.h */
#include "Stk500Protocol.h"

// 基于Hardware_Config.h中的“扩展宏定义”-----
// 扩展宏定义--AVR DOPER编程器ISP接口--------
#define HWPIN_ISP_MOSI          HW_DUT_PIN7_DAT     // SPI1_MOSI
#define HWPIN_ISP_MISO          HW_DUT_PIN4_DAT     // SPI1_MISO
#define HWPIN_ISP_SCK           HW_DUT_PIN5_DAT     // SPI1_SCK
#define HWPIN_ISP_CLK           HW_DUT_PIN8_DAT     // TIM9_CH1
#define HWPIN_ISP_RESET         HW_DUT_PIN6_DAT    // 控制RESET的接地

#define ISP_POWER_ON            PORT_OUT(HW_DUT_VDD_VH_ON) = 1
#define ISP_POWER_OFF           PORT_OUT(HW_DUT_VDD_VH_ON) = 0

uint8_t   ispEnterProgmode(stkEnterProgIsp_t *param);
void      ispLeaveProgmode(stkLeaveProgIsp_t *param);
uint8_t   ispChipErase(stkChipEraseIsp_t *param);
uint8_t   ispProgramMemory(stkProgramFlashIsp_t *param, uint8_t isEeprom);
uint8_t   ispVerifyMemory(stkProgramFlashIsp_t *param, uint8_t isEeprom);
uint16_t  ispReadMemory(stkReadFlashIsp_t *param, stkReadFlashIspResult_t *result, uint8_t isEeprom);
uint8_t   ispProgramFuse(stkProgramFuseIsp_t *param);
uint8_t   ispVerifyFuse(stkProgramFuseIsp_t *param);
uint8_t   ispReadFuse(stkReadFuseIsp_t *param);
uint16_t  ispMulti(stkMultiIsp_t *param, stkMultiIspResult_t *result);

#endif  /* __ISP_H_INCLUDED__ */


