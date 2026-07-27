/*
 * avrDeviceConst.h — AVR 器件参数常量表 (共享参数表法)
 *
 * 压缩策略: 将 392 个器件的 op[11] (完全重复) 和 mem[8] (高度重复)
 * 提取到共享表中, 每个器件仅存差异字段 + 索引, 从 ~63KB 压缩到 ~24KB。
 */
#ifndef __AVR_DEVICE_CONST_H__
#define __AVR_DEVICE_CONST_H__
#include <stdint.h>

/*
 * STK500 ISP 协议固定的 SPI 命令数量 (11 个)
 * 通过: AVR_OP_PGM_ENABLE(进入编程), AVR_OP_CHIP_ERASE(整片擦除), AVR_OP_READ/READ_LO/READ_HI(读取 flash/低字节/高字节),
 *       AVR_OP_WRITE/WRITE_LO/WRITE_HI(写入 flash/低字节/高字节), AVR_OP_LOADPAGE_LO/LOADPAGE_HI(写页缓存),
 *       AVR_OP_LOAD_EXT_ADDR(扩展地址)
 */
#define AVR_OP_MAX     11

/* AVR 存储器类型数量: flash, eeprom, lfuse, hfuse, efuse, lock, signature, calibration */
#define AVR_MEM_MAX    8

/* 共享 OP 组数量: 所有 AVR ISP 器件的 SPI 命令序列完全相同, 仅存 1 组 */
#define AVR_OP_SHARED_COUNT  1

/* 共享 MEM 组数量: 当前生成表只保留 1 组默认 classic ISP 参数 */
#define AVR_MEM_SHARED_COUNT 1

/* AVR 器件总数量 (来自 AVRDUDE avrdude.conf) */
#define AVR_DEVICE_COUNT 392

/* 器件名称字符串长度 */
#ifndef     DEVICE_NAME_CHAR_LENGTH
#define     DEVICE_NAME_CHAR_LENGTH     16
#endif

/*
 * AVR 编程操作类型枚举
 * 与 STK500 ISP 协议中的 SPI 命令一一对应,
 * 索引 0~10 对应 g_avrOpGroups[0].op[0~10]
 */
typedef enum {
    AVR_OP_PGM_ENABLE = 0,      /* 进入 ISP 编程模式 */
    AVR_OP_CHIP_ERASE = 1,      /* 整片擦除 */
    AVR_OP_READ        = 2,     /* 读 Flash (字节) */
    AVR_OP_READ_LO     = 3,     /* 读 Flash 低字节 (部分器件) */
    AVR_OP_READ_HI     = 4,     /* 读 Flash 高字节 (部分器件) */
    AVR_OP_WRITE       = 5,     /* 写 Flash (字节) */
    AVR_OP_WRITE_LO    = 6,     /* 写 Flash 低字节 (部分器件) */
    AVR_OP_WRITE_HI    = 7,     /* 写 Flash 高字节 (部分器件) */
    AVR_OP_LOADPAGE_LO = 8,     /* 加载页缓存低字节 */
    AVR_OP_LOADPAGE_HI = 9,     /* 加载页缓存高字节 */
    AVR_OP_LOAD_EXT_ADDR = 10   /* 加载扩展地址 */
} AVR_OpType;

/*
 * AVR 存储器类型枚举
 * 对应 AVR 芯片的不同存储器区域,
 * 索引 0~7 对应 AVR_MemGroup.mem[0~7]
 */
typedef enum {
    AVR_MEM_FLASH,              /* Flash 程序存储器 */
    AVR_MEM_EEPROM,             /* EEPROM 数据存储器 */
    AVR_MEM_LFUSE,              /* 低熔丝位 */
    AVR_MEM_HFUSE,              /* 高熔丝位 */
    AVR_MEM_EFUSE,              /* 扩展熔丝位 */
    AVR_MEM_LOCK,               /* 锁定位 */
    AVR_MEM_SIGNATURE,          /* 器件签名 (3 字节) */
    AVR_MEM_CALIBRATION         /* 出厂校准字节 */
} AVR_MemType;

