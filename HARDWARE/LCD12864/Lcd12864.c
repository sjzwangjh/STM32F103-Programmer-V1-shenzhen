#include "Lcd12864.h"
#include "delay.h"

/*******************************************************************************
 * LCD SPI 专用延迟，与 GB2312 字库 SPI 延迟相互独立
 * LCD 通讯速率通常较慢（参考 LCD12864.cpp 中 m_ClkDelayUs = 200）
 * 此处采用空循环实现约 200us 半周期延迟
 ******************************************************************************/
static void lcd_spi_delay(void)
{
    uint8_t i;
    for(i = 0; i < 60; i++);
}

/*******************************************************************************
 * @brief  LCD 专用 SPI 写一个字节（仅发送，不读取 MISO）
 * @param  data : 要发送的字节
 * @note   当 LCD 与 GB2312 字库共享 SCK/MOSI 引脚时，
 *         LCD 通信用本函数，字库通信用 GB2312_SPI_ReadByte()，
 *         二者使用独立的延迟常数，避免时序互相干扰。
 ******************************************************************************/
void LCD_SPI_WriteByte(uint8_t data)
{
    uint8_t i;

    for(i = 0; i < 8; i++)
    {
        /* 设置 MOSI 数据位 */
        if(data & 0x80)
            PORT_OUT(HW_LCD12864_GB2312_MOSI) = 1;
        else
            PORT_OUT(HW_LCD12864_GB2312_MOSI) = 0;
        data <<= 1;

        lcd_spi_delay();

        /* SCK ↑ 上升沿：LCD 锁存 MOSI */
        PORT_OUT(HW_LCD12864_GB2312_SCK) = 1;

        lcd_spi_delay();

        /* SCK ↓ 下降沿 */
        PORT_OUT(HW_LCD12864_GB2312_SCK) = 0;
    }
}

/*******************************************************************************
 * @brief  初始化 LCD 和字库芯片的 GPIO 引脚
 *         - SCK(B,1) 和 MOSI(C,5) 由 GB2312_SPI_Init() 初始化（两者共享）
 *         - CS(E,10)、RS(E,8)、RST(B,10) 在此初始化
 * @note   需先调用 GB2312_SPI_Init() 初始化共享的 SCK/MOSI 引脚
 ******************************************************************************/
void LCD_GPIO_Init(void)
{
    /* 使能 GPIO 时钟 */
    PORT_RCC_CLK(HW_LCD12864_CS);
    PORT_RCC_CLK(HW_LCD12864_RS);
    PORT_RCC_CLK(HW_LCD12864_RST);

    /* CS → 推挽输出，高电平释放片选 */
    PORT_SET_DIR_PP(HW_LCD12864_CS);
    PORT_OUT(HW_LCD12864_CS) = 1;

    /* RS → 推挽输出，默认命令模式 */
    PORT_SET_DIR_PP(HW_LCD12864_RS);
    PORT_OUT(HW_LCD12864_RS) = 0;

    /* RST → 推挽输出 */
    PORT_SET_DIR_PP(HW_LCD12864_RST);
    PORT_OUT(HW_LCD12864_RST) = 1;
    
    /* 初始化LCD同时初始化字库 */
    GB2312_SPI_Init();   // 初始化字库 SPI 引脚（共享 SCK/MOSI）
}

/*******************************************************************************
 * @brief  向 LCD 写入一个命令字节
 * @param  cmd : 命令字节
 * @note
 *         时序：CS=L → RS=0(命令) → 写字节(SCK 上升沿 MOSI 移出) → CS=H
 ******************************************************************************/
void LCD_WriteCmd(uint8_t cmd)
{
    LCD_CS_L();                 // 片选使能
    LCD_RS_CMD();               // 选择命令模式 (A0=0)
    LCD_SPI_WriteByte(cmd);     // LCD 专用 SPI 发送命令字节
    LCD_CS_H();                 // 片选释放
}

/*******************************************************************************
 * @brief  向 LCD 写入一个数据字节
 * @param  data : 数据字节
 * @note
 *         时序：CS=L → RS=1(数据) → 写字节(SCK 上升沿 MOSI 移出) → RS=0 → CS=H
 ******************************************************************************/
void LCD_WriteData(uint8_t data)
{
    LCD_CS_L();                 // 片选使能
    LCD_RS_DATA();              // 选择数据模式 (A0=1)
    LCD_SPI_WriteByte(data);    // LCD 专用 SPI 发送数据字节
    LCD_RS_CMD();               // 恢复命令模式（参考 LCD12864.cpp 的做法）
    LCD_CS_H();                 // 片选释放
}

