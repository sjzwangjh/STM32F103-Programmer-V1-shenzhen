#ifndef __OFFLINE_RECORDER_H__
#define __OFFLINE_RECORDER_H__

#include "sys.h"
#include <stdint.h>
#include "avrDeviceConst.h"
#include "picDeviceConst.h"
#include "Stk500Protocol.h"


/*
 * offLineRecorder 只负责“数据包记录”。
 * 离线包内容保存为 START_PROG 到 STOP_PROG 之间收到的完整 STK500 原始帧。
 * Flash/EEPROM/Fuse 的旧结构化分段保存链路已经剔除, 避免同一烧录数据重复存储。
 */

/* STK500 扩展工作模式。 */
#define STK500_WORK_MODE_SIMULATE       0U      /* deprecated: no longer accepted from host */
#define STK500_WORK_MODE_ONLINE         1U      /* 在线模式: 直接按上位机命令烧录目标芯片 */
#define STK500_WORK_MODE_RECORD         2U      /* record: host commands stored to board flash, no target I/O */
#define STK500_WORK_MODE_REPLAY         3U      /* replay: handler-triggered, program target from board flash package */

/* 一次编程会话的记录状态, 由 CMD_SET_PROG_STATE 控制。 */
#define STK500_PROGRAM_IDLE             0U      /* 空闲: 当前没有打开离线包文件 */
#define STK500_PROGRAM_RECORDING        1U      /* 记录中: START_PROG 到 STOP_PROG 之间 */

#define OFFLINE_PGMER_STATUS_OK         0x00U
#define OFFLINE_PGMER_STATUS_FAILED     0xC0U

/* Raw STK500 离线包格式的魔术字和版本号。 */
#define OFFLINE_RAW_MAGIC       0x4B504C4FUL  /* "OLPK" */
#define OFFLINE_RAW_COMMIT_MAGIC 0x4D434C4FUL  /* "OLCM" */
#define OFFLINE_ACTIVE_MAGIC    0x43414C4FUL  /* "OLAC" */
#define OFFLINE_RAW_VERSION     1U
#define OFFLINE_MAX_PACKAGES    32U

/* Debug mode: keep only one package (slot 0), overwrite it on next recording.
 * Set to 0 to restore multi-package recording. */
#define OFFLINE_SINGLE_PACKET_MODE  1U

/* SPI Flash 索引表中的离线包状态。 */
#define OFFLINE_PACKAGE_EMPTY   0U
#define OFFLINE_PACKAGE_WRITING 1U
#define OFFLINE_PACKAGE_VALID   2U
#define OFFLINE_PACKAGE_DELETED 3U

/* Raw packet flags: 当前只记录上位机发到下位机的 RX 包。 */
#define OFFLINE_RAW_PACKET_RX   0x01U

/* 离线包目标架构。 */
#define OFFLINE_ARCH_AVR8       1U            /* AVR 8-bit */
#define OFFLINE_ARCH_PIC8       2U            /* PIC 8-bit */

/* PIC/AVR 器件参数联合体。离线执行时按 device_arch 选择对应成员。 */
typedef union mcuParamUnion{
    pic_prog_params_t picParam;     /* PIC 器件参数 */
    avr_prog_params_t avrParam;     /* AVR 器件参数 */
} mcuParamUnion_t;

/* 当前激活的离线编程目标信息。 */
typedef struct offlineDeviceParams{
    uint8_t     device_arch;        /* 器件架构: STK_MCU_ARCH_AVR / STK_MCU_ARCH_PIC */
    uint16_t    device_index;       /* 器件索引号, 来自上位机下发的 device identity */
    uint8_t     item_id[STK_PARAM_ITEM_ID_LEN]; /* 项目 ID, 按 device identity 原样保存 */
    uint8_t     item_desc[64];      /* 项目描述字符串 */
    mcuParamUnion_t device_params;  /* 当前器件的完整编程参数 */
    uint32_t    device_opeator_bit; /* 预留: 离线执行控制位 */
    uint8_t     reserved[10];       /* 预留字节 */
} offlineDeviceParams_t;

/* Raw STK500 离线包头。每个离线包在 SPI Flash 中以该结构开头。 */
typedef struct {
    uint32_t magic;                     /* OFFLINE_RAW_MAGIC */
    uint16_t version;                   /* OFFLINE_RAW_VERSION */
    uint16_t header_size;               /* 本结构大小 */
    uint16_t package_index;             /* 离线包序号 */
    uint16_t package_state;             /* OFFLINE_PACKAGE_xxx */
    stkDeviceIdentity_t identity;       /* 上位机下发的器件和项目信息 */
    uint32_t packet_count;              /* 包内 raw STK500 数据包数量 */
    uint32_t packet_area_offset;        /* raw packet 区相对包起始偏移 */
    uint32_t packet_area_size;          /* raw packet 区长度 */
    uint32_t total_size;                /* 整个离线包总长度 */
    uint32_t start_timestamp;           /* 预留: 开始记录时间 */
    uint32_t end_timestamp;             /* 预留: 结束记录时间 */
    uint32_t crc32;                     /* 简易校验值 */
    uint8_t  reserved[32];              /* 格式扩展预留 */
} offline_raw_package_header_t;

