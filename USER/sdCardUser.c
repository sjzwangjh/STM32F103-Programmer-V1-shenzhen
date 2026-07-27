/*
 * SD 卡用户层接口实现
 * 提供文件创建、重写、追加、读取、删除、改名以及调试示例。
 */

#include "sys.h"
#include "sdCardUser.h"
#include "diskio.h"
#include "sdcard.h"
#include <stdio.h>
#include <string.h>

/* SD 卡文件系统对象 */
static FATFS g_sdFatFs;
/* 挂载标志：0=未挂载，1=已挂载 */
static BYTE  g_sdMounted = 0;

/*
 * 确保文件系统已挂载。
 * 若开启速度扫描，则在挂载后对引导区、FAT 区和数据区做一次测试。
 * 默认仅保留测试日志，不改变最终工作时钟。
 */
static FRESULT SDCardUser_EnsureMounted(void)
{
    FRESULT res;
    u32 tuneSectors[3];
    u8 tuneCount;

    if (g_sdMounted != 0U)
        return FR_OK;

    if (disk_initialize(0) != RES_OK)
        return FR_NOT_READY;

    res = f_mount(&g_sdFatFs, "", 1);
    if (res != FR_OK)
        return res;

#if SD_ENABLE_SPEED_SCAN
    tuneCount = 0U;
    tuneSectors[tuneCount++] = 0U;
    tuneSectors[tuneCount++] = g_sdFatFs.fatbase;
    tuneSectors[tuneCount++] = g_sdFatFs.database;
    (void)SD_TuneBusSpeedForSectors(SDCard_Info.bus_width, tuneSectors, tuneCount);
#else
    (void)tuneSectors;
    (void)tuneCount;
#endif

    g_sdMounted = 1U;
    return FR_OK;
}

FRESULT SDCardUser_Init(void)
{
    return SDCardUser_EnsureMounted();
}

u16 SDCardUser_ListFiles(char fileList[][SDCARD_USER_MAX_NAME_LEN], u16 maxFiles)
{
    DIR dirObj;
    FILINFO fileInfo;
    FRESULT res;
    u16 fileCount;

    if (fileList == 0 || maxFiles == 0U)
        return 0;

    res = SDCardUser_EnsureMounted();
    if (res != FR_OK)
        return 0;

    res = f_opendir(&dirObj, "");
    if (res != FR_OK)
        return 0;

    fileCount = 0U;
    while (fileCount < maxFiles)
    {
        res = f_readdir(&dirObj, &fileInfo);
        if (res != FR_OK)
            break;
        if (fileInfo.fname[0] == 0)
            break;
        if ((fileInfo.fattrib & AM_DIR) != 0U)
            continue;

        strncpy(fileList[fileCount], fileInfo.fname, SDCARD_USER_MAX_NAME_LEN - 1U);
        fileList[fileCount][SDCARD_USER_MAX_NAME_LEN - 1U] = 0;
        fileCount++;
    }

    f_closedir(&dirObj);
    return fileCount;
}

FRESULT SDCardUser_CreateFile(const char *fileName)
{
    FIL fileObj;
    FRESULT res;

    res = SDCardUser_EnsureMounted();
    if (res != FR_OK)
        return res;

    res = f_open(&g_sdFatFs, &fileObj, fileName, FA_CREATE_NEW | FA_WRITE);
    if (res != FR_OK)
        return res;

    return f_close(&fileObj);
}

FRESULT SDCardUser_AppendFile(const char *fileName, const void *data, u32 dataLen, u32 *writtenLen)
{
    FIL fileObj;
    UINT bw;
    FRESULT res;

    if (writtenLen != 0)
        *writtenLen = 0U;
    if (data == 0 && dataLen != 0U)
        return FR_INVALID_PARAMETER;

    res = SDCardUser_EnsureMounted();
    if (res != FR_OK)
        return res;

    res = f_open(&g_sdFatFs, &fileObj, fileName, FA_OPEN_ALWAYS | FA_WRITE);
    if (res != FR_OK)
        return res;

    res = f_lseek(&fileObj, fileObj.fsize);
    if (res == FR_OK && dataLen != 0U)
    {
        bw = 0U;
        res = f_write(&fileObj, data, (UINT)dataLen, &bw);
        if (writtenLen != 0)
            *writtenLen = bw;
    }

    if (res == FR_OK)
        res = f_close(&fileObj);
    else
        (void)f_close(&fileObj);

    return res;
}

