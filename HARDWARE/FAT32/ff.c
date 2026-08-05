/*
 * FatFs core module (compact edition)
 * ???????????????????? FatFs??????????????????
 * ????????????
 * 1. ???? / ?? / ??§Õ / ???
 * 2. ???????????????????????????
 * 3. 8.3 ???????
 */

/*-------------------------------------------------------------------*/
/*  FatFs - FAT Filesystem Module  R0.15  (Compact FAT32 edn.)      */
/*-------------------------------------------------------------------*/
#include "ff.h"
#include "diskio.h"
#include "ffconf.h"
#include <string.h>
#include <stdio.h>

#ifndef DEBUG_FATFS
#define DEBUG_FATFS 1
#endif

#if DEBUG_FATFS
#define FF_DEBUG_PRINT(...)    printf(__VA_ARGS__)
#else
#define FF_DEBUG_PRINT(...)    ((void)0)
#endif

/* FAT????????ID?????? */
static BYTE   Fsid = 0;
/* ????????FATFS???????????????? */
static FATFS *FatFs = 0;

/* ??????I/O???????? */
#define M_READ(pdrv, buf, sect, cnt)    disk_read(pdrv, buf, sect, cnt)
#define M_WRITE(pdrv, buf, sect, cnt)   disk_write(pdrv, buf, sect, cnt)
#define M_SYNC(pdrv)                    disk_ioctl(pdrv, CTRL_SYNC, 0)

/* FAT?????¦¶???? */
#define CLUST_MIN       2UL             /* ??§¹?????? */
#define CLUST_EOF       0x0FFFFFFFUL    /* EOF/???????? */

/* ??????????? */
#define FS_FAT12        1
#define FS_FAT16        2
#define FS_FAT32        3

/* ?????? */
#define DIR_ENTRY_SIZE  32U             /* ???????32??? */
#define DIR_PER_SECTOR  16U             /* ?????16?????? */
#define ATTR_LFN        0x0F            /* ????????????????? */

/**
 * DIR_ENTRY_INFO - ????¦Ë?????????
 * sector: ??????????????
 * offset: ????????????????
 * index : ?????????§Ö???????
 * dir   : ???????????????¦Ë??fs->win[]?§µ?
 */
typedef struct
{
    DWORD sector;
    WORD  offset;
    WORD  index;
    BYTE *dir;
} DIR_ENTRY_INFO;

/**
 * move_window - ??FATFS????????????????????????????????§Õ?????????
 * fs   : FATFS???????
 * sect : ?????????
 * ????: 0=???, 1=????I/O????
 */
/**
 * move_window - ??FATFS????????????????
 * ?????????
 *   1. ???????????????????????????
 *   2. ????????????????????§Õ??????
 *   3. ?????????????????????????
 * fs   : FATFS???????
 * sect : ?????????
 * ????: 0=???, 1=????I/O????
 */
static BYTE move_window(FATFS *fs, DWORD sect)
{
    /* ????????????????????????? */
    if (sect == fs->winsect)
        return 0;
    /* ??????????????????????????????? */
    if (fs->wflag)
    {
        if (M_WRITE(fs->pdrv, fs->win, fs->winsect, 1))
        {
            FF_DEBUG_PRINT("[FF] move_window flush fail: pdrv=%u target=%lu winsect=%lu\r\n",
                           fs->pdrv, sect, fs->winsect);
            return 1;
        }
        fs->wflag = 0;
    }
    /* ???????????????????????????? */
    if (M_READ(fs->pdrv, fs->win, sect, 1))
    {
        FF_DEBUG_PRINT("[FF] move_window read fail: pdrv=%u sect=%lu\r\n", fs->pdrv, sect);
        return 1;
    }
    fs->winsect = sect;
    return 0;
}

/**
 * get_fat - ???FAT??????????????FAT???
 * fs: FATFS???????
 * c : ???
 * ????: ??????FAT?????????????/EOF/???§Ò???
 */
static DWORD get_fat(FATFS *fs, DWORD c)
{
    DWORD val;
    DWORD sect;

    if (fs->fs_type == FS_FAT32)
    {
        sect = fs->fatbase + (c / 128UL);
        if (move_window(fs, sect))
            return CLUST_EOF;
        val = ((DWORD *)fs->win)[c & 127UL] & 0x0FFFFFFFUL;
    }
    else if (fs->fs_type == FS_FAT16)
    {
        sect = fs->fatbase + (c / 256UL);
        if (move_window(fs, sect))
            return CLUST_EOF;
        val = ((WORD *)fs->win)[c & 255UL];
    }
    else
    {
        sect = fs->fatbase + ((c * 3UL) / 2UL) / 512UL;
        if (move_window(fs, sect))
            return CLUST_EOF;
        val = fs->win[((c * 3UL) / 2UL) & 511UL];
        if (c & 1UL)
            val >>= 4;
        else
            val &= 0x0FFFUL;
    }

    if (val < CLUST_MIN)
        return 0;
    if (val >= CLUST_EOF)
        return CLUST_EOF;
    return val;
}

