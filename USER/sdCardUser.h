#ifndef __SD_CARD_USER_H__
#define __SD_CARD_USER_H__

#include "sys.h"
#include "ff.h"

#define SDCARD_USER_MAX_NAME_LEN      13

/* SD 卡文件系统初始化 */
FRESULT SDCardUser_Init(void);
/* 读取根目录文件列表，仅支持 8.3 短文件名 */
u16     SDCardUser_ListFiles(char fileList[][SDCARD_USER_MAX_NAME_LEN], u16 maxFiles);
/* 创建空文件 */
FRESULT SDCardUser_CreateFile(const char *fileName);
/* 追加写入文件内容，不存在则自动创建 */
FRESULT SDCardUser_AppendFile(const char *fileName, const void *data, u32 dataLen, u32 *writtenLen);
/* 重写文件内容，原文件存在时先删除再重建 */
FRESULT SDCardUser_RewriteFile(const char *fileName, const void *data, u32 dataLen, u32 *writtenLen);
/* 读取文件内容 */
FRESULT SDCardUser_ReadFile(const char *fileName, void *dataBuf, u32 bufSize, u32 *readLen);
/* 删除文件 */
FRESULT SDCardUser_DeleteFile(const char *fileName);
/* 修改文件名 */
FRESULT SDCardUser_RenameFile(const char *oldName, const char *newName);
/* 调试示例：演示 SD 卡文件基本操作 */
void    SDCardUser_DebugDemo(void);
/* 调试示例：多块整数文件写入与读回校验 */
void    SDCardUser_BlockRwDebugDemo(void);
/* 调试示例：DMA/轮询读写速度对比测试 */
void SDCard_DebugDemo_DMA(void);                                     

#endif


