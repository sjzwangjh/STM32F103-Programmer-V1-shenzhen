/*-----------------------------------------------------------------------/
/  Low level disk I/O module header  (CHN, 2026)
/-----------------------------------------------------------------------*/

#ifndef _DISKIO_H
#define _DISKIO_H

#include "integer.h"

/* Status of Disk Functions */
typedef BYTE DSTATUS;

/* Disk Status Flags */
#define STA_NOINIT          0x01    /* Drive not initialized */
#define STA_NODISK          0x02    /* No medium in the drive */
#define STA_PROTECT         0x04    /* Write protected */

/* Results of Disk Functions */
typedef enum {
    RES_OK = 0,     /* 0: Successful */
    RES_ERROR,      /* 1: R/W Error */
    RES_WRPRT,      /* 2: Write Protected */
    RES_NOTRDY,     /* 3: Not Ready */
    RES_PARERR      /* 4: Invalid Parameter */
} DRESULT;

/*---------------------------------------*/
/* Prototypes for disk control functions */

DSTATUS disk_initialize(BYTE pdrv);
DSTATUS disk_status(BYTE pdrv);
DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count);
DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count);
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff);
void    disk_set_dma_mode(BYTE enable);
BYTE    disk_get_dma_mode(void);

/* Disk Control Commands */
#define CTRL_SYNC           0   /* Flush pending data (used by f_sync) */
#define GET_SECTOR_COUNT    1   /* Get media size (used by f_mkfs) */
#define GET_SECTOR_SIZE     2   /* Get sector size (used by f_mkfs) */
#define GET_BLOCK_SIZE      3   /* Get erase block size (used by f_mkfs) */
#define CTRL_TRIM           4   /* Inform device about unused sectors */

/* FatFs R0.15 API ºÊ»›∂®“Â */
DWORD get_fattime(void);

#endif