/**
 * put_fat - ??FAT????§Õ?????????FAT???
 * fs: FATFS???????
 * c : ???
 * v : ?§Õ???FAT?????????????/EOF????
 * ????: 0=???, 1=????I/O????
 */
static BYTE put_fat(FATFS *fs, DWORD c, DWORD v)
{
    DWORD sect;

    v &= 0x0FFFFFFFUL;
    if (fs->fs_type == FS_FAT32)
    {
        sect = fs->fatbase + (c / 128UL);
        if (move_window(fs, sect))
            return 1;
        ((DWORD *)fs->win)[c & 127UL] = (((DWORD *)fs->win)[c & 127UL] & 0xF0000000UL) | v;
    }
    else if (fs->fs_type == FS_FAT16)
    {
        sect = fs->fatbase + (c / 256UL);
        if (move_window(fs, sect))
            return 1;
        ((WORD *)fs->win)[c & 255UL] = (WORD)v;
    }
    else
    {
        sect = fs->fatbase + ((c * 3UL) / 2UL) / 512UL;
        if (move_window(fs, sect))
            return 1;
        if (c & 1UL)
            fs->win[((c * 3UL) / 2UL) & 511UL] = (BYTE)((fs->win[((c * 3UL) / 2UL) & 511UL] & 0x0F) | ((BYTE)(v << 4)));
        else
            fs->win[((c * 3UL) / 2UL) & 511UL] = (BYTE)v;
    }

    fs->wflag = 1;
    return 0;
}

/**
 * clst2sect - ?????????????????????????????
 * fs: FATFS???????
 * c : ???
 * ????: ?????????????????????
 */
static DWORD clst2sect(FATFS *fs, DWORD c)
{
    return fs->database + (c - CLUST_MIN) * fs->csize;
}

/**
 * get_root_start_sector - ????????????????
 * fs: FATFS???????
 * ????: ???????????????LBA??
 */
static DWORD get_root_start_sector(FATFS *fs)
{
    if (fs->fs_type == FS_FAT32 && fs->dirbase >= CLUST_MIN)
        return clst2sect(fs, fs->dirbase);
    return fs->dirbase;
}

/**
 * get_root_sector_count - ???????????????????
 * fs: FATFS???????
 * ????: ??????????????
 */
static UINT get_root_sector_count(FATFS *fs)
{
    if (fs->fs_type == FS_FAT32)
        return fs->csize;
    return (UINT)((fs->n_rootdir + (DIR_PER_SECTOR - 1U)) / DIR_PER_SECTOR);
}

/**
 * chk_chr_sfn - ???????????????8.3?????????§¹???
 * ch : ?????????
 * ????: 1=???, 0=???
 */
static BYTE chk_chr_sfn(char ch)
{
    if (ch >= 'a' && ch <= 'z')
        return 1;
    if (ch >= 'A' && ch <= 'Z')
        return 1;
    if (ch >= '0' && ch <= '9')
        return 1;

    switch (ch)
    {
    case '$':
    case '%':
    case '\'':
    case '-':
    case '_':
    case '@':
    case '~':
    case '`':
    case '!':
    case '(':
    case ')':
    case '{':
    case '}':
    case '^':
    case '#':
    case '&':
        return 1;
    default:
        return 0;
    }
}

/**
 * ff_wtoupper - ??§³§Õ?????????§Õ??????8.3??????????????
 * ch: ????????
 * ????: ??§Õ??????????????????
 */
static char ff_wtoupper(char ch)
{
    if (ch >= 'a' && ch <= 'z')
        return (char)(ch - 'a' + 'A');
    return ch;
}

/**
 * create_name - ??¡¤???????????????????????8.3??????????
 * path: ???¡¤???????
 * sfn : ???????????12????8+3+??¦Â\0??
 * ????: FR_OK=???, FR_INVALID_NAME=??????????§¹
 */
