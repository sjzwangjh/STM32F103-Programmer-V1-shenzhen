/*
 * Debug.h - 串口调试接口定义
 */

#ifndef __DEBUG_H__
#define __DEBUG_H__

#include "sys.h"

/* 命令索引，顺序需与 Debug.c 中的 debugCmdList 保持一致。 */
typedef enum debugCmdEnum
{
    DEBUG_CMD_HID = 0,
    DEBUG_CMD_LED,
    DEBUG_CMD_BEEP,
    DEBUG_CMD_VBUS,
    DEBUG_CMD_VPPSET,
    DEBUG_CMD_VDDSET,
    DEBUG_CMD_DUTVPP,
    DEBUG_CMD_DUTVDD,
    DEBUG_CMD_DBGPIN,
    DEBUG_CMD_DBGDELAY,
    DEBUG_CMD_PIN,
    DEBUG_CMD_HANDLER,
    DEBUG_CMD_ADC,
    DEBUG_CMD_WEEPROM,
    DEBUG_CMD_REEPROM,
    DEBUG_CMD_WFLASH,
    DEBUG_CMD_RFLASH,
    DEBUG_CMD_WSD,
    DEBUG_CMD_RSD,
    DEBUG_CMD_LSD,
    DEBUG_CMD_RDAVRPARAM,
    DEBUG_CMD_RDPICPARAM,
    DEBUG_CMD_HELP,
    DEBUG_CMD_TEST,
    
    DEBUG_CMD_MAX
} debugCmdEnum_t;

void usartCmdTask(void);

#endif
