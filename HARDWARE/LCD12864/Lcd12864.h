/*
 * LCD12864驱动头文件 - 显示函数和绘图接口
 */

#ifndef __LCD12864_H
#define __LCD12864_H

#include "sys.h"
#include "Hardware_Config.h"
#include "FontGB2312.h"

// LCD 命令宏定义 (ST7567)
#define LCD_CMD_RESET               0xe2    // 软复位
#define LCD_CMD_BOOST1              0x2c    // 升压步骤1
#define LCD_CMD_BOOST2              0x2e    // 升压步骤2
#define LCD_CMD_BOOST3              0x2f    // 升压步骤3
#define LCD_CMD_RESRATIO            0x23    // 粗调对比度 (0x20~0x27)
#define LCD_CMD_RESTRIM             0x81    // 微调对比度指令
#define LCD_CMD_TRIMVALUE           0x28    // 微调对比度参数值 (0x00~0x3F)
#define LCD_CMD_BIAS                0xa2    // 1/9 偏压比
#define LCD_CMD_LINESCAN            0xc8    // 行扫描顺序：从上到下
#define LCD_CMD_COLUMNSCAN          0xa0    // 列扫描顺序：从左到右
#define LCD_CMD_STARTLINE           0x40    // 起始行
#define LCD_CMD_DISPLAYON           0xaf    // 显示开
#define LCD_CMD_DISPLAYOFF          0xae    // 显示关
#define LCD_DAT_CLEARSCREEN         0x00    // 清屏数据

// RS 控制宏
#define LCD_RS_CMD()                PORT_OUT(HW_LCD12864_RS) = 0  // 命令模式 (A0=0)
#define LCD_RS_DATA()               PORT_OUT(HW_LCD12864_RS) = 1  // 数据模式 (A0=1)

// RST 控制宏
#define LCD_RST_L()                 PORT_OUT(HW_LCD12864_RST) = 0 // 复位低电平
#define LCD_RST_H()                 PORT_OUT(HW_LCD12864_RST) = 1 // 复位高电平

// CS 控制宏
#define LCD_CS_L()                  PORT_OUT(HW_LCD12864_CS) = 0  // 片选使能
#define LCD_CS_H()                  PORT_OUT(HW_LCD12864_CS) = 1  // 片选释放

// 函数声明
void LCD_GPIO_Init(void);
void LCD_WriteCmd(uint8_t cmd);
void LCD_WriteData(uint8_t data);
void LCD_SetAddress(uint8_t page, uint8_t column);
void LCD_Init(void);
void LCD_Clear(void);
void LCD_Display128x64(const uint8_t *dp);
void LCD_DisplayGraphic(uint8_t page, uint8_t column, uint8_t xDotCount, uint8_t yDotCount, const uint8_t *dp);
void LCD_DisplayGraphic16x16(uint8_t page, uint8_t column, const uint8_t *dp);
void LCD_DisplayGraphic8x16(uint8_t page, uint8_t column, const uint8_t *dp);
void LCD_DisplayGB2312String(uint8_t page, uint8_t charIndex, const uint8_t *text);
void LCD_DisplayString58(uint8_t page, uint8_t charIndex, const uint8_t *text);

#endif