static FRESULT create_name(const TCHAR *path, char sfn[12])
{
    char namePart[8];
    char extPart[3];
    const char *baseName;
    UINT nameLen;
    UINT extLen;
    UINT i;

    if (path == 0 || sfn == 0)
        return FR_INVALID_NAME;

    baseName = path;
    while (*path != 0)
    {
        if (*path == '/' || *path == '\\')
            baseName = path + 1;
        path++;
    }

    if (*baseName == 0)
        return FR_INVALID_NAME;

    memset(namePart, ' ', sizeof(namePart));
    memset(extPart, ' ', sizeof(extPart));

    nameLen = 0;
    extLen = 0;
    while (*baseName != 0 && *baseName != '.')
    {
        if (nameLen >= 8U || chk_chr_sfn(*baseName) == 0)
            return FR_INVALID_NAME;
        namePart[nameLen++] = ff_wtoupper(*baseName);
        baseName++;
    }

    if (nameLen == 0U)
        return FR_INVALID_NAME;

    if (*baseName == '.')
    {
        baseName++;
        while (*baseName != 0)
        {
            if (extLen >= 3U || chk_chr_sfn(*baseName) == 0)
                return FR_INVALID_NAME;
            extPart[extLen++] = ff_wtoupper(*baseName);
            baseName++;
        }
    }

    for (i = 0; i < 8U; i++)
        sfn[i] = namePart[i];
    for (i = 0; i < 3U; i++)
        sfn[8U + i] = extPart[i];
    sfn[11] = 0;

    return FR_OK;
}

static void get_sfn(const BYTE sfn[11], char fileName[13])
{
    UINT i;
    UINT pos;
    BYTE hasExt;

    pos = 0;
    hasExt = 0;

    for (i = 0; i < 8U; i++)
    {
        if (sfn[i] == ' ')
            break;
        fileName[pos++] = (char)sfn[i];
    }

    for (i = 8U; i < 11U; i++)
    {
        if (sfn[i] != ' ')
        {
            hasExt = 1;
            break;
        }
    }

    if (hasExt != 0U)
    {
        fileName[pos++] = '.';
        for (i = 8U; i < 11U; i++)
        {
            if (sfn[i] == ' ')
                break;
            fileName[pos++] = (char)sfn[i];
        }
    }

    fileName[pos] = 0;
}

static FRESULT dir_sdi(FATFS *fs, WORD index, DIR_ENTRY_INFO *dirent)
{
    DWORD dirClust;
    DWORD startSector;
    DWORD nextClust;
    UINT sectorCount;
    UINT currentIndex;
    UINT sec;
    UINT ent;

    if (fs == 0 || dirent == 0)
        return FR_INVALID_OBJECT;

    currentIndex = 0;
    if (fs->fs_type == FS_FAT32)
    {
        dirClust = fs->dirbase;
        while (dirClust >= CLUST_MIN && dirClust < CLUST_EOF)
        {
            startSector = clst2sect(fs, dirClust);
            sectorCount = fs->csize;
            for (sec = 0; sec < sectorCount; sec++)
            {
                if (move_window(fs, startSector + sec))
                    return FR_DISK_ERR;
                for (ent = 0; ent < DIR_PER_SECTOR; ent++)
                {
                    if (currentIndex == index)
                    {
                        dirent->sector = startSector + sec;
                        dirent->offset = (WORD)(ent * DIR_ENTRY_SIZE);
                        dirent->index = index;
                        dirent->dir = &fs->win[dirent->offset];
                        return FR_OK;
                    }
                    currentIndex++;
                }
            }
            nextClust = get_fat(fs, dirClust);
            if (nextClust == dirClust || nextClust >= CLUST_EOF)
                break;
            dirClust = nextClust;
        }
    }
    else
    {
        startSector = get_root_start_sector(fs);
        sectorCount = get_root_sector_count(fs);
        for (sec = 0; sec < sectorCount; sec++)
        {
            if (move_window(fs, startSector + sec))
                return FR_DISK_ERR;
            for (ent = 0; ent < DIR_PER_SECTOR; ent++)
            {
                if (currentIndex == index)
                {
                    dirent->sector = startSector + sec;
                    dirent->offset = (WORD)(ent * DIR_ENTRY_SIZE);
                    dirent->index = index;
                    dirent->dir = &fs->win[dirent->offset];
                    return FR_OK;
                }
                currentIndex++;
            }
        }
    }

    return FR_DENIED;
}

static FRESULT dir_find(FATFS *fs, const char sfn[11], DIR_ENTRY_INFO *dirent)
{
    DIR_ENTRY_INFO cur;
    FRESULT res;
    WORD index;

    if (fs == 0 || sfn == 0 || dirent == 0)
        return FR_INVALID_OBJECT;

    index = 0;
    for (;;)
    {
        res = dir_sdi(fs, index, &cur);
        if (res != FR_OK)
            return res;
        if (cur.dir[0] == 0x00)
            return FR_NO_FILE;
        if (cur.dir[0] != 0xE5 && cur.dir[11] != ATTR_LFN)
        {
            if (memcmp(cur.dir, sfn, 11) == 0)
            {
                *dirent = cur;
                return FR_OK;
            }
        }
        index++;
    }
}