/*******************************************************************************
 * @brief  设置 LCD 显示地址（页+列）
 * @param  page   : 页地址，1~8（64 行分为 8 页，每页 8 行）
 * @param  column : 列地址，1~128
 * @note
 *         ST7567 内部命令：
 *         页地址：0xB0 + (page-1)
 *         列地址高4位：0x10 + ((column-1) >> 4)
 *         列地址低4位：0x00 + ((column-1) & 0x0F)
 ******************************************************************************/
void LCD_SetAddress(uint8_t page, uint8_t column)
{
    uint8_t col = column - 1;

    LCD_WriteCmd(0xB0 + (page - 1));        // 页地址
    LCD_WriteCmd(((col >> 4) & 0x0F) + 0x10); // 列地址高4位
    LCD_WriteCmd(col & 0x0F);               // 列地址低4位
}

/*******************************************************************************
 * @brief  初始化 LCD 模块（ST7567 控制器）
 * @note
 *         初始化序列参考 LCD12864.cpp：
 *         硬件复位 → 软复位 → 升压 → 对比度 → 偏压比 → 行/列扫描顺序 → 开显示
 ******************************************************************************/
void LCD_Init(void)
{
    /* 硬件复位 */
    LCD_RST_L();
    delay_ms(200);
    LCD_RST_H();
    delay_ms(200);

    /* 软复位 */
    LCD_WriteCmd(LCD_CMD_RESET);
    delay_ms(50);

    /* 升压步骤 */
    LCD_WriteCmd(LCD_CMD_BOOST1);       // 升压步骤1
    delay_ms(50);
    LCD_WriteCmd(LCD_CMD_BOOST2);       // 升压步骤2
    delay_ms(50);
    LCD_WriteCmd(LCD_CMD_BOOST3);       // 升压步骤3
    delay_ms(50);

    /* 对比度设置 */
    LCD_WriteCmd(LCD_CMD_RESRATIO);     // 粗调对比度
    LCD_WriteCmd(LCD_CMD_RESTRIM);      // 微调对比度指令
    LCD_WriteCmd(LCD_CMD_TRIMVALUE);    // 微调对比度参数值

    /* 显示参数设置 */
    LCD_WriteCmd(LCD_CMD_BIAS);         // 1/9 偏压比
    LCD_WriteCmd(LCD_CMD_LINESCAN);     // 行扫描：从上到下
    LCD_WriteCmd(LCD_CMD_COLUMNSCAN);   // 列扫描：从左到右
    LCD_WriteCmd(LCD_CMD_STARTLINE);    // 起始行：第0行

    /* 显示开启 */
    LCD_WriteCmd(LCD_CMD_DISPLAYON);

    /* 清屏 */
    LCD_Clear();
}

/*******************************************************************************
 * @brief  全屏清屏，将整个 128×64 的显示缓冲区清零
 ******************************************************************************/
void LCD_Clear(void)
{
    uint8_t i, j;

    for(i = 0; i < 8; i++)          // 8 页
    {
        LCD_SetAddress(i + 1, 1);   // 第1列开始
        for(j = 0; j < 128; j++)    // 128 列
        {
            LCD_WriteData(LCD_DAT_CLEARSCREEN);
        }
    }
}

/*******************************************************************************
 * @brief  显示 128×64 点阵全屏图像
 * @param  dp : 图像数据指针（128×64 / 8 = 1024 字节）
 ******************************************************************************/
void LCD_Display128x64(const uint8_t *dp)
{
    uint8_t i;
    uint16_t j;

    for(i = 0; i < 8; i++)              // 8 页
    {
        LCD_SetAddress(i + 1, 1);       // 每页从第1列开始
        for(j = 0; j < 128; j++)        // 128 列
        {
            LCD_WriteData(*dp++);
        }
    }
}

/*******************************************************************************
 * @brief  在指定位置显示指定尺寸的图像
 * @param  page      : 起始页，1~8
 * @param  column    : 起始列，1~128
 * @param  xDotCount : 图像宽度（点数）
 * @param  yDotCount : 图像高度（点数，必须为 8 的倍数）
 * @param  dp        : 图像数据指针
 ******************************************************************************/
void LCD_DisplayGraphic(uint8_t page, uint8_t column, uint8_t xDotCount, uint8_t yDotCount, const uint8_t *dp)
{
    uint8_t i, j;
    uint8_t yByteCount = yDotCount / 8;  // 高度占用的页数

    for(i = 0; i < yByteCount; i++)
    {
        LCD_SetAddress(page + i, column);
        for(j = 0; j < xDotCount; j++)
        {
            LCD_WriteData(*dp++);
        }
    }
}

/*******************************************************************************
 * @brief  在指定位置显示 16×16 点阵图像（如汉字、图标）
 * @param  page   : 起始页，1~8
 * @param  column : 起始列，1~128
 * @param  dp     : 16×16 图像数据指针（16*16/8 = 32 字节）
 ******************************************************************************/
