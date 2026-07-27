/* avrOffLinePgm.h */
#ifndef __AVR_OFFLINE_PGM_H__
#define __AVR_OFFLINE_PGM_H__
#include <stdint.h>
#include "avrDeviceConst.h"

/* ── 分段头 (每个数据段一个, 12 字节) ───────────────────────── */

/**
 * @brief  数据段描述符
 *
 * 每个 Flash/EEPROM 有效段对应一个 segment_header_t,
 * 紧跟着 segment_header_t 后面是 data_size 字节的实际数据。
 *
 * 示例:
 *   segment: addr=0x0000, size=256
 *   → segment_header_t {.addr=0x0000, .data_size=256, .section_type=FLASH_DATA}
 *   → 0x0000~0x00FF 的 256 字节 Flash 数据紧接其后
 *
 * 如果 HEX 文件在 Flash 中只有 3 个分散的段 (0x0000~256, 0x1000~128, 0x7000~64),
 * 则存储 3 个 segment_header_t + 对应数据。
 */
typedef struct {
    uint32_t addr;              /* 起始地址 (AVR: 字节地址; PIC: 字地址) */
    uint16_t data_size;         /* 数据大小 (字节数) */
    uint16_t section_type;      /* 段落类型 (OFFLINE_SEC_FLASH_DATA 等) */
    uint32_t crc32;             /* 本段数据的 CRC32 校验 */
} offline_segment_t;

/* ── Fuse/Config 条目 (每个一条, 3 字节) ────────────────────── */
/**
 * @brief  熔丝/配置字条目
 *
 * 格式: key-value 对
 *   AVR:  key = fuse 编号 (0=low, 1=high, 2=extended, 3=lock)
 *   PIC:  key = config word 索引 (0=CONFIG1, 1=CONFIG2, ...)
 */
typedef struct {
    uint8_t  key;               /* 编号 (AVR: fuse编号; PIC: config字索引) */
    uint16_t value;             /* 值 (小端序) */
} offline_fuse_entry_t;

/* ── ISP 命令模板 (仅 AVR ISP 模式使用) ─────────────────────── */

/**
 * @brief  ISP 时序与命令模板
 *
 * 离线编程时, AVR ISP 需要知道 SPI 命令序列。
 * 数据来源: STK500 协议中的 SET_PARAMETER + ENTER_PROGMODE_ISP 命令。
 * PIC ICSP 模式不需要这些字段 (全部填 0)。
 */
typedef struct {
    uint8_t  prog_mode;         /* ISP 编程模式字节 */
    uint8_t  prog_delay;        /* 编程后延时 (us) */
    uint8_t  poll_val;          /* 轮询值 */
    uint8_t  poll_idx;          /* 轮询字节索引 */
    uint8_t  cmd_chip_erase[4]; /* 整片擦除命令 */
    uint8_t  cmd_prog_flash[4]; /* Flash 编程命令 */
    uint8_t  cmd_read_flash[4]; /* Flash 读取命令 */
    uint8_t  cmd_prog_eeprom[4];/* EEPROM 编程命令 */
    uint8_t  cmd_read_eeprom[4];/* EEPROM 读取命令 */
    uint8_t  cmd_prog_fuse[4];  /* Fuse 编程命令 */
    uint8_t  cmd_read_fuse[4];  /* Fuse 读取命令 */
} offline_isp_template_t;


typedef enum { OFP_IDLE, OFP_CONNECTED, OFP_PROGRAMMING, OFP_VERIFYING, OFP_COMPLETE, OFP_ERROR } OfflinePgmState;
typedef struct {
    OfflinePgmState state; uint16_t device_index;
    avr_prog_params_t device_params;      /* 由 avrDeviceConst 展开后的器件参数 */
    uint32_t flash_addr, eeprom_addr; 
    uint16_t page_buf_idx; 
    uint8_t page_buf[256];
} AvrOfflineCtx;

void avr_init(AvrOfflineCtx* ctx);
int avr_select_device(AvrOfflineCtx* ctx, uint16_t idx);
int avr_select_by_signature(AvrOfflineCtx* ctx, const uint8_t sig[3]);
int avr_get_param_packet(AvrOfflineCtx* ctx, uint8_t* buffer, uint16_t bufsize);
int avr_make_enter_progmode_packet(AvrOfflineCtx* ctx, uint8_t* buffer);
int avr_make_leave_progmode_packet(AvrOfflineCtx* ctx, uint8_t* buffer);
int avr_make_chip_erase_packet(AvrOfflineCtx* ctx, uint8_t* buffer);
#endif