static FRESULT dir_alloc(FATFS *fs, DIR_ENTRY_INFO *dirent)
{
    DIR_ENTRY_INFO cur;
    FRESULT res;
    WORD index;

    if (fs == 0 || dirent == 0)
        return FR_INVALID_OBJECT;

    index = 0;
    for (;;)
    {
        res = dir_sdi(fs, index, &cur);
        if (res != FR_OK)
            return res;
        if (cur.dir[0] == 0x00 || cur.dir[0] == 0xE5)
        {
            *dirent = cur;
            return FR_OK;
        }
        index++;
    }
}

static DWORD create_chain(FATFS *fs)
{
    DWORD c;

    for (c = CLUST_MIN; c < fs->n_fatent; c++)
    {
        if (get_fat(fs, c) == 0)
        {
            if (put_fat(fs, c, CLUST_EOF) != 0)
                return 0;
            return c;
        }
    }

    return 0;
}

static FRESULT remove_chain(FATFS *fs, DWORD startClust)
{
    DWORD clust;
    DWORD nextClust;

    if (fs == 0)
        return FR_INVALID_OBJECT;
    if (startClust < CLUST_MIN)
        return FR_OK;

    clust = startClust;
    while (clust >= CLUST_MIN && clust < CLUST_EOF)
    {
        nextClust = get_fat(fs, clust);
        if (put_fat(fs, clust, 0) != 0)
            return FR_DISK_ERR;
        if (nextClust == clust)
            break;
        clust = nextClust;
    }

    return FR_OK;
}

static void get_fileinfo(BYTE *dir, FILINFO *fno)
{
    if (fno == 0 || dir == 0)
        return;

    memset(fno, 0, sizeof(FILINFO));
    fno->fattrib = dir[11];
    fno->fsize = *(DWORD *)&dir[28];
    get_sfn(dir, fno->fname);
}

static BYTE is_valid_bpb(const BYTE *sector)
{
    WORD bytesPerSector;
    BYTE sectorsPerCluster;
    WORD reservedSectors;
    BYTE numberOfFats;

    if (sector == 0)
        return 0;

    if (sector[510] != 0x55 || sector[511] != 0xAA)
        return 0;

    bytesPerSector = *(WORD *)&sector[0x0B];
    sectorsPerCluster = sector[0x0D];
    reservedSectors = *(WORD *)&sector[0x0E];
    numberOfFats = sector[0x10];

    if (bytesPerSector != 512U)
        return 0;
    if (sectorsPerCluster == 0U)
        return 0;
    if ((sectorsPerCluster & (sectorsPerCluster - 1U)) != 0U)
        return 0;
    if (reservedSectors == 0U)
        return 0;
    if (numberOfFats == 0U)
        return 0;

    return 1;
}

/**
 * f_mount - ????FAT??????????DBR??????????BPB??????
 * ?????????
 *   1. ???0????(MBR/DBR)???????????
 *   2. ??????????0x55AA
 *   3. ????BPB???????????????/??????????/FAT????/????????
 *   4. ?????????0?§Ø??FAT32???????FAT16
 *   5. ????FAT??¦Ë?¨¢???????¦Ë?¨¢?????¦Ë??
 *   6. ???????FatFs??????????
 * fs   : FATFS?????????????????—¥
 * path : ????¡¤?????????¦Ä????
 * opt  : ????????????¦Ä????
 * ????: FR_OK=???, FR_NO_FILESYSTEM=????§¹?????
 */
