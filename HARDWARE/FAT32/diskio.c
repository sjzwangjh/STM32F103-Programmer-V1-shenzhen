/*
 * 底层磁盘I/O接口实现 - 桥接FatFs文件系统与SD卡SDIO驱动
 */

/*-----------------------------------------------------------------------/
/  Low level disk I/O module
/  Bridges FatFs to SD Card driver via SDIO
/-----------------------------------------------------------------------*/

#include "diskio.h"
#include "sdcard.h"
#include "ffconf.h"

#define DRV_NUM     1               /* 磁盘驱动器数量 */

/* 各驱动器状态（初始化为未初始化） */
static DSTATUS diskState[DRV_NUM] = {STA_NOINIT};
static BYTE    diskDmaEnabled = 1;  /* DMA多块传输使能标志 */
static BYTE    diskDmaInited = 0;   /* DMA是否已初始化 */

static void disk_retune_for_failure(DWORD sector, UINT count)
{
#if SD_ENABLE_DYNAMIC_RETUNE
    u32 tuneSectors[3];
    u8 tuneCount;

    tuneCount = 0U;
    tuneSectors[tuneCount++] = 0U;
    tuneSectors[tuneCount++] = sector;
    if (count > 1U)
        tuneSectors[tuneCount++] = sector + (DWORD)count - 1UL;

    (void)SD_TuneBusSpeedForSectors(SDCard_Info.bus_width, tuneSectors, tuneCount);
#else
    (void)sector;
    (void)count;
#endif
}

/**
 * disk_initialize - 初始化磁盘驱动器（SD卡）
 * pdrv : 物理驱动器号
 * 返回: 磁盘状态（0=正常，STA_NOINIT=失败）
 */
DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv >= DRV_NUM)
        return STA_NOINIT;

    /* 若上层已经手动完成过 SD_Init()，这里不要重复初始化。 */
    if (SD_Detect() == SD_OK)
    {
        if (diskDmaInited == 0U)
        {
            SD_DMA_Init();
            diskDmaInited = 1U;
        }
        diskState[pdrv] = 0;
        return diskState[pdrv];
    }

    if (SD_Init() == SD_OK)
    {
        if (diskDmaInited == 0U)
        {
            SD_DMA_Init();
            diskDmaInited = 1U;
        }
        diskState[pdrv] = 0;
        return diskState[pdrv];
    }

    diskState[pdrv] = STA_NOINIT;
    return diskState[pdrv];
}

/**
 * disk_status - 获取磁盘驱动器状态
 * pdrv : 物理驱动器号
 * 返回: STA_NOINIT=未初始化, 0=就绪
 */
DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv >= DRV_NUM)
        return STA_NOINIT;

    return diskState[pdrv];
}

/**
 * disk_read - 从磁盘读取数据扇区
 * pdrv  : 物理驱动器号
 * buff  : 数据缓冲区指针
 * sector: 起始扇区号（LBA地址）
 * count : 要读取的扇区数
 * 返回: RES_OK=成功, 其他=错误码
 */
DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
    u8 sdRes;

    if (pdrv >= DRV_NUM)
        return RES_PARERR;
    if (buff == 0 || count == 0)
        return RES_PARERR;
    if (diskState[pdrv] & STA_NOINIT)
        return RES_NOTRDY;

    if (count == 1U)
    {
        sdRes = SD_ReadSingleBlock(buff, sector);
        if (sdRes == SD_OK)
            return RES_OK;

        disk_retune_for_failure(sector, count);
        return (SD_ReadSingleBlock(buff, sector) == SD_OK) ? RES_OK : RES_ERROR;
    }

    if ((diskDmaEnabled != 0U) && (diskDmaInited != 0U))
    {
        sdRes = SD_ReadBlocks_DMA(buff, sector, count);
        if (sdRes == SD_OK)
            return RES_OK;
    }

    sdRes = SD_ReadBlocks(buff, sector, count);
    if (sdRes == SD_OK)
        return RES_OK;

    disk_retune_for_failure(sector, count);
    return (SD_ReadBlocks(buff, sector, count) == SD_OK) ? RES_OK : RES_ERROR;
}

