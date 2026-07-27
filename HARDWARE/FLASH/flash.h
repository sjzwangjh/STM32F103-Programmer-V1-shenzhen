#ifndef __FLASH_H
#define __FLASH_H

#include "sys.h"
#include "Hardware_Config.h"

/* SPI NOR Flash 指令集定义
 * 支持 W25X/W25Q 系列 SPI Flash 芯片
 * 当前板载芯片为 FM25W32，容量 32 Mbit = 4 MB
 */

/* 支持芯片 ID 列表 */
#define W25Q80                  0xEF13  /* W25Q80 8Mbit */
#define W25Q16                  0xEF14  /* W25Q16 16Mbit */
#define W25Q32                  0xEF15  /* W25Q32 32Mbit */
#define W25Q64                  0xEF16  /* W25Q64 64Mbit */
#define FM25W32_ID              0x4016  /* FM25W32 32Mbit */

extern u16 SPI_FLASH_TYPE;              /* 当前检测到的 Flash 芯片型号 */

/* ---- 片选和写保护引脚控制 ---- */
#define FLASH_CS_L()            (PORT_OUT(HW_FLASH_CS) = 0)  /* 选中 Flash 芯片 */
#define FLASH_CS_H()            (PORT_OUT(HW_FLASH_CS) = 1)  /* 取消选中 */
#define FLASH_WP_L()            (PORT_OUT(HW_FLASH_WP) = 0)  /* 写保护使能 */
#define FLASH_WP_H()            (PORT_OUT(HW_FLASH_WP) = 1)  /* 写保护禁止 */

#define SPI_FLASH_CS            PORT_OUT(HW_FLASH_CS)

/* ---- SPI Flash 指令码 ---- */
#define W25X_WriteEnable        0x06    /* 写使能 */
#define W25X_WriteDisable       0x04    /* 写禁止 */
#define W25X_ReadStatusReg      0x05    /* 读状态寄存器 */
#define W25X_WriteStatusReg     0x01    /* 写状态寄存器 */
#define W25X_ReadData           0x03    /* 读数据 */
#define W25X_FastReadData       0x0B    /* 快速读数据 */
#define W25X_FastReadDual       0x3B    /* 双线快速读 */
#define W25X_PageProgram        0x02    /* 页编程（写入） */
#define W25X_BlockErase         0xD8    /* 块擦除（64KB） */
#define W25X_SectorErase        0x20    /* 扇区擦除（4KB） */
#define W25X_ChipErase          0xC7    /* 整片擦除 */
#define W25X_PowerDown          0xB9    /* 掉电 */
#define W25X_ReleasePowerDown   0xAB    /* 释放掉电 */
#define W25X_DeviceID           0xAB    /* 读设备 ID */
#define W25X_ManufactDeviceID   0x90    /* 读厂商/设备 ID */
#define W25X_JedecDeviceID      0x9F    /* 读 JEDEC ID */

/* ---- Flash 存储结构参数 ---- */
#define FLASH_PAGE_SIZE         256UL   /* 页大小：256 字节 */
#define FLASH_SECTOR_SIZE       4096UL  /* 扇区大小：4 KB（16页） */
#define FLASH_BLOCK_SIZE        65536UL /* 块大小：64 KB（16扇区） */
#define FLASH_CAPACITY          0x400000UL  /* 总容量：4 MB */

/* ---- 基本操作函数 ---- */
void SPI_Flash_Init(void);                                          /* 初始化 SPI Flash */
u16  SPI_Flash_ReadID(void);                                        /* 读设备 ID */
u32  SPI_Flash_ReadJEDECID(void);                                   /* 读 JEDEC ID */
u8   SPI_Flash_ReadSR(void);                                        /* 读状态寄存器 */
void SPI_FLASH_Write_SR(u8 sr);                                     /* 写状态寄存器 */
void SPI_FLASH_Write_Enable(void);                                  /* 写使能 */
void SPI_FLASH_Write_Disable(void);                                 /* 写禁止 */
void SPI_Flash_Wait_Busy(void);                                     /* 等待忙状态结束 */
void SPI_Flash_Read(u8 *pBuffer, u32 ReadAddr, u16 NumByteToRead);  /* 读数据 */
void SPI_Flash_Write_Page(const u8 *pBuffer, u32 WriteAddr, u16 NumByteToWrite);  /* 写一页 */
void SPI_Flash_Write_NoCheck(const u8 *pBuffer, u32 WriteAddr, u16 NumByteToWrite); /* 连续写（不检查） */
void SPI_Flash_Write(const u8 *pBuffer, u32 WriteAddr, u16 NumByteToWrite); /* 写数据（含擦除） */
void SPI_Flash_Erase_Chip(void);                                    /* 整片擦除 */
void SPI_Flash_Erase_Sector(u32 sectorIndex);                       /* 擦除扇区 */
void SPI_Flash_Erase_Block(u32 blockIndex);                         /* 擦除块 */
void SPI_Flash_PowerDown(void);                                     /* 进入掉电模式 */
void SPI_Flash_WAKEUP(void);                                        /* 唤醒 */
void SPI_Flash_WP_Set(u8 enable);                                   /* 设置写保护 */

/* ========== SPI2 DMA1_Ch4/Ch5 传输函数 ========== */
void SPI_Flash_DMA_Init(void);                          /* 初始化 DMA1_Ch4/Ch5 用于 SPI2 */
u8   SPI_Flash_Read_DMA_Start(u8 *pBuffer, u32 ReadAddr, u16 NumByteToRead); /* 启动 DMA 读 */
u8   SPI_Flash_Read_DMA_IsFinished(void);               /* 查询 DMA 读是否完成 */
void SPI_Flash_Read_DMA(u8 *pBuffer, u32 ReadAddr, u16 NumByteToRead);   /* DMA 读数据（阻塞兼容接口） */
u8   SPI_Flash_Write_Page_DMA_Start(const u8 *pBuffer, u32 WriteAddr, u16 NumByteToWrite); /* 启动 DMA 页写 */
u8   SPI_Flash_Write_Page_DMA_IsFinished(void);         /* 查询 DMA 页写是否完成 */
void SPI_Flash_Write_Page_DMA(const u8 *pBuffer, u32 WriteAddr, u16 NumByteToWrite); /* DMA 页写（阻塞兼容接口） */
void SPI_Flash_DebugDemo(void);                         /* 调试示例：读ID、写入、回读比对 */
void SPI_Flash_DebugDemo_DMA(void);                     /* 调试示例：DMA/轮询读写速度对比测试 */
u32  SPI_Flash_Erase_Auto(void);                        /* 自动检测并擦除已使用的 Flash 扇区 */

#endif

