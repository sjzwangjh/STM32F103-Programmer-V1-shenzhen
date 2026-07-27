#include "FontGB2312.h"

/*******************************************************************************
 * 软件 SPI 底层延迟
 * 在 72MHz 主频下，约产生 1~2us 的延迟（每个SCK半周期约5us，总周期10us）
 ******************************************************************************/
static void spi_delay(void)
{
    uint8_t i;
    for(i = 0; i < 30; i++);
}

/*******************************************************************************
 * @brief  初始化字库 SPI 接口引脚
 *         MOSI(C,5) → PP输出, SCK(B,1) → PP输出, CS(E,7) → PP输出(高=释放)
 *         MISO(B,0) → 浮空输入
 * @note   时钟和 GPIO 端口复用由 Lcd12864.h 中宏定义决定
 ******************************************************************************/
void GB2312_SPI_Init(void)
{
    /* 使能 GPIO 时钟 */
    PORT_RCC_CLK(HW_LCD12864_GB2312_MOSI);
    PORT_RCC_CLK(HW_GB2312_MISO);
    PORT_RCC_CLK(HW_LCD12864_GB2312_SCK);
    PORT_RCC_CLK(HW_GB2312_CS);

    /* MOSI → 推挽输出，初始低电平 */
    PORT_SET_DIR_PP(HW_LCD12864_GB2312_MOSI);
    PORT_OUT(HW_LCD12864_GB2312_MOSI) = 0;

    /* SCK → 推挽输出，初始低电平（SPI Mode 0/3 兼容） */
    PORT_SET_DIR_PP(HW_LCD12864_GB2312_SCK);
    PORT_OUT(HW_LCD12864_GB2312_SCK) = 0;

    /* CS → 推挽输出，高电平释放片选 */
    PORT_SET_DIR_PP(HW_GB2312_CS);
    PORT_OUT(HW_GB2312_CS) = 1;

    /* MISO → 浮空输入 */
    PORT_SET_DIR_IN_FLOAT(HW_GB2312_MISO);
}

/*******************************************************************************
 * @brief  软件 SPI 读写一个字节（MSB first）
 * @param  data : 要发送的字节
 * @return 接收到的字节
 * @note
 *         时序：SCK 空闲低电平，上升沿字库芯片锁存 MOSI，下降沿字库输出 MISO
 *                ┌──┐  ┌──┐  ┌──┐         ┌──┐
 *         SCK ──┘  └──┘  └──┘  └── ... ──┘  └──
 *                ↑        ↑                   ↑
 *               MOSI置位  MISO采样         最后1bit
 ******************************************************************************/
uint8_t GB2312_SPI_ReadByte(uint8_t data)
{
    uint8_t i;
    uint8_t rx = 0;

    for(i = 0; i < 8; i++)
    {
        /* 设置 MOSI 数据位 */
        if(data & 0x80)
            PORT_OUT(HW_LCD12864_GB2312_MOSI) = 1;
        else
            PORT_OUT(HW_LCD12864_GB2312_MOSI) = 0;
        data <<= 1;

        spi_delay();

        /* SCK ↑ 上升沿：字库芯片锁存 MOSI */
        PORT_OUT(HW_LCD12864_GB2312_SCK) = 1;

        spi_delay();

        /* 在 SCK 高电平期间采样 MISO */
        rx <<= 1;
        if(PORT_IN(HW_GB2312_MISO))
            rx |= 0x01;

        /* SCK ↓ 下降沿：字库芯片切换下一个 MISO 位 */
        PORT_OUT(HW_LCD12864_GB2312_SCK) = 0;
    }

    return rx;
}

/*******************************************************************************
 * @brief  从字库芯片指定地址连续读取 len 个字节
 * @param  addr : 24 位字节地址（从 0 开始）
 * @param  buf  : 接收缓冲区
 * @param  len  : 要读取的字节数
 * @note
 *         命令序列：
 *         CS=L → 0x03(READ) → Addr[23:16] → Addr[15:8] → Addr[7:0]
 *              → 读取 Data[0] ~ Data[len-1] → CS=H
 ******************************************************************************/
void GB2312_SPI_ReadData(uint32_t addr, uint8_t *buf, uint16_t len)
{
    uint16_t i;

    /* 片选使能 */
    PORT_OUT(HW_GB2312_CS) = 0;
    spi_delay();

    /* 发送 READ 指令 (0x03) */
    GB2312_SPI_ReadByte(0x03);

    /* 发送 24 位地址（高位→中位→低位） */
    GB2312_SPI_ReadByte((uint8_t)(addr >> 16));   // A[23:16]
    GB2312_SPI_ReadByte((uint8_t)(addr >> 8));    // A[15:8]
    GB2312_SPI_ReadByte((uint8_t)(addr));          // A[7:0]

    /* 连续读取数据 */
    for(i = 0; i < len; i++)
    {
        buf[i] = GB2312_SPI_ReadByte(0xFF);   // 发送 dummy 0xFF，读取返回数据
    }

    /* 片选释放 */
    spi_delay();
    PORT_OUT(HW_GB2312_CS) = 1;
}