FRESULT SDCardUser_RewriteFile(const char *fileName, const void *data, u32 dataLen, u32 *writtenLen)
{
    FIL fileObj;
    UINT bw;
    FRESULT res;

    if (writtenLen != 0)
        *writtenLen = 0U;
    if (data == 0 && dataLen != 0U)
        return FR_INVALID_PARAMETER;

    res = SDCardUser_EnsureMounted();
    if (res != FR_OK)
        return res;

    /* 先删除再重建，避免历史簇链残留影响重写测试。 */
    res = f_unlink(fileName);
    if (res != FR_OK && res != FR_NO_FILE)
        return res;

    res = f_open(&g_sdFatFs, &fileObj, fileName, FA_CREATE_NEW | FA_WRITE);
    if (res != FR_OK)
        return res;

    bw = 0U;
    if (dataLen != 0U)
    {
        res = f_write(&fileObj, data, (UINT)dataLen, &bw);
        if (writtenLen != 0)
            *writtenLen = bw;
    }

    if (res == FR_OK)
        res = f_close(&fileObj);
    else
        (void)f_close(&fileObj);

    return res;
}

FRESULT SDCardUser_ReadFile(const char *fileName, void *dataBuf, u32 bufSize, u32 *readLen)
{
    FIL fileObj;
    UINT br;
    FRESULT res;

    if (readLen != 0)
        *readLen = 0U;
    if (dataBuf == 0)
        return FR_INVALID_PARAMETER;

    res = SDCardUser_EnsureMounted();
    if (res != FR_OK)
        return res;

    res = f_open(&g_sdFatFs, &fileObj, fileName, FA_READ | FA_OPEN_EXISTING);
    if (res != FR_OK)
        return res;

    if (bufSize > fileObj.fsize)
        bufSize = fileObj.fsize;

    br = 0U;
    res = f_read(&fileObj, dataBuf, (UINT)bufSize, &br);
    if (readLen != 0)
        *readLen = br;

    if (res == FR_OK)
        res = f_close(&fileObj);
    else
        (void)f_close(&fileObj);

    return res;
}

FRESULT SDCardUser_DeleteFile(const char *fileName)
{
    FRESULT res;

    res = SDCardUser_EnsureMounted();
    if (res != FR_OK)
        return res;

    return f_unlink(fileName);
}

FRESULT SDCardUser_RenameFile(const char *oldName, const char *newName)
{
    FRESULT res;

    res = SDCardUser_EnsureMounted();
    if (res != FR_OK)
        return res;

    return f_rename(oldName, newName);
}

