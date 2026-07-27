#ifndef __SPI_EEPROM_H
#define __SPI_EEPROM_H

#include "sys.h"
#include "Hardware_Config.h"

/* FT25C64A SPI EEPROM 芯片说明：
 * 容量：64 Kbit = 8 KB
 * 页大小：32 Byte
 * 采用 16 位地址（0x0000~0x1FFF）
 * 标准时序：SPI Mode 0
 */

/* ---- SPI EEPROM 指令码 ---- */
#define SPI_EEPROM_CMD_WREN      0x06    /* 写使能 */
#define SPI_EEPROM_CMD_WRDI      0x04    /* 写禁止 */
#define SPI_EEPROM_CMD_RDSR      0x05    /* 读状态寄存器 */
#define SPI_EEPROM_CMD_WRSR      0x01    /* 写状态寄存器 */
#define SPI_EEPROM_CMD_READ      0x03    /* 读数据 */
#define SPI_EEPROM_CMD_WRITE     0x02    /* 写数据 */

/* ---- 存储结构参数 ---- */
#define SPI_EEPROM_PAGE_SIZE     32U     /* 页大小：32 字节 */
#define SPI_EEPROM_CAPACITY      8192U   /* 总容量：8192 字节（8KB） */
#define SPI_EEPROM_MAX_ADDR      (SPI_EEPROM_CAPACITY - 1U)  /* 最大地址：0x1FFF */

/* ---- 状态寄存器位定义 ---- */
#define SPI_EEPROM_SR_WIP        0x01    /* bit0: 忙标志（1=正在编程/擦除） */
#define SPI_EEPROM_SR_WEL        0x02    /* bit1: 写使能锁存（1=已使能） */
#define SPI_EEPROM_SR_BP0        0x04    /* bit2: 块保护位0 */
#define SPI_EEPROM_SR_BP1        0x08    /* bit3: 块保护位1 */
#define SPI_EEPROM_SR_WPEN       0x80    /* bit7: 写保护使能 */

/* ---- 引脚控制宏 ---- */
#define SPI_EEPROM_CS_L()        (PORT_OUT(HW_SPI_EEPROM_CS) = 0)  /* 选中 EEPROM 芯片 */
#define SPI_EEPROM_CS_H()        (PORT_OUT(HW_SPI_EEPROM_CS) = 1)  /* 取消选中 */
#define SPI_EEPROM_WP_L()        (PORT_OUT(HW_SPI_EEPROM_WP) = 0)  /* 写保护使能 */
#define SPI_EEPROM_WP_H()        (PORT_OUT(HW_SPI_EEPROM_WP) = 1)  /* 写保护禁止 */

/* ---- EEPROM 操作函数 ---- */
void SPI_EEPROM_Init(void);                 /* 初始化 SPI EEPROM */
void SPI_EEPROM_WriteEnable(void);          /* 写使能 */
void SPI_EEPROM_WriteDisable(void);         /* 写禁止 */
u8   SPI_EEPROM_ReadStatusReg(void);        /* 读状态寄存器 */
void SPI_EEPROM_WriteStatusReg(u8 sr);      /* 写状态寄存器 */
void SPI_EEPROM_WaitBusy(void);             /* 等待操作完成 */
u8   SPI_EEPROM_ReadByte(u32 addr);         /* 读取一个字节 */
void SPI_EEPROM_WriteByte(u32 addr, u8 data);   /* 写入一个字节 */
void SPI_EEPROM_Read(u32 addr, u8 *pBuf, u16 len);  /* 读取连续数据 */
void SPI_EEPROM_Write(u32 addr, const u8 *pBuf, u16 len); /* 写入连续数据（自动处理跨页） */
void SPI_EEPROM_WritePage(u32 addr, const u8 *pBuf, u16 len); /* 写入一页数据 */
void SPI_EEPROM_EraseAll(void);             /* 全片擦除（写0xFF到所有地址） */
void SPI_EEPROM_ReadID(u8 *mid, u8 *did);   /* 读取厂商ID和设备ID */
void SPI_EEPROM_DebugDemo(void);            /* 调试示例：读ID、写入、回读比对 */

#endif

