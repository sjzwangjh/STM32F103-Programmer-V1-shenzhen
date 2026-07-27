/*
 * FatFs API definitions (compact edition)
 * 当前工程保留标准 FatFs 的对象命名和 API 入口，
 * 并针对 STM32F103 + FAT32 根目录场景做了精简。
 */

/*-----------------------------------------------------------------------*/
/*  FatFs - FAT Filesystem Module API Definitions  R0.15                 */
/*-----------------------------------------------------------------------*/

#ifndef _FF_H
#define _FF_H

#include "integer.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================*/
/*  文件系统对象 FATFS - 存储单个分区的FAT文件系统信息                       */
/*===========================================================================*/
typedef struct {
    BYTE    fs_type;        /* 文件系统类型: 0=未挂载, FS_FAT12/16/32 */
    BYTE    pdrv;           /* 物理驱动器编号（0=SD卡） */
    BYTE    n_fats;         /* FAT表份数（通常为1或2） */
    BYTE    wflag;          /* 窗口缓冲区脏标志(1=win[]待刷写) */
    BYTE    fsi_flag;       /* FSINFO扇区脏标志(FAT32) */
    WORD    id;             /* 文件系统挂载ID（用于校验有效性） */
    WORD    n_rootdir;      /* 根目录最大目录项数（FAT12/16有效） */
    WORD    csize;          /* 每簇扇区数 */
    DWORD   n_fatent;       /* FAT表项总数（簇号范围） */
    DWORD   fatbase;        /* FAT表起始LBA扇区号 */
    DWORD   dirbase;        /* 根目录LBA（FAT32为簇号） */
    DWORD   database;       /* 数据区起始LBA扇区号 */
    DWORD   winsect;        /* 当前窗口win[]对应的扇区号 */
    BYTE    win[512];       /* 磁盘扇区读写窗口缓冲区（512字节） */
} FATFS;

/*===========================================================================*/
/*  文件对象 FIL - 代表一个已打开的文件                                      */
/*===========================================================================*/
typedef struct {
    FATFS*  fs;             /* 所属文件系统对象 */
    WORD    id;             /* 所属文件系统挂载ID（版本校验） */
    BYTE    flag;           /* 文件打开模式标志(FA_READ/FA_WRITE等) */
    BYTE    err;            /* 文件操作最后一次错误码 */
    DWORD   fptr;           /* 当前文件读写指针位置（字节偏移） */
    DWORD   fsize;          /* 文件大小（字节） */
    DWORD   sclust;         /* 文件起始簇号 */
    DWORD   clust;          /* 当前操作所在簇号 */
    DWORD   dsect;          /* 当前操作所在数据扇区号 */
    DWORD   dir_sect;       /* 文件目录项所在扇区号 */
    BYTE*   dir_ptr;        /* 指向文件目录项在win[]中的位置 */
} FIL;

/*===========================================================================*/
/*  目录对象 DIR - 用于遍历目录内容                                          */
/*===========================================================================*/
typedef struct {
    FATFS*  fs;             /* 所属文件系统对象 */
    WORD    id;             /* 文件系统挂载ID */
    WORD    index;          /* 当前遍历的目录项索引号 */
    DWORD   sclust;         /* 目录起始簇号 */
    DWORD   clust;          /* 当前簇号 */
    DWORD   sect;           /* 当前扇区号 */
    BYTE*   dir;            /* 指向当前目录项在win[]中的位置 */
    BYTE*   fn;             /* 当前文件名指针 */
    BYTE    lfn[256];       /* 长文件名缓冲区（当前未使用） */
} DIR;

/*===========================================================================*/
/*  文件信息结构体 FILINFO - 存储文件属性/大小/名称等信息                    */
/*===========================================================================*/
typedef struct {
    DWORD   fsize;          /* 文件大小（字节） */
    WORD    fdate;          /* 文件修改日期（FAT格式编码） */
    WORD    ftime;          /* 文件修改时间（FAT格式编码） */
    BYTE    fattrib;        /* 文件属性(AM_RDO/AM_HID/AM_SYS/AM_DIR/AM_ARC) */
    char    fname[13];      /* 8.3短文件名（含扩展名和结尾\0） */
} FILINFO;

/*===========================================================================*/
/*  文件属性掩码                                                             */
/*===========================================================================*/
#define AM_RDO  0x01    /* 只读属性 */
#define AM_HID  0x02    /* 隐藏属性 */
#define AM_SYS  0x04    /* 系统文件属性 */
#define AM_DIR  0x10    /* 目录属性 */
#define AM_ARC  0x20    /* 归档属性 */