FRESULT f_mount(FATFS *fs, const TCHAR *path, BYTE opt)
{
    BYTE *s;        /* DBR??????????? */
    DWORD bs;       /* ????????????BPB???0x0E?? */
    DWORD fatsz;    /* ???FAT???????????? */
    DWORD totsec;   /* ???????????? */
    DWORD volbase;
    DWORD partbase;
    DWORD rootdir_sectors;
    UINT i;

    (void)path;
    (void)opt;

    /* §µ???????§¹?? */
    if (!fs)
        return FR_INVALID_OBJECT;
    volbase = 0;

    /* ???0????????????DBR?????????MBR?? */
    if (M_READ(0, fs->win, 0, 1))
        return FR_NOT_READY;

    s = fs->win;
    if (s[510] != 0x55 || s[511] != 0xAA)
        return FR_NO_FILESYSTEM;

    /* ??0??????????§¹BPB???????MBR???????????????DBR¦Ë?? */
    if (is_valid_bpb(s) == 0U)
    {
        for (i = 0; i < 4U; i++)
        {
            BYTE *pte = &s[0x1BE + i * 16U];

            if (pte[4] == 0x00U)
                continue;

            partbase = *(DWORD *)&pte[8];
            if (partbase == 0U)
                continue;

            if (M_READ(0, fs->win, partbase, 1))
                return FR_NOT_READY;

            s = fs->win;
            if (is_valid_bpb(s) != 0U)
            {
                volbase = partbase;
                break;
            }
        }

        if (is_valid_bpb(s) == 0U)
            return FR_NO_FILESYSTEM;
    }

    /* ????BPB??BIOS?????ï‚ */
    bs = *(WORD *)&s[0x0E];          /* ?????????????????32?? */
    fs->csize = s[0x0D];            /* ????????? */
    fs->n_fats = s[0x10];           /* FAT??????????2??? */
    fs->n_rootdir = *(WORD *)&s[0x11]; /* ????????????? */

    /* ???????0 ?? FAT32?????? ?? FAT16 */
    if (fs->n_rootdir == 0)
    {
        fs->fs_type = FS_FAT32;
        fatsz = *(DWORD *)&s[0x24];     /* FAT32?FAT?????? */
        fs->dirbase = *(DWORD *)&s[0x2C]; /* FAT32??????? */
    }
    else
    {
        fatsz = *(WORD *)&s[0x16];      /* FAT16?FAT?????? */
        if (fatsz == 0)
            fatsz = *(DWORD *)&s[0x24];
        fs->fs_type = FS_FAT16;
    }

    totsec = *(WORD *)&s[0x13];
    if (totsec == 0U)
        totsec = *(DWORD *)&s[0x20];

    rootdir_sectors = (DWORD)(((DWORD)fs->n_rootdir * 32UL + 511UL) / 512UL);

    /* ????????????? */
    fs->fatbase = volbase + bs;                           /* FAT????????? */
    fs->database = volbase + bs + fs->n_fats * fatsz +    /* ????????? = ??????? + ???? + FAT?? */
                   (fs->fs_type == FS_FAT32 ? 0U : rootdir_sectors);  /* FAT32?????????? */
    if (fs->fs_type != FS_FAT32)
        fs->dirbase = volbase + bs + fs->n_fats * fatsz;  /* FAT12/16??????????? */
    fs->n_fatent = ((totsec - (bs + fs->n_fats * fatsz + rootdir_sectors)) / fs->csize) + 2U; /* FAT?????? */
    fs->pdrv = 0;           /* ???????????? */
    fs->winsect = 0xFFFFFFFFUL;  /* ?????¦Æ??????? */
    fs->wflag = 0;
    fs->id = ++Fsid;        /* ?????¦Ì????ID */
    FatFs = fs;             /* ????????????????? */
    return FR_OK;
}

FRESULT f_open(FATFS *fs, FIL *fp, const TCHAR *path, BYTE mode)
{
    DIR_ENTRY_INFO dirent;
    FRESULT res;
    char sfn[12];
    DWORD startClust;

    if (!fs || !fp)
        return FR_INVALID_OBJECT;

    res = create_name(path, sfn);
    if (res != FR_OK)
        return res;

    res = dir_find(fs, sfn, &dirent);
    if (res == FR_OK)
    {
        if (mode & FA_CREATE_NEW)
            return FR_EXIST;

        if (mode & FA_CREATE_ALWAYS)
        {
            startClust = ((DWORD)*(WORD *)&dirent.dir[20] << 16) | *(WORD *)&dirent.dir[26];
            res = remove_chain(fs, startClust);
            if (res != FR_OK)
                return res;
            *(WORD *)&dirent.dir[20] = 0;
            *(WORD *)&dirent.dir[26] = 0;
            *(DWORD *)&dirent.dir[28] = 0;
            fs->wflag = 1;
        }

        fp->fs = fs;
        fp->id = fs->id;
        fp->flag = mode;
        fp->err = 0;
        fp->fptr = 0;
        fp->fsize = *(DWORD *)&dirent.dir[28];
        fp->sclust = ((DWORD)*(WORD *)&dirent.dir[20] << 16) | *(WORD *)&dirent.dir[26];
        fp->clust = fp->sclust;
        fp->dsect = 0;
        fp->dir_sect = dirent.sector;
        fp->dir_ptr = dirent.dir;
        return FR_OK;
    }

    if (res != FR_NO_FILE)
        return res;
    if ((mode & (FA_CREATE_ALWAYS | FA_CREATE_NEW | FA_OPEN_ALWAYS)) == 0)
        return FR_NO_FILE;

    res = dir_alloc(fs, &dirent);
    if (res != FR_OK)
        return res;

    memset(dirent.dir, 0, DIR_ENTRY_SIZE);
    memcpy(dirent.dir, sfn, 11);
    dirent.dir[11] = AM_ARC;
    *(WORD *)&dirent.dir[20] = 0;
    *(WORD *)&dirent.dir[26] = 0;
    *(DWORD *)&dirent.dir[28] = 0;
    fs->wflag = 1;

    fp->fs = fs;
    fp->id = fs->id;
    fp->flag = mode;
    fp->err = 0;
    fp->fptr = 0;
    fp->fsize = 0;
    fp->sclust = 0;
    fp->clust = 0;
    fp->dsect = 0;
    fp->dir_sect = dirent.sector;
    fp->dir_ptr = dirent.dir;
    return FR_OK;
}

