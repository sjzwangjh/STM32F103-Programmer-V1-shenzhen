#ifndef __OFFLINE_PGMER_H__
#define __OFFLINE_PGMER_H__

#include "offLineRecorder.h"
#include <stdint.h>

/*
 * Load the active package selected in EEPROM and validate its basic header.
 * Return 0 on success, 1 on initialization failure.
 */
uint8_t offlinePgmer_init(void);

/*
 * 兼容旧文件名: 离线记录器接口已经迁移到 offLineRecorder.h。
 * 新代码请直接包含 offLineRecorder.h。
 */
uint16_t offlinePgmer(void);

#endif /* __OFFLINE_PGMER_H__ */