void LCD_DisplayGraphic16x16(uint8_t page, uint8_t column, const uint8_t *dp)
{
    uint8_t i, j;

    for(i = 0; i < 2; i++)          // 占 2 页
    {
        LCD_SetAddress(page + i, column);
        for(j = 0; j < 16; j++)     // 每页 16 列
        {
            LCD_WriteData(*dp++);
        }
    }
}

/*******************************************************************************
 * @brief  在指定位置显示 8×16 点阵图像（如 ASCII 8×16 字符）
 * @param  page   : 起始页，1~8
 * @param  column : 起始列，1~128
 * @param  dp     : 8×16 图像数据指针（8*16/8 = 16 字节）
 ******************************************************************************/
void LCD_DisplayGraphic8x16(uint8_t page, uint8_t column, const uint8_t *dp)
{
    uint8_t i, j;

    for(i = 0; i < 2; i++)          // 占 2 页
    {
        LCD_SetAddress(page + i, column);
        for(j = 0; j < 8; j++)      // 每页 8 列
        {
            LCD_WriteData(*dp++);
        }
    }
}

/*******************************************************************************
 * @brief  显示 GB2312 编码的字符串（中文用 16×16，ASCII 用 8×16 粗体）
 * @param  page      : 起始页，1~8
 * @param  charIndex : 起始字符位置索引，从 1 开始
 * @param  text      : GB2312 编码的字符串（以 \0 结尾）
 * @note
 *         每个汉字占 16 列，每个 ASCII 占 8 列
 *         column = (charIndex-1) * 8 + 1
 *         中文字符合为一个字符时：汉字占16列，ASCII依次排列
 ******************************************************************************/
void LCD_DisplayGB2312String(uint8_t page, uint8_t charIndex, const uint8_t *text)
{
    uint8_t i = 0;
    uint8_t column = (charIndex - 1) * 8 + 1;
    uint8_t buf[32];    // 用于缓存字模数据

    while(text[i] != 0)
    {
        if((text[i] >= 0xB0) && (text[i] <= 0xF7) && (text[i + 1] >= 0xA1))
        {
            /* 一级/二级汉字（16×16 点阵） */
            GB2312_GetChnFont(text[i], text[i + 1], buf);
            LCD_DisplayGraphic16x16(page, column, buf);
            i += 2;
            column += 16;
        }
        else if((text[i] >= 0xA1) && (text[i] <= 0xA9) && (text[i + 1] >= 0xA1))
        {
            /* GB2312 符号区字符（15×16 点阵，使用 16×16 显示区域显示） */
            GB2312_GetChnFont(text[i], text[i + 1], buf);
            LCD_DisplayGraphic16x16(page, column, buf);
            i += 2;
            column += 16;
        }
        else if((text[i] >= 0x20) && (text[i] <= 0x7E))
        {
            /* ASCII 字符（8×16 粗体） */
            GB2312_GetAscii8x16(text[i], buf);
            LCD_DisplayGraphic8x16(page, column, buf);
            i += 1;
            column += 8;
        }
        else
        {
            i++;    // 跳过无法识别的字节
        }

        /* 超出屏幕宽度则截断 */
        if(column > 128)
            break;
    }
}

/*******************************************************************************
 * @brief  显示 5×7 点阵英文字符串（一行显示）
 * @param  page      : 页，1~8
 * @param  charIndex : 字符起始位置，从 1 开始
 * @param  text      : ASCII 字符串（以 \0 结尾）
 * @note
 *         每个字符占 6 列（5 点阵宽度 + 1 间隔）
 *         最大 21 个字符每行
 ******************************************************************************/
void LCD_DisplayString58(uint8_t page, uint8_t charIndex, const uint8_t *text)
{
    uint8_t i = 0;
    uint8_t column = (charIndex - 1) * 6 + 1;
    uint8_t buf[8];     // 5×7 ASCII 字模 8 字节

    while(text[i] != 0)
    {
        if((text[i] >= 0x20) && (text[i] <= 0x7E))
        {
            GB2312_GetAscii5x7(text[i], buf);
            LCD_SetAddress(page, column);
            LCD_WriteData(buf[0]);  // 5×7 点阵每
            LCD_WriteData(buf[1]);  // 个字符占 5 个字
            LCD_WriteData(buf[2]);  // 节，余 3 字节为
            LCD_WriteData(buf[3]);  // 0（竖置横排排
            LCD_WriteData(buf[4]);  // 列，5×7 实际 5 字节/行）
            column += 6;    // 5 列实际宽度 + 1 间隔
        }
        i++;

        if(column >= 128)
            break;
    }
}