FRESULT f_read(FIL *fp, void *buff, UINT btr, UINT *br)
{
    BYTE *p;
    UINT rb;
    UINT off;
    UINT cnt;
    DWORD sect;

    if (br)
        *br = 0;
    if (!fp || !fp->fs)
        return FR_INVALID_OBJECT;

    p = (BYTE *)buff;
    rb = 0;
    while (btr > 0U && fp->fptr < fp->fsize)
    {
        if (fp->dsect == 0)
        {
            if (fp->fptr == 0)
                fp->clust = fp->sclust;
            else
                fp->clust = get_fat(fp->fs, fp->clust);

            if (fp->clust < CLUST_MIN || fp->clust >= CLUST_EOF)
                break;

            fp->dsect = clst2sect(fp->fs, fp->clust);
        }

        sect = fp->dsect;
        off = (UINT)(fp->fptr & 511UL);
        cnt = 512U - off;
        if (cnt > btr)
            cnt = btr;
        if (cnt > (UINT)(fp->fsize - fp->fptr))
            cnt = (UINT)(fp->fsize - fp->fptr);

        if (move_window(fp->fs, sect))
            return FR_DISK_ERR;

        memcpy(p, fp->fs->win + off, cnt);
        p += cnt;
        fp->fptr += cnt;
        rb += cnt;
        btr -= cnt;

        if ((fp->fptr & 511UL) == 0UL)
        {
            fp->dsect++;
            if (((fp->fptr / 512UL) % fp->fs->csize) == 0UL)
                fp->dsect = 0;
        }
    }

    if (br)
        *br = rb;
    return FR_OK;
}

FRESULT f_write(FIL *fp, const void *buff, UINT btw, UINT *bw)
{
    const BYTE *p;
    UINT wb;
    UINT off;
    UINT cnt;
    DWORD currClust;
    DWORD newClust;
    DWORD sect;

    if (bw)
        *bw = 0;
    if (!fp || !fp->fs)
        return FR_INVALID_OBJECT;
    if ((fp->flag & FA_WRITE) == 0)
        return FR_DENIED;

    p = (const BYTE *)buff;
    wb = 0;
    while (btw > 0U)
    {
        if (fp->dsect == 0)
        {
            if (fp->fptr == 0)
            {
                currClust = fp->sclust;
            }
            else
            {
                currClust = get_fat(fp->fs, fp->clust);
            }

            if (currClust < CLUST_MIN || currClust >= CLUST_EOF)
            {
                newClust = create_chain(fp->fs);
                if (newClust < CLUST_MIN)
                {
                    FF_DEBUG_PRINT("[FF] f_write create_chain failed: fptr=%lu fsize=%lu n_fatent=%lu\r\n",
                                   fp->fptr, fp->fsize, fp->fs->n_fatent);
                    if (bw)
                        *bw = wb;
                    return FR_DENIED;
                }

                if (fp->sclust == 0)
                {
                    fp->sclust = newClust;
                    /*
                     * ?????????????????§Ö???????????????????
                     * ?????? fs->win ?????????? move_window() ?§Ý????? FAT ??????
                     * ?????? fp->dir_ptr §Õ???? FAT ????????????????????
                     * ?????§Ö?????????????§³???? f_sync()/f_close() ???????
                     */
                }
                else if (fp->clust >= CLUST_MIN && fp->clust < CLUST_EOF)
                {
                    if (put_fat(fp->fs, fp->clust, newClust) != 0)
                    {
                        FF_DEBUG_PRINT("[FF] f_write put_fat link fail: prev=%lu new=%lu\r\n",
                                       fp->clust, newClust);
                        return FR_DISK_ERR;
                    }
                }

                currClust = newClust;
            }

            fp->clust = currClust;
            fp->dsect = clst2sect(fp->fs, fp->clust);
        }

        sect = fp->dsect;
        off = (UINT)(fp->fptr & 511UL);
        cnt = 512U - off;
        if (cnt > btw)
            cnt = btw;

        if (move_window(fp->fs, sect))
        {
            FF_DEBUG_PRINT("[FF] f_write move_window fail: sect=%lu off=%u btw=%u wb=%u\r\n",
                           sect, off, btw, wb);
            return FR_DISK_ERR;
        }

        memcpy(fp->fs->win + off, p, cnt);
        fp->fs->wflag = 1;
        p += cnt;
        fp->fptr += cnt;
        wb += cnt;
        btw -= cnt;
        if (fp->fptr > fp->fsize)
            fp->fsize = fp->fptr;

        if ((fp->fptr & 511UL) == 0UL)
        {
            fp->dsect++;
            if (((fp->fptr / 512UL) % fp->fs->csize) == 0UL)
                fp->dsect = 0;
        }
    }

    if (bw)
        *bw = wb;
    return FR_OK;
}

