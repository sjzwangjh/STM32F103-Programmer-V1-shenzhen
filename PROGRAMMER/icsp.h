/*
 * ICSP 编程驱动头文件 - PIC10/12/16 系列串行编程接口定义
 */

#ifndef     __ICSP_H__
#define     __ICSP_H__

#include "sys.h"
#include "Hardware_Config.h"
#include "dutBus.h"
#include "picDeviceConst.h"
/* Protocol-facing types (stkProgramFlashIcsp_t etc.) and stkAddress. */
#include "Stk500Protocol.h"

// 基于Hardware_Config.h中的"扩展宏定义"-----
// 扩展宏定义--PIC 编程器ICSP接口----CLK/DAT/LVP----
#define HWPIN_ICSP_DAT          HW_DUT_PIN4_DAT     // ICSPDAT
#define HWPIN_ICSP_CLK          HW_DUT_PIN5_DAT     // ICSPCLK
#define HWPIN_ICSP_LVP          HW_DUT_PIN6_DAT     // PGM (LVP控制)

// ICSP电源控制宏定义
#define ICSP_POWER_ON           DUT_VDD_SET_VDD
#define ICSP_POWER_OFF          DUT_VDD_SET_FLOAT
#define ICSP_POWER_GND          DUT_VDD_SET_GND

// ICSP高压控制宏定义
#define ICSP_HVP_ON             DUT_VPP_SET_VPP
#define ICSP_HVP_OFF            DUT_VPP_SET_FLOAT
#define ICSP_HVP_GND            DUT_VPP_SET_GND

/* ================================================================= */
/* A层: 电源与引脚控制 —— 全部通过宏内联，消除函数调用开销               */
/* ================================================================= */

/* VDD 操作 */
#define ICSP_VDD_ON()           do{ PORT_RCC_CLK(HW_DUT_VDD_VH_ON); PORT_RCC_CLK(HW_DUT_VDD_VL_ON); ICSP_POWER_ON; }while(0)
#define ICSP_VDD_OFF()          ICSP_POWER_OFF

/* VPP 操作 */
#define ICSP_VPP_ON()           do{ PORT_RCC_CLK(HW_DUT_VPP_VH_ON); PORT_RCC_CLK(HW_DUT_VPP_VL_ON); ICSP_HVP_ON; }while(0)
#define ICSP_VPP_OFF()          ICSP_HVP_OFF
#define ICSP_VPP_GND()          ICSP_HVP_GND

/* CLK 操作 */
#define ICSP_CLK_H()            PORT_OUT(HWPIN_ICSP_CLK) = 1
#define ICSP_CLK_L()            PORT_OUT(HWPIN_ICSP_CLK) = 0
#define ICSP_CLK_OUT()          DUT_PIN5_SET_OUTPUT
#define ICSP_CLK_IN()           DUT_PIN5_SET_INPUT

/* DAT 操作 */
#define ICSP_DAT_H()            PORT_OUT(HWPIN_ICSP_DAT) = 1
#define ICSP_DAT_L()            PORT_OUT(HWPIN_ICSP_DAT) = 0
#define ICSP_DAT_R()            (PORT_IN(HWPIN_ICSP_DAT))
#define ICSP_DAT_OUT()          DUT_PIN4_SET_OUTPUT
#define ICSP_DAT_IN()           DUT_PIN4_SET_INPUT

/* LVP/PGM 操作 */
#define ICSP_LVP_H()            PORT_OUT(HWPIN_ICSP_LVP) = 1
#define ICSP_LVP_L()            PORT_OUT(HWPIN_ICSP_LVP) = 0
#define ICSP_LVP_OUT()          DUT_PIN6_SET_OUTPUT
#define ICSP_LVP_IN()           DUT_PIN6_SET_INPUT

/* 微秒延时 */
void icspDelayUs(uint32_t us);
#define ICSP_DELAY_US(n)        icspDelayUs((uint32_t)(n))
#define ICSP_CLK_DELAY          ICSP_DELAY_US(1)

/* ================================================================= */
/* B层: ICSP 时序位操作层                                              */
/* ================================================================= */

/* 发送一个时钟脉冲 (高→低) */
#define ICSP_CLOCK_PULSE()      do{ ICSP_CLK_H(); ICSP_CLK_DELAY; ICSP_CLK_L(); ICSP_CLK_DELAY; }while(0)

/* 发送命令 (6-bit, LSB first) */
void icspLoadCmd(uint8_t cmd);

/* 发送数据字 */
void icspLoadData(uint16_t data, uint8_t width);

/* 接收数据字 */
uint16_t icspReadData(uint8_t width);

/* ================================================================= */
/* C层: 编程模式进入/退出                                               */
/* ================================================================= */

#define ICSP_OK      0
#define ICSP_ERR     1

uint8_t icspEnterHV(const pic_prog_params_t *dev);   /* HV 高压进入 */
uint8_t icspEnterLV(const pic_prog_params_t *dev);   /* LVP 低压进入 */
void    icspExit(void);                                /* 退出编程模式 */

/* ================================================================= */
/* D层: 器件操作原语                                                   */
/* ================================================================= */

uint8_t  icspReadSignature(uint16_t *sig);
uint32_t icspReadDevID(void);
uint8_t  icspBulkErase(void);
uint8_t  icspProgWord(uint16_t data);
uint16_t icspReadWord(void);
uint8_t  icspProgRow(const uint16_t *buf, uint32_t cnt);
uint8_t  icspSetProgramAddress(uint32_t addr);
uint8_t  icspSetEeAddress(uint32_t addr);
uint8_t  icspProgEE(uint8_t val);
uint8_t  icspReadEE(uint8_t *val);
uint8_t  icspProgCfg(uint8_t idx, uint16_t val);
uint16_t icspReadCfg(uint8_t idx);
uint8_t  icspProgUID(uint8_t idx, uint16_t val);
uint16_t icspReadUID(uint8_t idx);
uint16_t icspReadOSCCAL(uint8_t idx);
uint8_t  icspWriteOSCCAL(uint8_t idx, uint16_t val);
uint8_t  icspProgUserIdWords(uint32_t baseAddr, const uint8_t *data, uint16_t count);
uint8_t  icspReadUserIdWords(uint32_t baseAddr, uint8_t *out, uint16_t count);
/* ================================================================= */
/* D2: STK500v2 protocol adapter layer (same layout as isp.c/hvproc.c) */
/* ================================================================= */

uint8_t  icspProgramMemory(stkProgramFlashIcsp_t *param, uint8_t isEeprom);
uint16_t icspReadMemory(stkReadFlashIcsp_t *param,
                        stkReadFlashIcspResult_t *result,
                        uint8_t isEeprom);

/* ================================================================= */
/* E层: 校验与安全                                                     */
/* ================================================================= */


/* ================================================================= */
/* F层: Family 驱动入口                                                */
/* ================================================================= */

void    pic8Init(const pic_prog_params_t *dev);
uint8_t pic8EnterProgmode(uint8_t preferLvp);
void    pic8LeaveProgmode(void);

#endif /* __ICSP_H__ */