/**
 * @brief 单个 SPI 编程命令 (5 字节)
 *
 * STK500 ISP 协议中每个编程操作对应一条 SPI 命令:
 *   cmd[0..3] = 4 字节 SPI 发送数据
 *   data_index = 在返回响应中第几个字节是有效数据
 */
typedef struct {
    uint8_t cmd[4];             /* SPI 发送的 4 字节命令序列 */
    uint8_t data_index;         /* 响应中的有效数据偏移量 */
} AVR_SPICmd;

/**
 * @brief 一组 SPI 命令 (共享表, 所有 AVR ISP 器件通用)
 *
 * 存储在 Flash 中的 g_avrOpGroups[0] 常量,
 * 392 个 AVR 器件共享这 55 字节 (11 × 5)。
 * 节省约 21KB 的 Flash 空间。
 */
typedef struct {
    AVR_SPICmd op[AVR_OP_MAX];  /* 11 条 SPI 命令 */
} AVR_OpGroup;

/**
 * @brief 单个存储器区域参数 (8 字节)
 *
 * 描述 Flash/EEPROM/Fuse 等区域的容量、页大小、读写特性。
 * 不同器件的这些参数不同, 但相同型号族 (如 mega48/88/168/328)
 * 往往共享相同的 MEM 参数。
 */
typedef struct {
    uint16_t size;              /* 区域总大小 (字节) */
    uint16_t page_size;         /* 页大小 (字节), 编程时每次写入的最大字节数 */
    uint8_t  readsize;          /* 单次读取的字节数 */
    uint8_t  delay;             /* 写入后延时 (ms) */
    uint8_t  flags;             /* 区域属性标志 */
    uint8_t  mode;              /* STK500v2 Program_* 命令的 mode 字节 */
    uint8_t  readback[2];       /* 值轮询/读回比较字节 */
} AVR_MemSlot;

/**
 * @brief 一组存储器参数 (共享表)
 *
 * 存储在 Flash 中的 g_avrMemGroups[] 常量,
 * 每个条目 64 字节 (8 × 8)。
 * 约 15 种独特组合, 节省约 21KB。
 */
typedef struct {
    AVR_MemSlot mem[AVR_MEM_MAX]; /* 8 个存储器区域 */
} AVR_MemGroup;

/**
 * @brief AVR 器件索引条目 (压缩后约 44 字节)
 *
 * 每条目存储一个 AVR 器件的所有编程参数,
 * 名称通过 avrName[16] 直接内联, mem_group 查找存储器布局,
 * op 命令统一使用 g_avrOpGroups[0] (所有器件相同)。
 */
typedef struct {
    char     avrName[DEVICE_NAME_CHAR_LENGTH];  /* 器件名称字符串 (例如 "ATmega328P") */
    uint8_t  signature[3];          /* 3 字节器件签名 (如 ATmega328P: 0x1E 0x95 0x0F) */
    uint8_t  mem_group;             /* 存储器参数组索引 (0 ~ AVR_MEM_SHARED_COUNT-1) */
    uint8_t  flash_page_size_msb;   /* Flash 页大小高字节 */
    uint32_t flash_size;            /* Flash 总容量 (字节) */
    uint16_t eeprom_size;           /* EEPROM 总容量 (字节) */
    uint32_t chip_erase_delay;      /* 整片擦除延时 (ms), XML 中部分器件超过 65535 */
    uint8_t  fuse_count;            /* 熔丝位数 (通常 1~3) */
    uint8_t  timeout;               /* ISP 通信超时 (ms) */
    uint8_t  stabdelay;             /* ISP 进入编程模式后的稳定延时 (ms) */
    uint8_t  cmdexedelay;           /* ISP 命令执行延时 (ms) */
    uint8_t  synchloops;            /* ISP 同步循环次数 */
    uint8_t  pollvalue;             /* 编程轮询值 (与 pollindex 配合) */
    uint8_t  pollindex;             /* 编程轮询字节索引 (通常为 3) */
    uint8_t  flash_page_size_lsb;   /* Flash 页大小低字节 */
    uint8_t  stk500_devcode;        /* STK500 协议器件代号 */
    uint8_t  avr910_devcode;        /* AVR910 协议器件代号 */
    uint8_t  bytedelay;             /* 字节间延时 (μs) */
    uint8_t  runtime_group;         /* 扩展运行时参数组索引 */
} AVR_DeviceEntry;