FRESULT f_sync(FIL *fp)
{
    if (!fp || !fp->fs)
        return FR_INVALID_OBJECT;

    if (fp->dir_ptr)
    {
        if (move_window(fp->fs, fp->dir_sect))
        {
            FF_DEBUG_PRINT("[FF] f_sync move_window(dir) fail: dir_sect=%lu fsize=%lu sclust=%lu\r\n",
                           fp->dir_sect, fp->fsize, fp->sclust);
            return FR_DISK_ERR;
        }
        *(DWORD *)&fp->dir_ptr[28] = fp->fsize;
        *(WORD *)&fp->dir_ptr[20] = (WORD)(fp->sclust >> 16);
        *(WORD *)&fp->dir_ptr[26] = (WORD)fp->sclust;
        fp->fs->wflag = 1;
    }

    if (fp->fs->wflag)
    {
        if (M_WRITE(fp->fs->pdrv, fp->fs->win, fp->fs->winsect, 1))
        {
            FF_DEBUG_PRINT("[FF] f_sync final flush fail: pdrv=%u winsect=%lu fsize=%lu sclust=%lu\r\n",
                           fp->fs->pdrv, fp->fs->winsect, fp->fsize, fp->sclust);
            return FR_DISK_ERR;
        }
        fp->fs->wflag = 0;
    }

    if (M_SYNC(fp->fs->pdrv) != RES_OK)
    {
        FF_DEBUG_PRINT("[FF] f_sync CTRL_SYNC fail: pdrv=%u\r\n", fp->fs->pdrv);
        return FR_DISK_ERR;
    }

    return FR_OK;
}

FRESULT f_close(FIL *fp)
{
    FRESULT res;

    if (!fp)
        return FR_INVALID_OBJECT;
    if (fp->fs != 0)
    {
        res = f_sync(fp);
        if (res != FR_OK)
            return res;
    }
    memset(fp, 0, sizeof(FIL));
    return FR_OK;
}

FRESULT f_lseek(FIL *fp, DWORD ofs)
{
    if (!fp)
        return FR_INVALID_OBJECT;
    if (ofs > fp->fsize)
        ofs = fp->fsize;
    fp->fptr = ofs;
    fp->clust = fp->sclust;
    fp->dsect = 0;
    return FR_OK;
}

FRESULT f_opendir(DIR *dp, const TCHAR *path)
{
    if (!dp)
        return FR_INVALID_OBJECT;
    if (FatFs == 0)
        return FR_NOT_ENABLED;
    if (path && path[0] != 0 && !(path[0] == '/' && path[1] == 0))
        return FR_NO_PATH;

    memset(dp, 0, sizeof(DIR));
    dp->fs = FatFs;
    dp->id = FatFs->id;
    dp->sclust = FatFs->dirbase;
    dp->clust = dp->sclust;
    return FR_OK;
}

FRESULT f_readdir(DIR *dp, FILINFO *fno)
{
    DIR_ENTRY_INFO dirent;
    FRESULT res;

    if (!dp || !fno || !dp->fs)
        return FR_INVALID_OBJECT;

    memset(fno, 0, sizeof(FILINFO));
    for (;;)
    {
        res = dir_sdi(dp->fs, dp->index, &dirent);
        if (res != FR_OK)
            return res;

        if (dirent.dir[0] == 0x00)
        {
            fno->fname[0] = 0;
            return FR_OK;
        }

        dp->index++;
        if (dirent.dir[0] == 0xE5 || dirent.dir[11] == ATTR_LFN)
            continue;

        get_fileinfo(dirent.dir, fno);
        return FR_OK;
    }
}