/*******************************************************************************
 * @brief  获取 GB2312 汉字（15×16 点阵）字模数据
 * @param  msb : GB2312 内码高字节
 * @param  lsb : GB2312 内码低字节
 * @param  buf : 输出缓冲区，至少 32 字节
 * @return 0=成功，1=非法编码
 * @note
 *         汉字地址公式：
 *         - 一级/二级汉字 (0xB0~0xF7,0xA1~0xFE)：
 *           Address = ((MSB-0xB0)*94 + (LSB-0xA1) + 846) * 32
 *         - 符号区 (0xA1~0xA9,0xA1~0xFE)：
 *           Address = ((MSB-0xA1)*94 + (LSB-0xA1)) * 32
 ******************************************************************************/
uint8_t GB2312_GetChnFont(uint8_t msb, uint8_t lsb, uint8_t *buf)
{
    uint32_t addr;

    /* GB2312 一级+二级汉字：0xB0~0xF7 区 */
    if((msb >= 0xB0) && (msb <= 0xF7) && (lsb >= 0xA1))
    {
        addr = (uint32_t)(msb - 0xB0) * 94;
        addr += (uint32_t)(lsb - 0xA1) + 846;
    }
    /* GB2312 符号区：0xA1~0xA3 区（3个区，每区94个字符，共282个） */
    else if((msb >= 0xA1) && (msb <= 0xA3) && (lsb >= 0xA1))
    {
        addr = (uint32_t)(msb - 0xA1) * 94;
        addr += (uint32_t)(lsb - 0xA1);
    }
    /* 0xA9 区特殊符号（偏移 282 = 3×94 个字符） */
    else if((msb == 0xA9) && (lsb >= 0xA1))
    {
        addr = 282 + (uint32_t)(lsb - 0xA1);
    }
    else
    {
        return 1;   // 非法编码
    }

    addr = addr * 32;                   // 每个汉字占 32 字节
    GB2312_SPI_ReadData(addr, buf, 32);

    return 0;
}

/*******************************************************************************
 * @brief  获取 8×16 点阵 ASCII 粗体字符字模
 * @param  ascii : ASCII 码 (0x20 ~ 0x7E)
 * @param  buf   : 输出缓冲区，至少 16 字节
 * @return 0=成功，1=非法编码
 * @note
 *         地址公式：
 *         Address = (ASCII - 0x20) * 16 + 0x3CF80
 ******************************************************************************/
uint8_t GB2312_GetAscii8x16(uint8_t ascii, uint8_t *buf)
{
    uint32_t addr;

    if((ascii >= 0x20) && (ascii <= 0x7E))
    {
        addr = (uint32_t)(ascii - 0x20) * 16;
        addr += GB2312_ASC8x16_BASE;

        GB2312_SPI_ReadData(addr, buf, 16);
        return 0;
    }

    return 1;   // 非法编码
}

/*******************************************************************************
 * @brief  获取 5×7 点阵 ASCII 字符字模
 * @param  ascii : ASCII 码 (0x20 ~ 0x7E)
 * @param  buf   : 输出缓冲区，至少 8 字节
 * @return 0=成功，1=非法编码
 * @note
 *         地址公式：
 *         Address = (ASCII - 0x20) * 8 + 0x3BFC0
 ******************************************************************************/
uint8_t GB2312_GetAscii5x7(uint8_t ascii, uint8_t *buf)
{
    uint32_t addr;

    if((ascii >= 0x20) && (ascii <= 0x7E))
    {
        addr = (uint32_t)(ascii - 0x20) * 8;
        addr += 0x3BFC0UL;

        GB2312_SPI_ReadData(addr, buf, 8);
        return 0;
    }

    return 1;
}

/*******************************************************************************
 * @brief  获取 7×8 点阵 ASCII 字符字模
 * @param  ascii : ASCII 码 (0x20 ~ 0x7E)
 * @param  buf   : 输出缓冲区，至少 8 字节
 * @return 0=成功，1=非法编码
 * @note
 *         地址公式：
 *         Address = (ASCII - 0x20) * 8 + 0x66C0
 ******************************************************************************/
uint8_t GB2312_GetAscii7x8(uint8_t ascii, uint8_t *buf)
{
    uint32_t addr;

    if((ascii >= 0x20) && (ascii <= 0x7E))
    {
        addr = (uint32_t)(ascii - 0x20) * 8;
        addr += 0x66C0UL;

        GB2312_SPI_ReadData(addr, buf, 8);
        return 0;
    }

    return 1;
}

/*******************************************************************************
 * @brief  获取 8×16 点阵国标扩展字符字模（内码 0xAAA1~0xABC0）
 * @param  fontcode : 字符内码（16 位）
 * @param  buf      : 输出缓冲区，至少 16 字节
 * @return 0=成功，1=非法编码
 * @note
 *         地址公式：
 *         BaseAdd = 0x3B7D0
 *         if (0xAAA1~0xAAFE) : Address = (FontCode-0xAAA1)*16 + BaseAdd
 *         if (0xABA1~0xABC0) : Address = (FontCode-0xABA1+95)*16 + BaseAdd
 ******************************************************************************/
uint8_t GB2312_GetExtChar(uint16_t fontcode, uint8_t *buf)
{
    uint32_t addr;

    if((fontcode >= 0xAAA1) && (fontcode <= 0xAAFE))
    {
        addr = (uint32_t)(fontcode - 0xAAA1) * 16;
    }
    else if((fontcode >= 0xABA1) && (fontcode <= 0xABC0))
    {
        addr = ((uint32_t)(fontcode - 0xABA1) + 95) * 16;
    }
    else
    {
        return 1;
    }

    addr += 0x3B7D0UL;
    GB2312_SPI_ReadData(addr, buf, 16);

    return 0;
}