/*
 * avrDude 原始 AVRMEM / AVRPART 结构很大，包含链表、缓冲区、开发者选项等 PC 侧字段。
 * STM32 端只保留当前协议层/离线层真正会用到的参数，字段名尽量与 avrDude 对齐，
 * 这样后续做参数导入、协议映射、对照 avrdude.conf 时更直接。
 */
typedef struct avrmem {
    const char *desc;             /* "flash"/"eeprom"/"lfuse"... */
    uint8_t   mem_index;          /* 对应 AVR_MemType */
    uint8_t   paged;              /* 是否按页编程 */
    uint16_t  size;               /* 总容量，单位 byte */
    uint16_t  page_size;          /* 页大小，单位 byte */
    uint8_t   readsize;           /* 单次读取粒度 */
    uint8_t   delay;              /* 写后等待参数 */
    uint8_t   flags;              /* 区域属性 */
    uint8_t   mode;               /* 在线页写/字写用 mode 字节 */
    uint8_t   readback[2];        /* 在线值轮询用 readback 字节 */
    AVR_SPICmd op[AVR_OP_MAX];    /* 当前存储区对应操作码，缺省直接复制共享 op */
} AVRMEM;

typedef struct avrpart {
    uint8_t   device_name[DEVICE_NAME_CHAR_LENGTH]; /* 短名称，来自器件名表 */
    uint8_t   signature[3];
    uint8_t   stk500_devcode;
    uint8_t   avr910_devcode;
    uint8_t   timeout;
    uint8_t   stabdelay;
    uint8_t   cmdexedelay;
    uint8_t   synchloops;
    uint8_t   bytedelay;
    uint8_t   pollvalue;
    uint8_t   pollindex;
    uint8_t   predelay;
    uint8_t   postdelay;
    uint8_t   pollmethod;
    uint32_t  chip_erase_delay;
    uint16_t  flash_page_size;
    uint32_t  flash_size;
    uint16_t  eeprom_size;
    uint8_t   fuse_count;
    uint8_t   hventerstabdelay;
    uint8_t   progmodedelay;
    uint8_t   latchcycles;
    uint8_t   togglevtg;
    uint8_t   poweroffdelay;
    uint8_t   resetdelayms;
    uint8_t   resetdelayus;
    uint8_t   hvleavestabdelay;
    uint8_t   resetdelay;
    uint8_t   chiperasepulsewidth;
    uint8_t   chiperasepolltimeout;
    uint8_t   chiperasetime;
    uint8_t   programfusepulsewidth;
    uint8_t   programfusepolltimeout;
    uint8_t   programlockpulsewidth;
    uint8_t   programlockpolltimeout;
    uint8_t   synchcycles;
    uint8_t   hvspcmdexedelay;
    AVR_SPICmd op[AVR_OP_MAX];
    AVRMEM    mem[AVR_MEM_MAX];
} AVRPART;

typedef AVRPART avr_prog_params_t;

/* ══════════════════════════════════════════════════════════════
 * 公开 API — avrDeviceConst 模块对外接口
 * ══════════════════════════════════════════════════════════════ */

/** 根据器件名称查找并填充 avr_prog_params_t (AVRPART) */
int avrFindDeviceByName(const char *deviceName, avr_prog_params_t *out);

/** 根据索引填充 avr_prog_params_t (AVRPART) */
int avrFindDeviceByIndex(uint16_t index, avr_prog_params_t *out);

#endif

