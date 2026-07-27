/*
 * FatFs文件系统配置文件 - 开启/关闭功能模块（适用于STM32F103+SD卡）
 */

/*-------------------------------------------*/
/* FatFs Configuration for STM32F103 + SDCARD */
/*-------------------------------------------*/

#ifndef _FFCONF_H
#define _FFCONF_H

/*---------------------------------------------------------------------------/
/  Function Configurations (0=Disable, 1=Enable)
/---------------------------------------------------------------------------*/

#define FF_FS_READONLY          0   /* 1: Read-only, 0: Read/Write */
#define FF_FS_MINIMIZE          0   /* 0: All functions enabled */
#define FF_USE_STRFUNC          0   /* 0: No string functions */
#define FF_USE_FIND             0   /* 0: No find functions */
#define FF_USE_MKFS             1   /* 1: Enable f_mkfs */
#define FF_USE_FASTSEEK         0   /* 0: No fast seek */
#define FF_USE_EXPAND           0   /* 0: No expand */
#define FF_USE_CHMOD            0   /* 0: No chmod */
#define FF_USE_LABEL            0   /* 0: No volume label */
#define FF_USE_FORWARD          0   /* 0: No forward */

/*---------------------------------------------------------------------------/
/  Locale and Namespace Configurations
/---------------------------------------------------------------------------*/

#define FF_CODE_PAGE            936 /* GBK (Simplified Chinese) */
#define FF_USE_LFN              2   /* 0: 8.3 only, 1: LFN with static buffer, 2: LFN on heap */
#define FF_MAX_LFN              255 /* Max LFN length */
#define FF_LFN_UNICODE          0   /* 0: ANSI/OEM, 1: Unicode */
#define FF_STRF_ENCODE          0   /* 0: ANSI/OEM */

/*---------------------------------------------------------------------------/
/  Volume / Drive / Partition Configurations
/---------------------------------------------------------------------------*/

#define FF_VOLUMES              1   /* Number of volumes (drives) */
#define FF_STR_VOLUME_ID        0   /* 0: Use drive number only */
#define FF_MULTI_PARTITION      0   /* 0: Single partition per physical drive */
#define FF_MIN_SS               512 /* Minimum sector size */
#define FF_MAX_SS               512 /* Maximum sector size (always 512 for SD) */
#define FF_LBA64                0   /* 0: 32-bit LBA, 1: 64-bit LBA */
#define FF_MIN_GPT              0   /* 0: No GPT */
#define FF_USE_TRIM             0   /* 0: No TRIM */

/*---------------------------------------------------------------------------/
/  System Configurations
/---------------------------------------------------------------------------*/

#define FF_FS_TINY              0   /* 0: Normal, 1: Tiny (reduces RAM) */
#define FF_FS_EXFAT             0   /* 0: No exFAT, 1: exFAT */
#define FF_FS_NORTC             1   /* 1: No RTC (use fixed timestamp) */
#define FF_NORTC_MON            1   /* Fixed month */
#define FF_NORTC_MDAY           1   /* Fixed day */
#define FF_NORTC_YEAR           2020/* Fixed year */
#define FF_FS_NOFSINFO          0   /* 0: Use fsinfo for speed */
#define FF_FS_LOCK              0   /* 0: No file lock */
#define FF_FS_REENTRANT         0   /* 0: No reentrancy */

#endif


