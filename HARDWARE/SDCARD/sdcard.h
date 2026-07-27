#ifndef __SDCARD_H
#define __SDCARD_H

#include "sys.h"
#include "Hardware_Config.h"
#include "ff.h"

/*
 * SD 卡调试与工作参数
 * 说明：
 * 1. 先在 1-bit 模式下完成初始化与基础读写验证。
 * 2. 若打开 SD_USE_4BIT_MODE，则在 1-bit 正常后切到 4-bit，再做一次数据通路验证。
 * 3. 若打开 SD_ENABLE_SPEED_SCAN，则执行速度扫描测试。
 * 4. 默认仅保留扫描日志，不改变最终工作时钟；若需要按扫描结果生效，再打开 SD_SPEED_SCAN_APPLY_RESULT。
 */
#ifndef DEBUG_SD
#define DEBUG_SD                    1
#endif

#ifndef SD_CLK_INIT_DIV
#define SD_CLK_INIT_DIV             178U
#endif

#ifndef SD_CLK_WORK_DIV
#define SD_CLK_WORK_DIV             40U
#endif

#ifndef SD_CLK_4BIT_VERIFY_DIV
#define SD_CLK_4BIT_VERIFY_DIV      40U
#endif

#ifndef SD_USE_4BIT_MODE
#define SD_USE_4BIT_MODE            0U
#endif

#ifndef SD_ENABLE_SPEED_SCAN
#define SD_ENABLE_SPEED_SCAN        0U
#endif

#ifndef SD_SPEED_SCAN_APPLY_RESULT
#define SD_SPEED_SCAN_APPLY_RESULT  0U
#endif

#ifndef SD_ENABLE_DYNAMIC_RETUNE
#define SD_ENABLE_DYNAMIC_RETUNE    0U
#endif

#ifndef SD_SINGLE_BLOCK_RETRY
#define SD_SINGLE_BLOCK_RETRY       3U
#endif

/* SD 卡类型 */
typedef enum
{
    SD_TYPE_UNKNOWN = 0,
    SD_TYPE_MMC     = 1,
    SD_TYPE_V1      = 2,
    SD_TYPE_V2      = 3,
    SD_TYPE_V2HC    = 4
} SDCardType_t;

/* SD 卡基础信息 */
typedef struct
{
    SDCardType_t type;      /* 卡类型 */
    volatile u8  state;     /* 当前状态 */
    u16          rca;       /* 相对卡地址 RCA */
    u32          blk_cnt;   /* 扇区总数 */
    u32          blk_size;  /* 扇区大小，通常 512 字节 */
    u32          capacity;  /* 总容量，单位字节 */
    u32          cid[4];    /* CID 原始寄存器 */
    u32          csd[4];    /* CSD 原始寄存器 */
    u8           bus_width; /* 当前总线宽度：1 或 4 */
} SDCardInfo_t;

extern SDCardInfo_t SDCard_Info;

#define SD_OK           0x00
#define SD_ERROR        0x01
#define SD_TIMEOUT      0x02
#define SD_BLOCK_SIZE   512U

u8 SD_Init(void);
u8 SD_Detect(void);
u8 SD_ReadBlocks(u8 *buf, u32 sector, u32 count);
u8 SD_WriteBlocks(const u8 *buf, u32 sector, u32 count);
u8 SD_ReadSingleBlock(u8 *buf, u32 sector);
u8 SD_WriteSingleBlock(const u8 *buf, u32 sector);
u8 SD_GetCardInfo(void);
void SD_StopTransfer(void);
u8 SD_WaitReady(void);

/*
 * 速度扫描测试接口
 * width       : 1 或 4
 * sectors     : 参与测试的扇区集合
 * sectorCount : 测试扇区数量
 * 返回值      : SD_OK 表示至少有一档通过；SD_ERROR 表示全部失败
 */
u8 SD_TuneBusSpeedForSectors(u8 width, const u32 *sectors, u8 sectorCount);

void SD_DMA_Init(void);
u8   SD_ReadBlocks_DMA_Start(u8 *buf, u32 sector, u32 count);       /* 启动 DMA 多块读 */
u8   SD_ReadBlocks_DMA_IsFinished(void);                           /* 查询 DMA 多块读是否完成 */
u8   SD_WriteBlocks_DMA_Start(const u8 *buf, u32 sector, u32 count);/* 启动 DMA 多块写 */
u8   SD_WriteBlocks_DMA_IsFinished(void);                          /* 查询 DMA 多块写是否完成 */
u8   SD_DMA_GetResult(void);                                       /* 获取最近一次 DMA 传输结果 */
u8   SD_ReadBlocks_DMA(u8 *buf, u32 sector, u32 count);             /* DMA 多块读（阻塞兼容接口） */
u8   SD_WriteBlocks_DMA(const u8 *buf, u32 sector, u32 count);      /* DMA 多块写（阻塞兼容接口） */

#endif
