#ifndef __FONT_GB2312_H__
#define __FONT_GB2312_H__

#include "sys.h"
#include "Lcd12864.h"

// 字库类型常量
#define GB2312_CHN_FONT_SIZE    32  // 15×16 汉字字模字节数
#define GB2312_ASC8x16_SIZE     16  // 8×16 ASCII字模字节数
#define GB2312_ASC8x16_BASE     0x3CF80UL  // 8×16 ASCII粗体字符起始地址

// 函数声明
void GB2312_SPI_Init(void);
uint8_t GB2312_SPI_ReadByte(uint8_t data);
void GB2312_SPI_ReadData(uint32_t addr, uint8_t *buf, uint16_t len);
uint8_t GB2312_GetChnFont(uint8_t msb, uint8_t lsb, uint8_t *buf);
uint8_t GB2312_GetAscii8x16(uint8_t ascii, uint8_t *buf);
uint8_t GB2312_GetAscii5x7(uint8_t ascii, uint8_t *buf);
uint8_t GB2312_GetAscii7x8(uint8_t ascii, uint8_t *buf);
uint8_t GB2312_GetExtChar(uint16_t fontcode, uint8_t *buf);

#endif



