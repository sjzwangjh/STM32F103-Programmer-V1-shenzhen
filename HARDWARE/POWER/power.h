/*
 * 电源管理头文件 - 电源通道控制接口
 */

#ifndef __POWER_H__
#define __POWER_H__

#include "sys.h"

void power_init(void);
void powerSoftInit(u16 stopV, u16 delaymsPerCycle);

#endif