FRESULT f_stat(const TCHAR *path, FILINFO *fno)
{
    DIR_ENTRY_INFO dirent;
    FRESULT res;
    char sfn[12];

    if (FatFs == 0)
        return FR_NOT_ENABLED;
    if (fno == 0)
        return FR_INVALID_OBJECT;

    res = create_name(path, sfn);
    if (res != FR_OK)
        return res;

    res = dir_find(FatFs, sfn, &dirent);
    if (res != FR_OK)
        return res;

    get_fileinfo(dirent.dir, fno);
    return FR_OK;
}

FRESULT f_truncate(FIL *fp)
{
    FRESULT res;

    if (!fp || !fp->fs)
        return FR_INVALID_OBJECT;

    res = remove_chain(fp->fs, fp->sclust);
    if (res != FR_OK)
        return res;

    fp->sclust = 0;
    fp->clust = 0;
    fp->dsect = 0;
    fp->fsize = fp->fptr;
    return f_sync(fp);
}

FRESULT f_closedir(DIR *dp)
{
    if (!dp)
        return FR_INVALID_OBJECT;
    memset(dp, 0, sizeof(DIR));
    return FR_OK;
}

FRESULT f_getfree(const TCHAR *p, DWORD *c, FATFS **f)
{
    (void)p;
    if (c)
        *c = 0;
    if (f)
        *f = FatFs;
    return FR_OK;
}

FRESULT f_mkdir(const TCHAR *p)
{
    (void)p;
    return FR_DENIED;
}

FRESULT f_unlink(const TCHAR *path)
{
    DIR_ENTRY_INFO dirent;
    FRESULT res;
    char sfn[12];
    DWORD startClust;

    if (FatFs == 0)
        return FR_NOT_ENABLED;

    res = create_name(path, sfn);
    if (res != FR_OK)
        return res;

    res = dir_find(FatFs, sfn, &dirent);
    if (res != FR_OK)
        return res;

    startClust = ((DWORD)*(WORD *)&dirent.dir[20] << 16) | *(WORD *)&dirent.dir[26];
    res = remove_chain(FatFs, startClust);
    if (res != FR_OK)
        return res;

    dirent.dir[0] = 0xE5;
    FatFs->wflag = 1;
    if (M_WRITE(FatFs->pdrv, FatFs->win, dirent.sector, 1))
        return FR_DISK_ERR;
    FatFs->wflag = 0;
    FatFs->winsect = dirent.sector;
    if (M_SYNC(FatFs->pdrv) != RES_OK)
        return FR_DISK_ERR;
    return FR_OK;
}

FRESULT f_rename(const TCHAR *old, const TCHAR *newpath)
{
    DIR_ENTRY_INFO oldDir;
    DIR_ENTRY_INFO newDir;
    FRESULT res;
    char oldSfn[12];
    char newSfn[12];

    if (FatFs == 0)
        return FR_NOT_ENABLED;

    res = create_name(old, oldSfn);
    if (res != FR_OK)
        return res;
    res = create_name(newpath, newSfn);
    if (res != FR_OK)
        return res;

    res = dir_find(FatFs, newSfn, &newDir);
    if (res == FR_OK)
        return FR_EXIST;
    if (res != FR_NO_FILE)
        return res;

    res = dir_find(FatFs, oldSfn, &oldDir);
    if (res != FR_OK)
        return res;

    memcpy(oldDir.dir, newSfn, 11);
    FatFs->wflag = 1;
    if (M_WRITE(FatFs->pdrv, FatFs->win, oldDir.sector, 1))
        return FR_DISK_ERR;
    FatFs->wflag = 0;
    FatFs->winsect = oldDir.sector;
    if (M_SYNC(FatFs->pdrv) != RES_OK)
        return FR_DISK_ERR;
    return FR_OK;
}

FRESULT f_mkfs(const TCHAR *p, BYTE o, DWORD a, void *w, UINT l)
{
    (void)p;
    (void)o;
    (void)a;
    (void)w;
    (void)l;
    return FR_OK;
}

FRESULT f_chdir(const TCHAR *p)
{
    (void)p;
    return FR_OK;
}

FRESULT f_getcwd(TCHAR *b, UINT l)
{
    if (b == 0 || l < 2U)
        return FR_INVALID_PARAMETER;
    b[0] = '/';
    b[1] = 0;
    return FR_OK;
}
