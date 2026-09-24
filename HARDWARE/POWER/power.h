/*
 * 电源管理头文件 - 电源通道控制接口
 */

#ifndef __POWER_H__
#define __POWER_H__

#include "sys.h"

void power_init(void);
void powerSoftInit(u16 stopV, u16 delaymsPerCycle);
u8 powerVppScan(u8 start, u8 stop, u8 step, u16 delaymsPerCycle);
u8 powerVddScan(u8 start, u8 stop, u8 step, u16 delaymsPerCycle);

#endif