/* Tail commit record appended after raw packets. */
typedef struct {
    uint32_t magic;                     /* OFFLINE_RAW_COMMIT_MAGIC */
    uint16_t version;                   /* OFFLINE_RAW_VERSION */
    uint16_t commit_size;               /* sizeof(this struct) */
    uint16_t package_index;             /* offline package slot */
    uint16_t package_state;             /* OFFLINE_PACKAGE_VALID */
    stkDeviceIdentity_t identity;       /* final device/project identity */
    uint32_t packet_count;              /* raw STK500 frame count */
    uint32_t packet_area_size;          /* bytes occupied by raw packets */
    uint32_t total_size;                /* begin header + packets + commit */
    uint32_t crc32;                     /* accumulated raw packet checksum */
    uint8_t  reserved[16];
} offline_raw_package_commit_t;

/* Raw STK500 数据包头。每个被记录的 STK500 帧前面都追加该头。 */
typedef struct {
    uint16_t frame_len;                 /* 完整 STK500 帧长度 */
    uint16_t payload_len;               /* STK500 payload 长度 */
    uint8_t  cmd;                       /* payload[0], 即 STK 命令号 */
    uint8_t  flags;                     /* OFFLINE_RAW_PACKET_RX 等标志 */
    uint16_t seq;                       /* raw 包内部递增序号 */
    uint32_t crc32;                     /* 本帧简易校验 */
} offline_raw_packet_header_t;

/* SPI Flash 中的离线包索引项。索引表用于快速列举、查询和定位离线包。 */
typedef struct {
    uint8_t  used;                      /* 1=该索引项有效 */
    uint8_t  package_state;             /* OFFLINE_PACKAGE_xxx */
    uint16_t package_index;             /* 离线包序号 */
    uint32_t flash_addr;                /* 离线包在 SPI Flash 中的起始地址 */
    uint32_t total_size;                /* 离线包总长度 */
    uint32_t packet_area_size;          /* raw packet area size */
    uint32_t packet_count;              /* raw STK500 数据包数量 */
    uint32_t crc32;                     /* 包校验值 */
    stkDeviceIdentity_t identity;       /* 摘要中的器件和项目描述 */
} offline_package_index_t;

/* EEPROM 中保存的当前激活离线包记录。 */
typedef struct {
    uint32_t magic;                     /* OFFLINE_ACTIVE_MAGIC */
    uint16_t version;                   /* OFFLINE_RAW_VERSION */
    uint16_t active_index;              /* 当前激活离线包序号 */
    uint32_t active_flash_addr;         /* 激活包起始地址 */
    uint32_t active_crc32;              /* 激活包校验值 */
    uint8_t  boot_action;               /* 预留: 上电后是否自动执行 */
    uint8_t  reserved[15];              /* 对齐和扩展预留 */
    uint32_t crc32;                     /* 本记录校验 */
} offline_active_record_t;

/* 上位机查询离线包总体信息时返回的摘要。 */
typedef struct {
    uint16_t package_count;             /* 当前有效离线包数量 */
    uint16_t active_index;              /* EEPROM 中记录的激活包序号 */
    uint16_t max_count;                 /* 最大支持离线包数量 */
} offline_package_info_t;

extern uint8_t g_stkWorkMode;
extern offlineDeviceParams_t g_activeDeviceParams;

void offlinePgmerInit(void);
void offlinePgmerInitWith(stkDeviceIdentity_t* di);

uint8_t stkSetWorkMode(uint8_t mode);
uint8_t stkGetWorkMode(void);
uint8_t stkIsOnlineMode(void);
uint8_t stkIsRecordMode(void);

uint8_t offlinePgmerRawBegin(const stkDeviceIdentity_t *identity);
uint8_t offlinePgmerRawAppendRxPacket(const uint8_t *frame, uint16_t frameLen);
uint8_t offlinePgmerRawEnd(void);
uint16_t offlinePgmerRawReadBack(uint8_t readCmd, uint32_t addr, const uint8_t *readFrame, uint8_t *out, uint16_t outCap);
uint8_t offlinePgmerGetOfflineInfo(offline_package_info_t *info);
uint8_t offlinePgmerGetPackageSummary(uint16_t index, offline_package_index_t *summary);
uint8_t offlinePgmerSetActivePackage(uint16_t index);
uint8_t offlinePgmerGetActivePackage(uint16_t *index);

#endif /* __OFFLINE_RECORDER_H__ */
