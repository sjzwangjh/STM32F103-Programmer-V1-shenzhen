#ifndef __OFFLINE_PGMER_H__
#define __OFFLINE_PGMER_H__

#include "offLineRecorder.h"
#include <stdint.h>

/* Shared replay context: header/state of the package being replayed. */
typedef struct
{
    uint8_t initialized;                        /* init flag */
    uint16_t active_index;                     /* active package index */
    uint32_t package_addr;                     /* package base in SPI flash */
    offline_raw_package_header_t header;       /* validated package header */
} offline_replay_context_t;

/* Shared replay state/helpers exposed to the AVR/PIC replay groups. */
extern offline_replay_context_t g_replay;
extern uint8_t g_replayFrame[BUFFER_SIZE];
uint8_t offlineReadPacket(uint32_t *cursor, uint32_t packetNo,
                          offline_raw_packet_header_t *packetHeader);
uint8_t offlineExecuteFrame(uint16_t frameLen);

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