static void SDCardUser_FillDebugPattern(u8 *buf, u32 len)
{
    static const u8 patternTable[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    u32 i;
    u32 patternLen;

    if (buf == 0)
        return;

    patternLen = (u32)(sizeof(patternTable) - 1U);
    for (i = 0U; i < len; i++)
        buf[i] = patternTable[i % patternLen];
}

static void SDCardUser_ReverseCopy(u8 *dst, const u8 *src, u32 len)
{
    u32 i;

    if (dst == 0 || src == 0)
        return;

    for (i = 0U; i < len; i++)
        dst[i] = src[len - 1U - i];
}

static void SDCardUser_PrintPreview(const char *tag, const u8 *buf, u32 len)
{
    u32 previewLen;
    u32 i;

    if (buf == 0)
        return;

    previewLen = (len < 24U) ? len : 24U;
    printf("%s长度=%ld, 前%ld字节=", tag, len, previewLen);
    for (i = 0U; i < previewLen; i++)
        printf("%c", buf[i]);
    printf("\r\n");
}

void SDCardUser_DebugDemo(void)
{
#define SDCARD_DEBUG_DATA_LEN   200U

    static const u8 appendData[] = "APPEND\r\n";
    char fileList[4][SDCARD_USER_MAX_NAME_LEN];
    u8 createData[SDCARD_DEBUG_DATA_LEN];
    u8 rewriteData[SDCARD_DEBUG_DATA_LEN];
    u8 readBuf[SDCARD_DEBUG_DATA_LEN + 32U];
    u32 rwLen;
    u32 readLen;
    u16 fileCount;
    u32 i;
    FRESULT res;

    SDCardUser_FillDebugPattern(createData, SDCARD_DEBUG_DATA_LEN);
    SDCardUser_ReverseCopy(rewriteData, createData, SDCARD_DEBUG_DATA_LEN);
    memset(readBuf, 0, sizeof(readBuf));
    fileCount = 0U;
    rwLen = 0U;
    readLen = 0U;

    /* 清理历史调试文件。 */
    (void)SDCardUser_DeleteFile("DEMO.TXT");
    (void)SDCardUser_DeleteFile("DEMO2.TXT");

    printf("【SD卡调试】开始 SD 卡文件操作调试...\r\n");
    res = SDCardUser_Init();
    if (res != FR_OK)
    {
        printf("【SD卡调试】初始化失败, 错误码=%d\r\n", res);
        return;
    }
    printf("【SD卡调试】初始化成功\r\n");

    fileCount = SDCardUser_ListFiles(fileList, 4);
    printf("【SD卡调试】列举文件: SD卡有%d个文件\r\n", fileCount);

    printf("【SD卡调试】创建文件 'DEMO.TXT' ...\r\n");
    res = SDCardUser_CreateFile("DEMO.TXT");
    if (res != FR_OK && res != FR_EXIST)
    {
        printf("【SD卡调试】创建文件失败, 错误码=%d\r\n", res);
        return;
    }
    printf("【SD卡调试】创建文件操作成功\r\n");

    SDCardUser_PrintPreview("【SD卡调试】步骤3写入数据预览: ", createData, SDCARD_DEBUG_DATA_LEN);
    printf("【SD卡调试】步骤3写入文件 'DEMO.TXT' 200字节...\r\n");
    res = SDCardUser_RewriteFile("DEMO.TXT", createData, SDCARD_DEBUG_DATA_LEN, &rwLen);
    if (res != FR_OK)
    {
        printf("【SD卡调试】步骤3写入失败, 错误码=%d, 期望=%d, 实际=%ld\r\n",
               res, SDCARD_DEBUG_DATA_LEN, rwLen);
        return;
    }
    printf("【SD卡调试】步骤3写入成功，实际写入%ld字节\r\n", rwLen);

    memset(readBuf, 0, sizeof(readBuf));
    readLen = 0U;
    res = SDCardUser_ReadFile("DEMO.TXT", readBuf, sizeof(readBuf), &readLen);
    if (res != FR_OK)
    {
        printf("【SD卡调试】步骤3读回失败, 错误码=%d\r\n", res);
        return;
    }
    printf("【SD卡调试】步骤3读回成功，读取%ld字节\r\n", readLen);
    SDCardUser_PrintPreview("【SD卡调试】步骤3读回数据预览: ", readBuf, readLen);
    if (readLen != SDCARD_DEBUG_DATA_LEN || memcmp(readBuf, createData, SDCARD_DEBUG_DATA_LEN) != 0)
    {
        for (i = 0U; i < readLen && i < SDCARD_DEBUG_DATA_LEN; i++)
        {
            if (readBuf[i] != createData[i])
            {
                printf("【SD卡调试】步骤3校验失败, 首个不匹配偏移=%ld, 读=%02X, 期望=%02X\r\n",
                       i, readBuf[i], createData[i]);
                return;
            }
        }
        printf("【SD卡调试】步骤3校验失败, 读取长度=%ld, 期望长度=%d\r\n",
               readLen, SDCARD_DEBUG_DATA_LEN);
        return;
    }
    printf("【SD卡调试】步骤3校验通过\r\n");

    SDCardUser_PrintPreview("【SD卡调试】步骤4重写数据预览: ", rewriteData, SDCARD_DEBUG_DATA_LEN);
    printf("【SD卡调试】重写文件 'DEMO.TXT' ...\r\n");
    res = SDCardUser_RewriteFile("DEMO.TXT", rewriteData, SDCARD_DEBUG_DATA_LEN, &rwLen);
    if (res != FR_OK)
    {
        printf("【SD卡调试】重写文件失败, 错误码=%d, 期望=%d, 实际=%ld\r\n",
               res, SDCARD_DEBUG_DATA_LEN, rwLen);
        return;
    }
    printf("【SD卡调试】重写文件操作成功，写入%ld字节\r\n", rwLen);

    memset(readBuf, 0, sizeof(readBuf));
    readLen = 0U;
    res = SDCardUser_ReadFile("DEMO.TXT", readBuf, sizeof(readBuf), &readLen);
    if (res != FR_OK)
    {
        printf("【SD卡调试】步骤4读回失败, 错误码=%d\r\n", res);
        return;
    }
    printf("【SD卡调试】步骤4读回成功，读取%ld字节\r\n", readLen);
    SDCardUser_PrintPreview("【SD卡调试】步骤4读回数据预览: ", readBuf, readLen);
    if (readLen != SDCARD_DEBUG_DATA_LEN || memcmp(readBuf, rewriteData, SDCARD_DEBUG_DATA_LEN) != 0)
    {
        for (i = 0U; i < readLen && i < SDCARD_DEBUG_DATA_LEN; i++)
        {
            if (readBuf[i] != rewriteData[i])
            {
                printf("【SD卡调试】步骤4校验失败, 首个不匹配偏移=%ld, 读=%02X, 期望=%02X\r\n",
                       i, readBuf[i], rewriteData[i]);
                return;
            }
        }
        printf("【SD卡调试】步骤4校验失败, 读取长度=%ld, 期望长度=%d\r\n",
               readLen, SDCARD_DEBUG_DATA_LEN);
        return;
    }
    printf("【SD卡调试】步骤4校验通过\r\n");

    printf("【SD卡调试】追加文件 'DEMO.TXT' ...\r\n");
    res = SDCardUser_AppendFile("DEMO.TXT", appendData, sizeof(appendData) - 1U, &rwLen);
    if (res != FR_OK)
    {
        printf("【SD卡调试】追加文件失败, 错误码=%d\r\n", res);
        return;
    }
    printf("【SD卡调试】追加文件操作成功，写入%ld字节\r\n", rwLen);

    memset(readBuf, 0, sizeof(readBuf));
    printf("【SD卡调试】读取文件 'DEMO.TXT' ...\r\n");
    res = SDCardUser_ReadFile("DEMO.TXT", readBuf, sizeof(readBuf), &readLen);
    if (res != FR_OK)
    {
        printf("【SD卡调试】读取文件失败, 错误码=%d\r\n", res);
        return;
    }
    printf("【SD卡调试】读取文件操作成功，读取%ld字节，内容: %s\r\n", readLen, readBuf);

    printf("【SD卡调试】重命名文件 'DEMO.TXT' -> 'DEMO2.TXT' ...\r\n");
    res = SDCardUser_RenameFile("DEMO.TXT", "DEMO2.TXT");
    if (res != FR_OK)
    {
        printf("【SD卡调试】重命名文件失败, 错误码=%d\r\n", res);
        return;
    }
    printf("【SD卡调试】重命名文件操作成功\r\n");

    fileCount = SDCardUser_ListFiles(fileList, 4);
    printf("【SD卡调试】重命名后SD卡有%d个文件\r\n", fileCount);

    printf("【SD卡调试】删除文件 'DEMO2.TXT' ...\r\n");
    res = SDCardUser_DeleteFile("DEMO2.TXT");
    if (res != FR_OK)
    {
        printf("【SD卡调试】删除文件失败, 错误码=%d\r\n", res);
        return;
    }
    printf("【SD卡调试】删除文件操作成功\r\n");

    printf("【SD卡调试】所有操作完成\r\n");

#undef SDCARD_DEBUG_DATA_LEN
}

void SDCardUser_BlockRwDebugDemo(void)
{
#define SDCARD_RW_BLOCK_COUNT      3U
#define SDCARD_RW_TOTAL_BYTES      (SDCARD_RW_BLOCK_COUNT * 512U)
#define SDCARD_RW_INTEGER_COUNT    (SDCARD_RW_TOTAL_BYTES / sizeof(u32))

    static u32 writeBuf[SDCARD_RW_INTEGER_COUNT];
    static u32 readBuf[SDCARD_RW_INTEGER_COUNT];
    u32 i;
    u32 writeLen;
    u32 readLen;
    u32 errorIndex;
    FRESULT res;
    BYTE dmaMode;

    writeLen = 0U;
    readLen = 0U;
    errorIndex = 0xFFFFFFFFUL;

    printf("【SD卡调试】开始多块文件读写调试...\r\n");
    for (i = 0U; i < SDCARD_RW_INTEGER_COUNT; i++)
    {
        writeBuf[i] = i;
        readBuf[i] = 0xFFFFFFFFUL;
    }
    printf("【SD卡调试】准备%ld个32位整数数据，共%d个数据块，总%ld字节\r\n",
           (u32)SDCARD_RW_INTEGER_COUNT, SDCARD_RW_BLOCK_COUNT, (u32)SDCARD_RW_TOTAL_BYTES);

    printf("【SD卡调试】初始化 SD 卡 ...\r\n");
    res = SDCardUser_Init();
    if (res != FR_OK)
    {
        printf("【SD卡调试】初始化失败, 错误码=%d\r\n", res);
        return;
    }
    printf("【SD卡调试】初始化成功\r\n");

    dmaMode = disk_get_dma_mode();
    printf("【SD卡调试】当前DMA模式: %s\r\n", (dmaMode != 0U) ? "开启" : "关闭");

    printf("【SD卡调试】写入文件 'INTDATA.BIN' ...\r\n");
    res = SDCardUser_RewriteFile("INTDATA.BIN", writeBuf, SDCARD_RW_TOTAL_BYTES, &writeLen);
    if (res != FR_OK)
    {
        printf("【SD卡调试】写入文件失败, 错误码=%d\r\n", res);
        return;
    }
    printf("【SD卡调试】写入文件成功，实际写入%ld字节（%d个数据块）\r\n",
           writeLen, SDCARD_RW_BLOCK_COUNT);

    printf("【SD卡调试】读取文件 'INTDATA.BIN' ...\r\n");
    res = SDCardUser_ReadFile("INTDATA.BIN", readBuf, SDCARD_RW_TOTAL_BYTES, &readLen);
    if (res != FR_OK)
    {
        printf("【SD卡调试】读取文件失败, 错误码=%d\r\n", res);
        return;
    }
    printf("【SD卡调试】读取文件成功，实际读取%ld字节\r\n", readLen);

    if (readLen != SDCARD_RW_TOTAL_BYTES)
    {
        printf("【SD卡调试】长度校验失败，期望%ld字节，实际%ld字节\r\n",
               (u32)SDCARD_RW_TOTAL_BYTES, readLen);
        return;
    }
    printf("【SD卡调试】长度校验通过\r\n");

    printf("【SD卡调试】开始逐项校验数据...\r\n");
    for (i = 0U; i < SDCARD_RW_INTEGER_COUNT; i++)
    {
        if (readBuf[i] != i)
        {
            errorIndex = i;
            break;
        }
    }

    if (errorIndex == 0xFFFFFFFFUL)
        printf("【SD卡调试】数据校验通过，共校验%ld个整数\r\n", (u32)SDCARD_RW_INTEGER_COUNT);
    else
        printf("【SD卡调试】数据校验失败，第%ld项读取=%ld，期望=%ld\r\n",
               errorIndex, readBuf[errorIndex], errorIndex);

    printf("【SD卡调试】多块文件读写调试结束\r\n");

#undef SDCARD_RW_BLOCK_COUNT
#undef SDCARD_RW_TOTAL_BYTES
#undef SDCARD_RW_INTEGER_COUNT
}

/* =================================================================
 * SDCard_DebugDemo_DMA — SD 卡 DMA / 轮询 读写速度对比测试
 *
 *   【测试流程】
 *     1. 初始化 SD 卡 + DMA2
 *     2. 确定测试扇区范围（从卡末尾 - 64 扇区开始，避免破坏文件系统）
 *     3. 用 DMA 多块方式写入并计时（每扇区填充伪随机数据）
 *     4. 用 DMA 多块方式读回并逐字节校验
 *     5. 用轮询单块方式写入并计时（使用另一组数据）
 *     6. 用轮询单块方式读回并校验
 *     7. 打印速度对比表格
 *
 *   【计时方式】DWT_CYCCNT 周期计数器（72 MHz → 每周期 13.89 ns）
 *
 *   【注意】该函数会破坏卡末尾 64 个扇区的数据，仅用于调试！
 *
 *   参数: 无
 *   返回: 无（结果通过串口打印）
 * ================================================================= */
void SDCard_DebugDemo_DMA(void)
{
    /* ---- 测试参数（可调整） ---- */
    #define SD_DMA_TEST_CNT     32U          /* 测试扇区数：32 × 512B = 16 KB */

    u32        i, j;
    u32        startCyc, endCyc;
    u32        cycDmaWrite, cycDmaRead, cycPollWrite, cycPollRead;
    u32        dmaWriteBuf[SD_BLOCK_SIZE / 4U];   /* 512 字节缓冲区（字对齐） */
    u32        verifyBuf[SD_BLOCK_SIZE / 4U];
    u8         dmaDataOk, pollDataOk;
    u32        dmaWriteKBps, dmaReadKBps, pollWriteKBps, pollReadKBps;
    u32        testSectorStart;
    u8         res;
    u32        dmaTotalBytes;
    u8        *pDmaWrite;
    u8        *pVerify;
    u32        wordCnt;

    /* ---- 初始化 DWT Cycle Counter ---- */
    *(__IO u32 *)0xE000EDFC |= (1UL << 24);     /* DEMCR |= TRCENA */
    *(__IO u32 *)0xE0001000 |= (1UL << 0);      /* DWT_CTRL |= CYCCNTENA */
    *(__IO u32 *)0xE0001004  = 0UL;             /* DWT_CYCCNT = 0 */

    dmaDataOk  = 1U;
    pollDataOk = 1U;

    dmaTotalBytes = SD_DMA_TEST_CNT * SD_BLOCK_SIZE;   /* 总测试字节数 */
    pDmaWrite     = (u8 *)dmaWriteBuf;
    pVerify       = (u8 *)verifyBuf;
    wordCnt       = SD_BLOCK_SIZE / 4U;

    printf("\r\n========== SD 卡 DMA 调试测试 ==========\r\n");

    /* ---- 1. 初始化 SD 卡 + DMA ---- */
    res = SD_Init();
    printf("SD_Init 结果: %u\r\n", res);
    if (res != SD_OK)
    {
        printf("SD 卡初始化失败！测试终止。\r\n");
        return;
    }

    SD_DMA_Init();
    printf("SD 卡信息: 类型=%u 容量=%lu 字节 (%lu MB) 扇区数=%lu 总线宽度=%u-bit\r\n",
           (u32)SDCard_Info.type,
           SDCard_Info.capacity,
           SDCard_Info.capacity / (1024UL * 1024UL),
           SDCard_Info.blk_cnt,
           (u32)SDCard_Info.bus_width);

    /* 确定测试起始扇区（从末尾 - 64 扇区开始，避免破坏文件系统） */
    if (SDCard_Info.blk_cnt > (SD_DMA_TEST_CNT + 32U))
        testSectorStart = SDCard_Info.blk_cnt - SD_DMA_TEST_CNT - 32U;
    else
        testSectorStart = 0U;   /* 卡太小，从头开始 */

    printf("测试扇区范围: %lu ~ %lu (%lu 扇区 = %lu KB)\r\n",
           testSectorStart,
           testSectorStart + SD_DMA_TEST_CNT - 1U,
           (u32)SD_DMA_TEST_CNT,
           (u32)dmaTotalBytes / 1024UL);

    /* ---- 2. DMA 多块写入 + 计时 ---- */
    printf("\r\n--- [1] DMA 多块写入 (%lu KB) ---\r\n", (u32)dmaTotalBytes / 1024UL);
    startCyc = *(__IO u32 *)0xE0001004;

    for (i = 0U; i < SD_DMA_TEST_CNT; i++)
    {
        /* 填充伪随机数据：每个扇区用扇区号异或 0xA5 */
        for (j = 0U; j < wordCnt; j++)
        {
            u32 base = (testSectorStart + i) * SD_BLOCK_SIZE + j * 4U;
            dmaWriteBuf[j] = base ^ 0xA5A5A5A5UL;
        }

        res = SD_WriteBlocks_DMA(pDmaWrite, testSectorStart + i, 1U);
        if (res != SD_OK)
        {
            printf("  DMA 写入扇区 %lu 失败, 错误码=%u\r\n", testSectorStart + i, res);
            dmaDataOk = 0U;
        }
    }

    endCyc = *(__IO u32 *)0xE0001004;
    cycDmaWrite = endCyc - startCyc;
    printf("  DMA 写入完成, 耗时 %lu 周期\r\n", cycDmaWrite);

    /* ---- 3. DMA 多块读取 + 校验 + 计时 ---- */
    printf("--- [2] DMA 多块读取 + 校验 ---\r\n");
    startCyc = *(__IO u32 *)0xE0001004;

    for (i = 0U; i < SD_DMA_TEST_CNT; i++)
    {
        res = SD_ReadBlocks_DMA(pVerify, testSectorStart + i, 1U);
        if (res != SD_OK)
        {
            printf("  DMA 读取扇区 %lu 失败, 错误码=%u\r\n", testSectorStart + i, res);
            dmaDataOk = 0U;
            continue;
        }

        /* 逐字校验 */
        for (j = 0U; j < wordCnt; j++)
        {
            u32 exp = ((testSectorStart + i) * SD_BLOCK_SIZE + j * 4U) ^ 0xA5A5A5A5UL;
            if (verifyBuf[j] != exp)
            {
                if (dmaDataOk != 0U)
                {
                    printf("  [DMA] 数据不一致! 扇区%lu 偏移%lu字: 期望0x%08lX 实际0x%08lX\r\n",
                           testSectorStart + i, j, exp, verifyBuf[j]);
                }
                dmaDataOk = 0U;
            }
        }
    }

    endCyc = *(__IO u32 *)0xE0001004;
    cycDmaRead = endCyc - startCyc;
    printf("  DMA 读取完成, 耗时 %lu 周期\r\n", cycDmaRead);
    printf("  DMA 数据校验: %s\r\n", (dmaDataOk != 0U) ? "通过" : "失败");

    /* ---- 4. 轮询单块写入 + 计时（使用另一组数据） ---- */
    printf("\r\n--- [3] 轮询单块写入 (%lu KB) ---\r\n", (u32)dmaTotalBytes / 1024UL);
    startCyc = *(__IO u32 *)0xE0001004;

    for (i = 0U; i < SD_DMA_TEST_CNT; i++)
    {
        /* 用 ^ 0x5A5A5A5A 产生不同的数据 */
        for (j = 0U; j < wordCnt; j++)
        {
            u32 base = (testSectorStart + i) * SD_BLOCK_SIZE + j * 4U;
            dmaWriteBuf[j] = base ^ 0x5A5A5A5AUL;
        }

        res = SD_WriteSingleBlock(pDmaWrite, testSectorStart + i);
        if (res != SD_OK)
        {
            printf("  轮询写入扇区 %lu 失败, 错误码=%u\r\n", testSectorStart + i, res);
            pollDataOk = 0U;
        }
    }

    endCyc = *(__IO u32 *)0xE0001004;
    cycPollWrite = endCyc - startCyc;
    printf("  轮询写入完成, 耗时 %lu 周期\r\n", cycPollWrite);

    /* ---- 5. 轮询单块读取 + 校验 + 计时 ---- */
    printf("--- [4] 轮询单块读取 + 校验 ---\r\n");
    startCyc = *(__IO u32 *)0xE0001004;

    for (i = 0U; i < SD_DMA_TEST_CNT; i++)
    {
        res = SD_ReadSingleBlock(pVerify, testSectorStart + i);
        if (res != SD_OK)
        {
            printf("  轮询读取扇区 %lu 失败, 错误码=%u\r\n", testSectorStart + i, res);
            pollDataOk = 0U;
            continue;
        }

        for (j = 0U; j < wordCnt; j++)
        {
            u32 exp = ((testSectorStart + i) * SD_BLOCK_SIZE + j * 4U) ^ 0x5A5A5A5AUL;
            if (verifyBuf[j] != exp)
            {
                if (pollDataOk != 0U)
                {
                    printf("  [轮询] 数据不一致! 扇区%lu 偏移%lu字: 期望0x%08lX 实际0x%08lX\r\n",
                           testSectorStart + i, j, exp, verifyBuf[j]);
                }
                pollDataOk = 0U;
            }
        }
    }

    endCyc = *(__IO u32 *)0xE0001004;
    cycPollRead = endCyc - startCyc;
    printf("  轮询读取完成, 耗时 %lu 周期\r\n", cycPollRead);
    printf("  轮询数据校验: %s\r\n", (pollDataOk != 0U) ? "通过" : "失败");

    /* ---- 6. 计算速度并打印汇总 ---- */
    #define SD_CYCLES_PER_US   72UL
    #define SD_US_PER_SEC      1000000UL

    dmaWriteKBps  = (u32)dmaTotalBytes * SD_US_PER_SEC / (cycDmaWrite  / SD_CYCLES_PER_US) / 1024UL;
    dmaReadKBps   = (u32)dmaTotalBytes * SD_US_PER_SEC / (cycDmaRead   / SD_CYCLES_PER_US) / 1024UL;
    pollWriteKBps = (u32)dmaTotalBytes * SD_US_PER_SEC / (cycPollWrite / SD_CYCLES_PER_US) / 1024UL;
    pollReadKBps  = (u32)dmaTotalBytes * SD_US_PER_SEC / (cycPollRead  / SD_CYCLES_PER_US) / 1024UL;

    printf("\r\n============================================\r\n");
    printf("  SD 卡 DMA / 轮询 速度对比  (%lu KB)\r\n",
           (u32)dmaTotalBytes / 1024UL);
    printf("============================================\r\n");
    printf("  数据一致性        DMA=%s  轮询=%s\r\n",
           (dmaDataOk  != 0U) ? "OK" : "FAIL",
           (pollDataOk != 0U) ? "OK" : "FAIL");
    printf("--------------------------------------------\r\n");
    printf("  传输方式      |  耗时(us)    |  速度(KB/s)\r\n");
    printf("--------------------------------------------\r\n");
    printf("  DMA 写入      | %12lu | %11lu\r\n",
           cycDmaWrite  / SD_CYCLES_PER_US, dmaWriteKBps);
    printf("  DMA 读取      | %12lu | %11lu\r\n",
           cycDmaRead   / SD_CYCLES_PER_US, dmaReadKBps);
    printf("  轮询写入      | %12lu | %11lu\r\n",
           cycPollWrite / SD_CYCLES_PER_US, pollWriteKBps);
    printf("  轮询读取      | %12lu | %11lu\r\n",
           cycPollRead  / SD_CYCLES_PER_US, pollReadKBps);
    printf("--------------------------------------------\r\n");
    printf("\r\n========== 测试结束 ==========\r\n");
}