/**
 * disk_write - 向磁盘写入数据扇区
 * pdrv  : 物理驱动器号
 * buff  : 待写入数据缓冲区指针
 * sector: 起始扇区号（LBA地址）
 * count : 要写入的扇区数
 * 返回: RES_OK=成功, 其他=错误码
 */
DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
    u8 sdRes;

    if (pdrv >= DRV_NUM)
        return RES_PARERR;
    if (buff == 0 || count == 0)
        return RES_PARERR;
    if (diskState[pdrv] & STA_NOINIT)
        return RES_NOTRDY;

    if (count == 1U)
    {
        sdRes = SD_WriteSingleBlock(buff, sector);
        if (sdRes == SD_OK)
            return RES_OK;

        disk_retune_for_failure(sector, count);
        return (SD_WriteSingleBlock(buff, sector) == SD_OK) ? RES_OK : RES_ERROR;
    }

    if ((diskDmaEnabled != 0U) && (diskDmaInited != 0U))
    {
        sdRes = SD_WriteBlocks_DMA(buff, sector, count);
        if (sdRes == SD_OK)
            return RES_OK;
    }

    sdRes = SD_WriteBlocks(buff, sector, count);
    if (sdRes == SD_OK)
        return RES_OK;

    disk_retune_for_failure(sector, count);
    return (SD_WriteBlocks(buff, sector, count) == SD_OK) ? RES_OK : RES_ERROR;
}

/**
 * disk_ioctl - 磁盘设备控制（获取容量/扇区大小/擦除块大小等）
 * pdrv: 物理驱动器号
 * cmd : 控制命令（CTRL_SYNC/GET_SECTOR_COUNT等）
 * buff: 命令参数/结果缓冲区
 * 返回: RES_OK=成功, 其他=错误码
 */
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    if (pdrv >= DRV_NUM)
        return RES_PARERR;
    if (diskState[pdrv] & STA_NOINIT)
        return RES_NOTRDY;

    switch (cmd)
    {
    case CTRL_SYNC:
        return RES_OK;

    case GET_SECTOR_COUNT:
        if (buff == 0)
            return RES_PARERR;
        *(DWORD *)buff = SDCard_Info.blk_cnt;
        return RES_OK;

    case GET_SECTOR_SIZE:
        if (buff == 0)
            return RES_PARERR;
        *(WORD *)buff = (WORD)SDCard_Info.blk_size;
        return RES_OK;

    case GET_BLOCK_SIZE:
        if (buff == 0)
            return RES_PARERR;
        *(DWORD *)buff = 1;
        return RES_OK;

    default:
        return RES_PARERR;
    }
}

/**
 * get_fattime - 获取当前FAT时间戳（使用ffconf.h中的固定日期）
 * 返回: 符合FAT格式的时间戳（年/月/日编码）
 */
DWORD get_fattime(void)
{
    return ((DWORD)(FF_NORTC_YEAR - 1980U) << 25)
         | ((DWORD)FF_NORTC_MON << 21)
         | ((DWORD)FF_NORTC_MDAY << 16);
}

/**
 * disk_set_dma_mode - 设置DMA多块传输模式使能状态
 * enable: 1=使能DMA, 0=关闭DMA（使用轮询模式）
 */
void disk_set_dma_mode(BYTE enable)
{
    diskDmaEnabled = (enable != 0U) ? 1U : 0U;
}

/**
 * disk_get_dma_mode - 获取当前DMA多块传输模式状态
 * 返回: 1=DMA使能, 0=DMA关闭
 */
BYTE disk_get_dma_mode(void)
{
    return diskDmaEnabled;
}