/*===========================================================================*/
/*  文件打开模式标志                                                         */
/*===========================================================================*/
#define FA_READ           0x01    /* 读模式 */
#define FA_WRITE          0x02    /* 写模式 */
#define FA_OPEN_EXISTING  0x00    /* 仅打开已存在文件（默认） */
#define FA_CREATE_NEW     0x04    /* 创建新文件（若已存在则返回错误） */
#define FA_CREATE_ALWAYS  0x08    /* 创建新文件（若已存在则截断覆盖） */
#define FA_OPEN_ALWAYS    0x10    /* 打开文件（若不存在则创建） */

/*===========================================================================*/
/*  函数执行结果枚举 FRESULT - 所有API的返回值类型                           */
/*===========================================================================*/
typedef enum {
    FR_OK = 0,              /* 操作成功 */
    FR_DISK_ERR,            /* 底层磁盘I/O错误 */
    FR_INT_ERR,             /* 内部断言/逻辑错误 */
    FR_NOT_READY,           /* 磁盘驱动器未就绪 */
    FR_NO_FILE,             /* 文件不存在 */
    FR_NO_PATH,             /* 路径不存在 */
    FR_INVALID_NAME,        /* 文件名格式无效 */
    FR_DENIED,              /* 访问被拒绝（权限/模式不匹配） */
    FR_EXIST,               /* 文件已存在（无法创建新文件） */
    FR_INVALID_OBJECT,      /* 文件/目录对象无效 */
    FR_WRITE_PROTECTED,     /* 磁盘写保护 */
    FR_INVALID_DRIVE,       /* 无效的驱动器号 */
    FR_NOT_ENABLED,         /* 文件系统未挂载 */
    FR_NO_FILESYSTEM,       /* 磁盘上无有效文件系统 */
    FR_MKFS_ABORTED,        /* 格式化操作中止 */
    FR_TIMEOUT,             /* 操作超时 */
    FR_LOCKED,              /* 文件被锁定 */
    FR_NOT_ENOUGH_CORE,     /* 内存不足 */
    FR_TOO_MANY_OPEN_FILES, /* 打开的文件过多 */
    FR_INVALID_PARAMETER    /* 无效参数 */
} FRESULT;

/*===========================================================================*/
/*  字符类型定义 TCHAR - FatFs统一使用char作为路径字符串类型                 */
/*===========================================================================*/
#ifndef _TCHAR_DEFINED
#define _TCHAR_DEFINED
typedef char TCHAR;
#endif

/*===========================================================================*/
/*  文件系统API函数原型声明                                                  */
/*===========================================================================*/
FRESULT f_mount     (FATFS* fs, const TCHAR* path, BYTE opt);    /* 挂载/卸载文件系统 */
FRESULT f_open      (FATFS* fs, FIL* fp, const TCHAR* path, BYTE mode); /* 打开/创建文件 */
FRESULT f_close     (FIL* fp);                                   /* 关闭文件（自动同步） */
FRESULT f_read      (FIL* fp, void* buff, UINT btr, UINT* br);  /* 从文件读取数据 */
FRESULT f_write     (FIL* fp, const void* buff, UINT btw, UINT* bw); /* 向文件写入数据 */
FRESULT f_lseek     (FIL* fp, DWORD ofs);                        /* 移动文件读写指针 */
FRESULT f_truncate  (FIL* fp);                                   /* 截断文件到当前指针位置 */
FRESULT f_sync      (FIL* fp);                                   /* 同步文件缓存到磁盘 */
FRESULT f_opendir   (DIR* dp, const TCHAR* path);                /* 打开根目录 */
FRESULT f_closedir  (DIR* dp);                                   /* 关闭目录 */
FRESULT f_readdir   (DIR* dp, FILINFO* fno);                     /* 读取下一个目录项 */
FRESULT f_stat      (const TCHAR* path, FILINFO* fno);           /* 获取文件/目录信息 */
FRESULT f_getfree   (const TCHAR* path, DWORD* nclst, FATFS** fatfs); /* 获取剩余空间扇区数 */
FRESULT f_mkdir     (const TCHAR* path);                         /* 创建目录（未实现） */
FRESULT f_unlink    (const TCHAR* path);                         /* 删除文件 */
FRESULT f_rename    (const TCHAR* old, const TCHAR* newpath);    /* 重命名/移动文件 */
FRESULT f_mkfs      (const TCHAR* path, BYTE opt, DWORD au, void* work, UINT len); /* 格式化（存根） */
FRESULT f_chdir     (const TCHAR* path);                         /* 改变当前目录（存根） */
FRESULT f_getcwd    (TCHAR* buff, UINT len);                     /* 获取当前目录路径 */

#ifdef __cplusplus
}
#endif

#endif

