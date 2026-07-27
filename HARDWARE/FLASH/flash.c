/*
 * SPI NOR Flash 驱动模块
 * 基于 STM32F103VET6 的 SPI2 外设
 * 支持 W25X/W25Q 系列 SPI Flash 芯片
 * 提供轮询和 DMA 两种传输方式
 */

#include "flash.h"
#include "spi.h"
#include "delay.h"
#include <string.h>
#include <stdio.h>

u16 SPI_FLASH_TYPE = 0;                 /* Flash 芯片型号（检测后赋值） */
static u8 SPI_FLASH_BUFFER[FLASH_SECTOR_SIZE];  /* 读-改-写缓冲区（大小=1扇区） */

/*
 * SPI_Flash_ClipLength - 截断长度以不超过芯片容量边界
 * addr: 起始地址
 * len:  请求长度
 * 返回值: 实际可用长度（超出容量则返回0或截断值）
 */
static u16 SPI_Flash_ClipLength(u32 addr, u16 len)
{
    u32 remain;

    if (addr >= FLASH_CAPACITY)
        return 0;                       /* 起始地址超出容量，返回0 */

    remain = FLASH_CAPACITY - addr;     /* 剩余可用空间 */
    if ((u32)len > remain)
        len = (u16)remain;              /* 截断到可用空间长度 */

    return len;
}

/*
 * SPI_Flash_Init - 初始化 SPI Flash 模块
 * 配置 CS 和 WP 引脚为 GPIO 输出
 * 初始化 SPI2 外设
 * 读取并保存 Flash 芯片 ID
 */
void SPI_Flash_Init(void)
{
    u16 id;

    /* 使能 CS 和 WP 引脚的 GPIO 时钟 */
    PORT_RCC_CLK(HW_FLASH_CS);
    PORT_RCC_CLK(HW_FLASH_WP);

    /* 配置 CS 和 WP 为推挽输出 */
    PORT_SET_DIR_PP(HW_FLASH_CS);
    PORT_SET_DIR_PP(HW_FLASH_WP);

    FLASH_CS_H();           /* CS 初始为高（不选中） */
    FLASH_WP_H();           /* WP 初始为高（禁止写保护） */

    SPI2_Init();            /* 初始化 SPI2 */
    SPI2_SetSpeed(SPI_SPEED_2); /* 设置 SPI 时钟为低速（约 9MHz/2） */

    /* 读取芯片 ID 以确认 Flash 是否正常工作 */
    id = SPI_Flash_ReadID();
    if (id == 0x0000 || id == 0xFFFF)   /* ID 无效时尝试读 JEDEC ID */
        id = (u16)(SPI_Flash_ReadJEDECID() & 0xFFFFU);

    SPI_FLASH_TYPE = id;    /* 保存芯片型号 */
}

/*
 * SPI_Flash_ReadSR - 读状态寄存器
 * 返回值: 状态寄存器值
 *   bit0: BUSY（1=忙）
 *   bit1: WEL（写使能锁存）
 */
u8 SPI_Flash_ReadSR(void)
{
    u8 byte;

    FLASH_CS_L();                       /* 选中芯片 */
    SPI2_ReadWriteByte(W25X_ReadStatusReg); /* 发送读状态寄存器指令 */
    byte = SPI2_ReadWriteByte(0xFF);    /* 读取状态寄存器值 */
    FLASH_CS_H();                       /* 取消选中 */

    return byte;
}

/*
 * SPI_FLASH_Write_SR - 写状态寄存器
 * sr: 要写入的状态寄存器值
 */
void SPI_FLASH_Write_SR(u8 sr)
{
    SPI_FLASH_Write_Enable();           /* 写入状态寄存器前需要写使能 */
    FLASH_CS_L();
    SPI2_ReadWriteByte(W25X_WriteStatusReg);
    SPI2_ReadWriteByte(sr);
    FLASH_CS_H();
    SPI_Flash_Wait_Busy();              /* 等待操作完成 */
    SPI_FLASH_Write_Disable();
}

/*
 * SPI_FLASH_Write_Enable - 写使能（发送 WREN 指令）
 * 在每次写入或擦除操作前必须调用
 */
void SPI_FLASH_Write_Enable(void)
{
    FLASH_CS_L();
    SPI2_ReadWriteByte(W25X_WriteEnable);
    FLASH_CS_H();
}

/*
 * SPI_FLASH_Write_Disable - 写禁止（发送 WRDI 指令）
 */
void SPI_FLASH_Write_Disable(void)
{
    FLASH_CS_L();
    SPI2_ReadWriteByte(W25X_WriteDisable);
    FLASH_CS_H();
}

/*
 * SPI_Flash_ReadID - 读厂商/设备 ID（双字节）
 * 返回值: (厂商ID << 8) | 设备ID
 */
u16 SPI_Flash_ReadID(void)
{
    u16 temp;

    FLASH_CS_L();
    SPI2_ReadWriteByte(W25X_ManufactDeviceID);  /* 读厂商/设备 ID 指令 */
    SPI2_ReadWriteByte(0x00);                   /* 3字节地址（全0） */
    SPI2_ReadWriteByte(0x00);
    SPI2_ReadWriteByte(0x00);
    temp  = (u16)SPI2_ReadWriteByte(0xFF) << 8; /* 读厂商 ID */
    temp |= SPI2_ReadWriteByte(0xFF);           /* 读设备 ID */
    FLASH_CS_H();

    return temp;
}

/*
 * SPI_Flash_ReadJEDECID - 读 JEDEC ID（三字节）
 * 返回值: 厂商ID(高8位) | 内存类型(中8位) | 容量(低8位)
 */
u32 SPI_Flash_ReadJEDECID(void)
{
    u32 id;

    FLASH_CS_L();
    SPI2_ReadWriteByte(W25X_JedecDeviceID);     /* 读 JEDEC ID 指令 */
    id  = (u32)SPI2_ReadWriteByte(0xFF) << 16;  /* 厂商 ID */
    id |= (u32)SPI2_ReadWriteByte(0xFF) << 8;   /* 内存类型 */
    id |= (u32)SPI2_ReadWriteByte(0xFF);        /* 容量 */
    FLASH_CS_H();

    return id;
}

/*
 * SPI_Flash_Wait_Busy - 等待 Flash 忙状态结束
 * 轮询状态寄存器的 BUSY 位（bit0），直到为0
 */
void SPI_Flash_Wait_Busy(void)
{
    while ((SPI_Flash_ReadSR() & 0x01U) != 0)
    {
        /* 等待 Flash 内部操作完成 */
    }
}

/*
 * SPI_Flash_Read - 从 Flash 读取数据
 * pBuffer:    输出缓冲区
 * ReadAddr:   读取起始地址（0~最大容量-1）
 * NumByteToRead: 要读取的字节数
 */
void SPI_Flash_Read(u8 *pBuffer, u32 ReadAddr, u16 NumByteToRead)
{
    if (pBuffer == 0)
        return;

    NumByteToRead = SPI_Flash_ClipLength(ReadAddr, NumByteToRead);
    if (NumByteToRead == 0)
        return;

    FLASH_CS_L();
    SPI2_ReadWriteByte(W25X_ReadData);                      /* 读数据指令 */
    SPI2_ReadWriteByte((u8)(ReadAddr >> 16));               /* 地址高8位 */
    SPI2_ReadWriteByte((u8)(ReadAddr >> 8));                /* 地址中8位 */
    SPI2_ReadWriteByte((u8)ReadAddr);                       /* 地址低8位 */
    SPI2_ReadBuf(pBuffer, NumByteToRead);                   /* 连续读取数据 */
    FLASH_CS_H();
}

/*
 * SPI_Flash_Write_Page - 写一页数据（最大 256 字节）
 * pBuffer:       数据源缓冲区
 * WriteAddr:     写入起始地址（需在页内对齐）
 * NumByteToWrite: 要写入的字节数（不超过页剩余空间）
 */
void SPI_Flash_Write_Page(const u8 *pBuffer, u32 WriteAddr, u16 NumByteToWrite)
{
    u16 pageRemain;

    if (pBuffer == 0)
        return;

    NumByteToWrite = SPI_Flash_ClipLength(WriteAddr, NumByteToWrite);
    if (NumByteToWrite == 0)
        return;

    /* 计算当前页内剩余空间 */
    pageRemain = (u16)(FLASH_PAGE_SIZE - (WriteAddr & (FLASH_PAGE_SIZE - 1UL)));
    if (NumByteToWrite > pageRemain)
        NumByteToWrite = pageRemain;    /* 不跨页 */

    SPI_FLASH_Write_Enable();           /* 写使能 */
    FLASH_CS_L();
    SPI2_ReadWriteByte(W25X_PageProgram);   /* 页编程指令 */
    SPI2_ReadWriteByte((u8)(WriteAddr >> 16));  /* 地址高8位 */
    SPI2_ReadWriteByte((u8)(WriteAddr >> 8));   /* 地址中8位 */
    SPI2_ReadWriteByte((u8)WriteAddr);          /* 地址低8位 */
    SPI2_WriteBuf(pBuffer, NumByteToWrite);     /* 连续写入数据 */
    FLASH_CS_H();
    SPI_Flash_Wait_Busy();              /* 等待编程完成 */
    SPI_FLASH_Write_Disable();
}

/*
 * SPI_Flash_Write_NoCheck - 连续写入多页数据（不检查是否需要擦除）
 * 假设目标地址已擦除（全0xFF），直接写入
 */
void SPI_Flash_Write_NoCheck(const u8 *pBuffer, u32 WriteAddr, u16 NumByteToWrite)
{
    u16 pageRemain;

    if (pBuffer == 0)
        return;

    NumByteToWrite = SPI_Flash_ClipLength(WriteAddr, NumByteToWrite);
    if (NumByteToWrite == 0)
        return;

    pageRemain = (u16)(FLASH_PAGE_SIZE - (WriteAddr % FLASH_PAGE_SIZE));
    if (NumByteToWrite <= pageRemain)
        pageRemain = NumByteToWrite;

    while (1)
    {
        SPI_Flash_Write_Page(pBuffer, WriteAddr, pageRemain);
        if (NumByteToWrite == pageRemain)
            break;                      /* 写完退出 */

        pBuffer += pageRemain;
        WriteAddr += pageRemain;
        NumByteToWrite -= pageRemain;

        if (NumByteToWrite > FLASH_PAGE_SIZE)
            pageRemain = FLASH_PAGE_SIZE;
        else
            pageRemain = NumByteToWrite;
    }
}

/*
 * SPI_Flash_Write - 写入数据（含扇区擦除与读-改-写操作）
 * 自动处理跨扇区写，目标扇区若非全0xFF则先擦除再写入
 */
void SPI_Flash_Write(const u8 *pBuffer, u32 WriteAddr, u16 NumByteToWrite)
{
    u32 secpos;
    u32 sectorAddr;
    u16 secoff;
    u16 secremain;
    u16 i;

    if (pBuffer == 0)
        return;

    NumByteToWrite = SPI_Flash_ClipLength(WriteAddr, NumByteToWrite);
    if (NumByteToWrite == 0)
        return;

    secpos    = WriteAddr / FLASH_SECTOR_SIZE;      /* 起始扇区号 */
    secoff    = (u16)(WriteAddr % FLASH_SECTOR_SIZE);/* 扇区内偏移 */
    secremain = (u16)(FLASH_SECTOR_SIZE - secoff);  /* 当前扇区剩余空间 */
    if (NumByteToWrite <= secremain)
        secremain = NumByteToWrite;

    while (1)
    {
        sectorAddr = secpos * FLASH_SECTOR_SIZE;
        /* 读取整个扇区到缓冲区 */
        SPI_Flash_Read(SPI_FLASH_BUFFER, sectorAddr, FLASH_SECTOR_SIZE);

        /* 检查目标区域是否已是全0xFF（无需擦除） */
        for (i = 0; i < secremain; i++)
        {
            if (SPI_FLASH_BUFFER[secoff + i] != 0xFF)
                break;
        }

        if (i < secremain)
        {
            /* 需要擦除：先擦除扇区，再更新缓冲区内容，最后写入 */
            SPI_Flash_Erase_Sector(secpos);
            for (i = 0; i < secremain; i++)
                SPI_FLASH_BUFFER[secoff + i] = pBuffer[i];

            SPI_Flash_Write_NoCheck(SPI_FLASH_BUFFER, sectorAddr, FLASH_SECTOR_SIZE);
        }
        else
        {
            /* 目标区域已是空（0xFF），直接写入 */
            SPI_Flash_Write_NoCheck(pBuffer, WriteAddr, secremain);
        }

        if (NumByteToWrite == secremain)
            break;          /* 全部写完 */

        secpos++;           /* 进入下一个扇区 */
        secoff = 0;
        pBuffer += secremain;
        WriteAddr += secremain;
        NumByteToWrite -= secremain;

        if (NumByteToWrite > FLASH_SECTOR_SIZE)
            secremain = FLASH_SECTOR_SIZE;
        else
            secremain = NumByteToWrite;
    }
}

/*
 * SPI_Flash_Erase_Chip - 整片擦除
 * 将所有存储单元擦除为 0xFF
 * 耗时较长（通常数秒）
 */
void SPI_Flash_Erase_Chip(void)
{
    SPI_FLASH_Write_Enable();
    SPI_Flash_Wait_Busy();

    FLASH_CS_L();
    SPI2_ReadWriteByte(W25X_ChipErase);     /* 整片擦除指令 */
    FLASH_CS_H();

    SPI_Flash_Wait_Busy();
    SPI_FLASH_Write_Disable();
}

/*
 * SPI_Flash_Erase_Sector - 擦除指定扇区（4KB）
 * sectorIndex: 扇区索引号（0~最大扇区数-1）
 */
void SPI_Flash_Erase_Sector(u32 sectorIndex)
{
    u32 addr;

    addr = sectorIndex * FLASH_SECTOR_SIZE;     /* 计算扇区起始地址 */

    SPI_FLASH_Write_Enable();
    SPI_Flash_Wait_Busy();

    FLASH_CS_L();
    SPI2_ReadWriteByte(W25X_SectorErase);       /* 扇区擦除指令 */
    SPI2_ReadWriteByte((u8)(addr >> 16));
    SPI2_ReadWriteByte((u8)(addr >> 8));
    SPI2_ReadWriteByte((u8)addr);
    FLASH_CS_H();

    SPI_Flash_Wait_Busy();
    SPI_FLASH_Write_Disable();
}

/*
 * SPI_Flash_Erase_Block - 擦除指定块（64KB）
 * blockIndex: 块索引号（0~最大块数-1）
 */
void SPI_Flash_Erase_Block(u32 blockIndex)
{
    u32 addr;

    addr = blockIndex * FLASH_BLOCK_SIZE;       /* 计算块起始地址 */

    SPI_FLASH_Write_Enable();
    SPI_Flash_Wait_Busy();

    FLASH_CS_L();
    SPI2_ReadWriteByte(W25X_BlockErase);        /* 块擦除指令 */
    SPI2_ReadWriteByte((u8)(addr >> 16));
    SPI2_ReadWriteByte((u8)(addr >> 8));
    SPI2_ReadWriteByte((u8)addr);
    FLASH_CS_H();

    SPI_Flash_Wait_Busy();
    SPI_FLASH_Write_Disable();
}

/*
 * SPI_Flash_PowerDown - 进入掉电模式
 * 进入低功耗状态，需要调用 WAKEUP 恢复
 */
void SPI_Flash_PowerDown(void)
{
    FLASH_CS_L();
    SPI2_ReadWriteByte(W25X_PowerDown);         /* 掉电指令 */
    FLASH_CS_H();
    delay_us(3);
}

/*
 * SPI_Flash_WAKEUP - 从掉电模式唤醒
 */
void SPI_Flash_WAKEUP(void)
{
    FLASH_CS_L();
    SPI2_ReadWriteByte(W25X_ReleasePowerDown);  /* 释放掉电指令 */
    FLASH_CS_H();
    delay_us(3);
}

/*
 * SPI_Flash_WP_Set - 设置写保护引脚状态
 * enable: 1=写保护使能, 0=写保护禁止
 */
void SPI_Flash_WP_Set(u8 enable)
{
    if (enable)
        FLASH_WP_L();       /* WP=0 使能硬件写保护 */
    else
        FLASH_WP_H();       /* WP=1 禁止硬件写保护 */
}

/*====================================================================
 * SPI2 DMA 传输函数
 * STM32F103 常规映射:
 *   DMA1_Channel4: SPI2_RX（外设 -> 内存）
 *   DMA1_Channel5: SPI2_TX（内存 -> 外设）
 * SPI 外设按 8 位数据帧工作，因此 DMA 也必须配置成 8 位宽度。
 *
 * 这里同时提供两类接口：
 *   1. Start + IsFinished: 非阻塞接口，用于乒乓缓冲/流水线读取。
 *   2. SPI_Flash_Read_DMA / SPI_Flash_Write_Page_DMA: 兼容旧代码的阻塞接口。
 *====================================================================*/

#define SPI_FLASH_DMA_RX_CH     DMA1_Channel4
#define SPI_FLASH_DMA_TX_CH     DMA1_Channel5
#define SPI_FLASH_DMA_TC_FLAGS  (DMA_ISR_TCIF4 | DMA_ISR_TCIF5)
#define SPI_FLASH_DMA_CLR_FLAGS (DMA_IFCR_CTCIF4 | DMA_IFCR_CTCIF5)

#define SPI_FLASH_DMA_IDLE      0U
#define SPI_FLASH_DMA_READ      1U
#define SPI_FLASH_DMA_WRITE_DMA 2U
#define SPI_FLASH_DMA_WRITE_BUSY 3U

static u8 s_flashDmaTxDummy = 0xFFU;
static u8 s_flashDmaRxDummy;
static volatile u8 s_flashDmaState = SPI_FLASH_DMA_IDLE;

/*
 * SPI_Flash_DMA_Init - 初始化 DMA1_Ch4/Ch5 用于 SPI2 传输
 * 这里只配置固定外设地址，具体方向、地址递增和长度在每次传输前设置。
 */
void SPI_Flash_DMA_Init(void)
{
    /* 使能 DMA1 时钟（AHB） */
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    delay_ms(1);

    SPI_FLASH_DMA_RX_CH->CCR = 0;
    SPI_FLASH_DMA_TX_CH->CCR = 0;
    SPI_FLASH_DMA_RX_CH->CPAR = (u32)&SPI2->DR;
    SPI_FLASH_DMA_TX_CH->CPAR = (u32)&SPI2->DR;
    DMA1->IFCR = SPI_FLASH_DMA_CLR_FLAGS;
    s_flashDmaState = SPI_FLASH_DMA_IDLE;
}

/* 检查 RX/TX 两个 DMA 通道是否都完成。 */
static u8 SPI_Flash_DMA_TransferDone(void)
{
    return ((DMA1->ISR & SPI_FLASH_DMA_TC_FLAGS) == SPI_FLASH_DMA_TC_FLAGS) ? 1U : 0U;
}

/* DMA 数据阶段完成后的公共收尾。 */
static void SPI_Flash_DMA_StopTransfer(void)
{
    while((SPI2->SR & SPI_SR_BSY) != 0)
    {
    }

    SPI_FLASH_DMA_RX_CH->CCR &= (u16)~DMA_CCR1_EN;
    SPI_FLASH_DMA_TX_CH->CCR &= (u16)~DMA_CCR1_EN;
    SPI2->CR2 &= (u16)~(SPI_CR2_RXDMAEN | SPI_CR2_TXDMAEN);
    DMA1->IFCR = SPI_FLASH_DMA_CLR_FLAGS;

    /* 清掉可能残留的接收数据，避免后续轮询传输读到旧字节。 */
    if ((SPI2->SR & SPI_SR_RXNE) != 0)
        (void)*(__IO u8 *)&SPI2->DR;
    (void)SPI2->SR;
}

/* 配置并启动 SPI2 的 RX/TX DMA 数据阶段。 */
static void SPI_Flash_DMA_StartTransfer(u8 *rxBuf,
                                        const u8 *txBuf,
                                        u16 len,
                                        u8 rxInc,
                                        u8 txInc)
{
    SPI_FLASH_DMA_RX_CH->CCR &= (u16)~DMA_CCR1_EN;
    SPI_FLASH_DMA_TX_CH->CCR &= (u16)~DMA_CCR1_EN;
    DMA1->IFCR = SPI_FLASH_DMA_CLR_FLAGS;

    SPI_FLASH_DMA_RX_CH->CMAR = (u32)rxBuf;
    SPI_FLASH_DMA_RX_CH->CNDTR = len;
    SPI_FLASH_DMA_RX_CH->CCR = DMA_CCR1_PL_1 | (rxInc ? DMA_CCR1_MINC : 0U);

    SPI_FLASH_DMA_TX_CH->CMAR = (u32)txBuf;
    SPI_FLASH_DMA_TX_CH->CNDTR = len;
    SPI_FLASH_DMA_TX_CH->CCR = DMA_CCR1_DIR | DMA_CCR1_PL_1 | (txInc ? DMA_CCR1_MINC : 0U);

    SPI2->CR2 |= SPI_CR2_RXDMAEN | SPI_CR2_TXDMAEN;

    /* 先开 RX，后开 TX，避免第一个接收字节丢失。 */
    SPI_FLASH_DMA_RX_CH->CCR |= DMA_CCR1_EN;
    SPI_FLASH_DMA_TX_CH->CCR |= DMA_CCR1_EN;
}

/*
 * SPI_Flash_Read_DMA_Start - 启动一次 DMA 读数据
 * 返回 1 表示启动成功，返回 0 表示参数错误或 DMA 正忙。
 */
u8 SPI_Flash_Read_DMA_Start(u8 *pBuffer, u32 ReadAddr, u16 NumByteToRead)
{
    if(pBuffer == 0) return 0U;
    if(s_flashDmaState != SPI_FLASH_DMA_IDLE) return 0U;

    NumByteToRead = SPI_Flash_ClipLength(ReadAddr, NumByteToRead);
    if(NumByteToRead == 0) return 0U;

    FLASH_CS_L();
    SPI2_ReadWriteByte(W25X_ReadData);
    SPI2_ReadWriteByte((u8)(ReadAddr >> 16));
    SPI2_ReadWriteByte((u8)(ReadAddr >> 8));
    SPI2_ReadWriteByte((u8)ReadAddr);

    s_flashDmaState = SPI_FLASH_DMA_READ;
    SPI_Flash_DMA_StartTransfer(pBuffer, &s_flashDmaTxDummy, NumByteToRead, 1U, 0U);
    return 1U;
}

/*
 * SPI_Flash_Read_DMA_IsFinished - 查询 DMA 读是否完成
 * 返回 1 表示已经完成并完成收尾，返回 0 表示仍在传输。
 */
u8 SPI_Flash_Read_DMA_IsFinished(void)
{
    if(s_flashDmaState != SPI_FLASH_DMA_READ)
        return 1U;

    if(!SPI_Flash_DMA_TransferDone())
        return 0U;

    SPI_Flash_DMA_StopTransfer();
    FLASH_CS_H();
    s_flashDmaState = SPI_FLASH_DMA_IDLE;
    return 1U;
}

/*
 * SPI_Flash_Read_DMA - DMA 方式读取 Flash 数据（阻塞兼容接口）
 */
void SPI_Flash_Read_DMA(u8 *pBuffer, u32 ReadAddr, u16 NumByteToRead)
{
    if(SPI_Flash_Read_DMA_Start(pBuffer, ReadAddr, NumByteToRead) == 0U)
        return;

    while(SPI_Flash_Read_DMA_IsFinished() == 0U)
    {
    }
}

/*
 * SPI_Flash_Write_Page_DMA_Start - 启动一次页内 DMA 写数据
 * 返回 1 表示启动成功，返回 0 表示参数错误或 DMA 正忙。
 * 注意：该函数只允许写同一页内的数据，超出页尾会自动裁剪。
 */
u8 SPI_Flash_Write_Page_DMA_Start(const u8 *pBuffer, u32 WriteAddr, u16 NumByteToWrite)
{
    u16 pageRemain;

    if(pBuffer == 0) return 0U;
    if(s_flashDmaState != SPI_FLASH_DMA_IDLE) return 0U;

    NumByteToWrite = SPI_Flash_ClipLength(WriteAddr, NumByteToWrite);
    if(NumByteToWrite == 0) return 0U;

    pageRemain = (u16)(FLASH_PAGE_SIZE - (WriteAddr & (FLASH_PAGE_SIZE - 1UL)));
    if(NumByteToWrite > pageRemain) NumByteToWrite = pageRemain;

    SPI_FLASH_Write_Enable();

    FLASH_CS_L();
    SPI2_ReadWriteByte(W25X_PageProgram);
    SPI2_ReadWriteByte((u8)(WriteAddr >> 16));
    SPI2_ReadWriteByte((u8)(WriteAddr >> 8));
    SPI2_ReadWriteByte((u8)WriteAddr);

    s_flashDmaState = SPI_FLASH_DMA_WRITE_DMA;
    SPI_Flash_DMA_StartTransfer(&s_flashDmaRxDummy, pBuffer, NumByteToWrite, 0U, 1U);
    return 1U;
}

/*
 * SPI_Flash_Write_Page_DMA_IsFinished - 查询 DMA 页写是否完成
 * 返回 1 表示 DMA 数据阶段和 Flash 内部页编程都已完成，返回 0 表示仍在忙。
 */
u8 SPI_Flash_Write_Page_DMA_IsFinished(void)
{
    if(s_flashDmaState == SPI_FLASH_DMA_IDLE)
        return 1U;

    if(s_flashDmaState == SPI_FLASH_DMA_WRITE_DMA)
    {
        if(!SPI_Flash_DMA_TransferDone())
            return 0U;

        SPI_Flash_DMA_StopTransfer();
        FLASH_CS_H();
        s_flashDmaState = SPI_FLASH_DMA_WRITE_BUSY;
    }

    if(s_flashDmaState == SPI_FLASH_DMA_WRITE_BUSY)
    {
        if((SPI_Flash_ReadSR() & 0x01U) != 0U)
            return 0U;

        SPI_FLASH_Write_Disable();
        s_flashDmaState = SPI_FLASH_DMA_IDLE;
        return 1U;
    }

    return 0U;
}

/*
 * SPI_Flash_Write_Page_DMA - DMA 方式写一页数据（阻塞兼容接口）
 */
void SPI_Flash_Write_Page_DMA(const u8 *pBuffer, u32 WriteAddr, u16 NumByteToWrite)
{
    if(SPI_Flash_Write_Page_DMA_Start(pBuffer, WriteAddr, NumByteToWrite) == 0U)
        return;

    while(SPI_Flash_Write_Page_DMA_IsFinished() == 0U)
    {
    }
}

/*
 * SPI_Flash_DebugDemo - SPI Flash 调试示例
 * 可在 main() 完成 USART1 初始化后手动调用。
 * 示例流程：
 * 1. 读取芯片 ID
 * 2. 擦除首扇区
 * 3. 写入测试数据
 * 4. 回读并比较
 * 5. 整片擦除后验证
 * 每一步都会通过 USART1 打印调试信息。
 */
void SPI_Flash_DebugDemo(void)
{
    static const u8 txBuf[16] =
    {
        0x46, 0x4C, 0x41, 0x53, 0x48, 0x5F, 0x44, 0x45,
        0x4D, 0x4F, 0x5F, 0x31, 0x30, 0x33, 0x56, 0x45
    };
    u8 rxBuf[sizeof(txBuf)];
    u16 flashId;
    u8 compareOk;
    u16 i;

    memset(rxBuf, 0, sizeof(rxBuf));
    flashId = 0;
    compareOk = 0;

    /* 步骤1: 初始化 SPI Flash */
    printf("【Flash调试】开始 SPI Flash 调试...\r\n");
    SPI_Flash_Init();
    printf("【Flash调试】初始化完成\r\n");

    /* 步骤2: 读取芯片 ID */
    flashId = SPI_Flash_ReadID();
    if (flashId == 0x0000U || flashId == 0xFFFFU)
        flashId = (u16)(SPI_Flash_ReadJEDECID() & 0xFFFFU);
    printf("【Flash调试】芯片 ID = 0x%04X\r\n", flashId);

    /* 步骤3: 擦除首扇区（4KB） */
    printf("【Flash调试】擦除扇区 0 ...\r\n");
    SPI_Flash_Erase_Sector(0);
    printf("【Flash调试】扇区擦除完成\r\n");

    /* 步骤4: 写入 16 字节测试数据到地址 0x00000000 */
    printf("【Flash调试】写入数据到地址 0x00000000: ");
    for (i = 0; i < sizeof(txBuf); i++)
        printf("%02X ", txBuf[i]);
    printf("\r\n");
    SPI_Flash_Write(txBuf, 0x00000000UL, sizeof(txBuf));
    printf("【Flash调试】写入完成\r\n");

    /* 步骤5: 从地址 0x00000000 回读数据 */
    memset(rxBuf, 0, sizeof(rxBuf));
    SPI_Flash_Read(rxBuf, 0x00000000UL, sizeof(rxBuf));
    printf("【Flash调试】回读数据: ");
    for (i = 0; i < sizeof(rxBuf); i++)
        printf("%02X ", rxBuf[i]);
    printf("\r\n");

    /* 步骤6: 比较写入和回读数据 */
    if (memcmp(txBuf, rxBuf, sizeof(txBuf)) == 0)
    {
        compareOk = 1;
        printf("【Flash调试】比较结果: 一致，读写测试通过！\r\n");
    }
    else
    {
        printf("【Flash调试】比较结果: 不一致，读写测试失败！\r\n");
    }

    /* 步骤7: 整片擦除后验证 */
    printf("【Flash调试】开始整片擦除...\r\n");
    SPI_Flash_Erase_Chip();
    printf("【Flash调试】整片擦除完成\r\n");

    memset(rxBuf, 0, sizeof(rxBuf));
    SPI_Flash_Read(rxBuf, 0x00000000UL, sizeof(rxBuf));
    printf("【Flash调试】擦除后读取 0x00000000: ");
    for (i = 0; i < sizeof(rxBuf); i++)
        printf("%02X ", rxBuf[i]);
    printf("\r\n");

    /* 检查擦除后是否全为 0xFF */
    compareOk = 1;
    for (i = 0; i < sizeof(rxBuf); i++)
    {
        if (rxBuf[i] != 0xFF)
        {
            compareOk = 0;
            break;
        }
    }
    if (compareOk != 0U)
        printf("【Flash调试】擦除验证通过，全部为 0xFF\r\n");
    else
        printf("【Flash调试】擦除验证失败，存在非 0xFF 数据\r\n");

    printf("【Flash调试】结束\r\n");

    (void)flashId;
    (void)compareOk;
}

/*
 * SPI_Flash_DebugDemo_DMA - SPI Flash DMA 读写调试测试
 *
 * 测试流程：
 *   1. 初始化 Flash 和 DMA1
 *   2. 擦除指定数量的测试扇区
 *   3. 用 DMA 方式逐页写入（每页使用不同的伪随机数据填充）
 *   4. 用 DMA 方式逐页读取并校验数据
 *   5. 重新擦除，用轮询方式逐页写入（使用另一组不同的数据）
 *   6. 用轮询方式逐页读取并校验
 *   7. 打印耗时和速度对比表格
 *
 * 使用 DWT 数据观察点与跟踪单元的周期计数器（CYCCNT）进行高精度计时，
 * 系统主频 72 MHz 时，每周期约 13.89 ns。
 *
 * 注意：该函数会破坏 Flash 前 N 个扇区的数据，仅用于调试。
 */
void SPI_Flash_DebugDemo_DMA(void)
{
    /* ============================================================
     *  测试参数（可调整）
     *  TEST_SECTOR_CNT × 4 KB = 总测试数据量
     *  默认 10 扇区 = 40 KB = 160 页，兼顾测试充分性与执行时间
     * ============================================================ */
    #define DMA_TEST_SECTOR_CNT     10U
    #define DMA_TEST_PAGE_CNT       (DMA_TEST_SECTOR_CNT * (FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE))
    #define DMA_TEST_TOTAL_BYTES    (DMA_TEST_SECTOR_CNT * FLASH_SECTOR_SIZE)
    #define DMA_TEST_START_ADDR     0UL

    u32        i, j;
    u32        startCyc, endCyc;
    u32        cycDmaWrite, cycDmaRead, cycPollWrite, cycPollRead;
    u8         pageBuf[FLASH_PAGE_SIZE];
    u8         verifyBuf[FLASH_PAGE_SIZE];
    u8         dmaDataOk, pollDataOk;
    u32        dmaWriteKBps, dmaReadKBps, pollWriteKBps, pollReadKBps;

    /* ============================================================
     *  初始化 DWT 周期计数器
     *  DEMCR[24] = TRCENA (Trace Enable)
     *  DWT_CTRL[0] = CYCCNTENA (Cycle Counter Enable)
     * ============================================================ */
    *(__IO u32 *)0xE000EDFC |= (1UL << 24);     /* DEMCR |= TRCENA */
    *(__IO u32 *)0xE0001000 |= (1UL << 0);      /* DWT_CTRL |= CYCCNTENA */
    *(__IO u32 *)0xE0001004  = 0UL;             /* DWT_CYCCNT = 0 */

    dmaDataOk   = 1U;
    pollDataOk  = 1U;

    printf("\r\n========== SPI Flash DMA 调试测试 ==========\r\n");

    /* ============================================================
     *  1. 初始化 Flash & DMA
     * ============================================================ */
    SPI_Flash_Init();
    SPI_Flash_DMA_Init();
    SPI2_SetSpeed(SPI_SPEED_2);     /* 18 MHz — Flash 最高支持 25~50 MHz，选最快分频 */

    printf("Flash ID = 0x%04X\r\n", SPI_FLASH_TYPE);
    printf("测试数据量: %lu 扇区 = %lu KB = %lu 页\r\n",
           (u32)DMA_TEST_SECTOR_CNT,
           (u32)DMA_TEST_TOTAL_BYTES / 1024UL,
           (u32)DMA_TEST_PAGE_CNT);

    /* ============================================================
     *  2. 擦除测试扇区
     * ============================================================ */
    printf("\r\n[擦除测试区域]\r\n");
    for (i = 0U; i < DMA_TEST_SECTOR_CNT; i++)
    {
        SPI_Flash_Erase_Sector(i);
    }
    printf("  已擦除 %lu 个扇区\r\n", (u32)DMA_TEST_SECTOR_CNT);

    /* ============================================================
     *  3. DMA 逐页写入 + 计时
     *     每页填充不同的伪随机数据：pageBuf[j] = (pageIdx * 256 + j) ^ 0xA5
     * ============================================================ */
    printf("\r\n--- [1] DMA 写入 (%lu KB) ---\r\n", (u32)DMA_TEST_TOTAL_BYTES / 1024UL);

    startCyc = *(__IO u32 *)0xE0001004;                 /* DWT_CYCCNT */
    for (i = 0U; i < DMA_TEST_PAGE_CNT; i++)
    {
        for (j = 0U; j < FLASH_PAGE_SIZE; j++)
            pageBuf[j] = (u8)((i * FLASH_PAGE_SIZE + j) ^ 0xA5U);

        SPI_Flash_Write_Page_DMA(pageBuf,
                                 DMA_TEST_START_ADDR + i * FLASH_PAGE_SIZE,
                                 FLASH_PAGE_SIZE);
    }
    endCyc = *(__IO u32 *)0xE0001004;
    cycDmaWrite = endCyc - startCyc;

    printf("  DMA 写入完成, 耗时 %lu 周期\r\n", cycDmaWrite);

    /* ============================================================
     *  4. DMA 逐页读取 + 校验 + 计时
     * ============================================================ */
    printf("--- [2] DMA 读取 + 校验 ---\r\n");

    startCyc = *(__IO u32 *)0xE0001004;
    for (i = 0U; i < DMA_TEST_PAGE_CNT; i++)
    {
        SPI_Flash_Read_DMA(verifyBuf,
                           DMA_TEST_START_ADDR + i * FLASH_PAGE_SIZE,
                           FLASH_PAGE_SIZE);

        for (j = 0U; j < FLASH_PAGE_SIZE; j++)
        {
            u8 exp = (u8)((i * FLASH_PAGE_SIZE + j) ^ 0xA5U);
            if (verifyBuf[j] != exp)
            {
                if (dmaDataOk != 0U)
                {
                    printf("  [DMA] 数据不一致! 页%lu 偏移%lu: 期望0x%02X 实际0x%02X\r\n",
                           i, j, exp, verifyBuf[j]);
                }
                dmaDataOk = 0U;
            }
        }
    }
    endCyc = *(__IO u32 *)0xE0001004;
    cycDmaRead = endCyc - startCyc;

    printf("  DMA 读取完成, 耗时 %lu 周期\r\n", cycDmaRead);
    printf("  DMA 数据校验: %s\r\n", (dmaDataOk != 0U) ? "通过" : "失败");

    /* ============================================================
     *  5. 重新擦除测试扇区，准备轮询测试（使用另一组数据）
     * ============================================================ */
    printf("\r\n[重新擦除测试区域]\r\n");
    for (i = 0U; i < DMA_TEST_SECTOR_CNT; i++)
    {
        SPI_Flash_Erase_Sector(i);
    }
    printf("  已重新擦除 %lu 个扇区\r\n", (u32)DMA_TEST_SECTOR_CNT);

    /* ============================================================
     *  6. 轮询逐页写入 + 计时
     *     使用不同的伪随机种子：pageBuf[j] = (pageIdx * 256 + j) ^ 0x5A
     * ============================================================ */
    printf("\r\n--- [3] 轮询写入 (%lu KB) ---\r\n", (u32)DMA_TEST_TOTAL_BYTES / 1024UL);

    startCyc = *(__IO u32 *)0xE0001004;
    for (i = 0U; i < DMA_TEST_PAGE_CNT; i++)
    {
        for (j = 0U; j < FLASH_PAGE_SIZE; j++)
            pageBuf[j] = (u8)((i * FLASH_PAGE_SIZE + j) ^ 0x5AU);

        SPI_Flash_Write_Page(pageBuf,
                             DMA_TEST_START_ADDR + i * FLASH_PAGE_SIZE,
                             FLASH_PAGE_SIZE);
    }
    endCyc = *(__IO u32 *)0xE0001004;
    cycPollWrite = endCyc - startCyc;

    printf("  轮询写入完成, 耗时 %lu 周期\r\n", cycPollWrite);

    /* ============================================================
     *  7. 轮询逐页读取 + 校验 + 计时
     * ============================================================ */
    printf("--- [4] 轮询读取 + 校验 ---\r\n");

    startCyc = *(__IO u32 *)0xE0001004;
    for (i = 0U; i < DMA_TEST_PAGE_CNT; i++)
    {
        SPI_Flash_Read(verifyBuf,
                       DMA_TEST_START_ADDR + i * FLASH_PAGE_SIZE,
                       FLASH_PAGE_SIZE);

        for (j = 0U; j < FLASH_PAGE_SIZE; j++)
        {
            u8 exp = (u8)((i * FLASH_PAGE_SIZE + j) ^ 0x5AU);
            if (verifyBuf[j] != exp)
            {
                if (pollDataOk != 0U)
                {
                    printf("  [轮询] 数据不一致! 页%lu 偏移%lu: 期望0x%02X 实际0x%02X\r\n",
                           i, j, exp, verifyBuf[j]);
                }
                pollDataOk = 0U;
            }
        }
    }
    endCyc = *(__IO u32 *)0xE0001004;
    cycPollRead = endCyc - startCyc;

    printf("  轮询读取完成, 耗时 %lu 周期\r\n", cycPollRead);
    printf("  轮询数据校验: %s\r\n", (pollDataOk != 0U) ? "通过" : "失败");

    /* ============================================================
     *  8. 计算速度并打印汇总表格
     *     主频 72 MHz → 1 us = 72 周期
     *     KB/s = (总字节数 / 1024) / (耗时_us / 1,000,000)
     *           = (总字节数 * 1,000,000) / (耗时_us * 1024)
     *           = (总字节数 * 1,000,000) / ((耗时周期/72) * 1024)
     *           = (总字节数 * 1,000,000 * 72) / (耗时周期 * 1024)
     * ============================================================ */
    #define CYCLES_PER_US   72UL
    #define US_PER_SEC      1000000UL

    dmaWriteKBps  = (u32)DMA_TEST_TOTAL_BYTES * US_PER_SEC / (cycDmaWrite  / CYCLES_PER_US) / 1024UL;
    dmaReadKBps   = (u32)DMA_TEST_TOTAL_BYTES * US_PER_SEC / (cycDmaRead   / CYCLES_PER_US) / 1024UL;
    pollWriteKBps = (u32)DMA_TEST_TOTAL_BYTES * US_PER_SEC / (cycPollWrite / CYCLES_PER_US) / 1024UL;
    pollReadKBps  = (u32)DMA_TEST_TOTAL_BYTES * US_PER_SEC / (cycPollRead  / CYCLES_PER_US) / 1024UL;

    printf("\r\n============================================\r\n");
    printf("  SPI Flash DMA / 轮询 速度对比  (%lu KB)\r\n",
           (u32)DMA_TEST_TOTAL_BYTES / 1024UL);
    printf("============================================\r\n");
    printf("  数据一致性        DMA=%s  轮询=%s\r\n",
           (dmaDataOk  != 0U) ? "OK" : "FAIL",
           (pollDataOk != 0U) ? "OK" : "FAIL");
    printf("--------------------------------------------\r\n");
    printf("  传输方式      |  耗时(us)    |  速度(KB/s)\r\n");
    printf("--------------------------------------------\r\n");
    printf("  DMA 写入      | %12lu | %11lu\r\n",
           cycDmaWrite  / CYCLES_PER_US, dmaWriteKBps);
    printf("  DMA 读取      | %12lu | %11lu\r\n",
           cycDmaRead   / CYCLES_PER_US, dmaReadKBps);
    printf("  轮询写入      | %12lu | %11lu\r\n",
           cycPollWrite / CYCLES_PER_US, pollWriteKBps);
    printf("  轮询读取      | %12lu | %11lu\r\n",
           cycPollRead  / CYCLES_PER_US, pollReadKBps);
    printf("--------------------------------------------\r\n");

    /* ============================================================
     *  9. 清理测试区域 — 擦除写入过数据的扇区，恢复全 0xFF
     * ============================================================ */
    printf("\r\n[清理测试区域]\r\n");
    for (i = 0U; i < DMA_TEST_SECTOR_CNT; i++)
    {
        SPI_Flash_Erase_Sector(i);
    }
    printf("  已擦除 %lu 个测试扇区，Flash 已恢复全 0xFF\r\n", (u32)DMA_TEST_SECTOR_CNT);
    printf("\r\n========== 测试结束 ==========\r\n");
}

/*
 * SPI_Flash_Erase_Auto — 自动检测并擦除已使用的 Flash 扇区
 *
 * 检测方法：
 *   先判断 Flash 首扇区（sector 0）是否有离线包索引数据。
 *   有 → 遍历 offline_package_index_t 索引表，只擦除已使用扇区（方案2）。
 *   无 → 全片扫描，逐扇区读取判断是否为全 0xFF，只擦除非空扇区（方案3）。
 *
 * 子函数说明：
 *   SPI_Flash_IsSectorEmpty(sectorIndex) — 判断指定扇区是否为全 0xFF
 *   SPI_Flash_EraseDirtySectors — 全片扫描方案，擦除所有有数据的扇区
 *   SPI_Flash_EraseByOfflineIndex — 使用离线包索引表精准擦除
 *
 * 返回值: 实际擦除的扇区数量
 */
static u8 SPI_Flash_IsSectorEmpty(u32 sectorIndex)
{
    u8  buf[8];
    u32 addr;
    u16 i;

    addr = sectorIndex * FLASH_SECTOR_SIZE;
    SPI_Flash_Read(buf, addr, sizeof(buf));

    for (i = 0U; i < sizeof(buf); i++)
    {
        if (buf[i] != 0xFFU)
            return 0U;      /* 发现非 0xFF 字节，扇区非空 */
    }
    return 1U;              /* 前 8 字节全为 0xFF，认为扇区为空 */
}

/*
 * SPI_Flash_EraseDirtySectors — 全片扫描，擦除所有有数据的扇区（方案3）
 *
 * 对 Flash 全部 1024 个扇区（4MB / 4KB），逐扇区读取前 8 字节判断，
 * 发现非 0xFF 则执行擦除。
 *
 * 返回值: 实际擦除的扇区数量
 */
static u32 SPI_Flash_EraseDirtySectors(void)
{
    u32 sectorCount;
    u32 i;
    u32 erased;

    sectorCount = FLASH_CAPACITY / FLASH_SECTOR_SIZE;
    erased = 0U;

    for (i = 0U; i < sectorCount; i++)
    {
        if (!SPI_Flash_IsSectorEmpty(i))
        {
            SPI_Flash_Erase_Sector(i);
            erased++;
        }
    }

    return erased;
}

/*
 * SPI_Flash_EraseByOfflineIndex — 使用离线包索引表精准擦除（方案2）
 *
 * 读取 Flash 首扇区中的 offline_package_index_t 索引表，
 * 对于每个 used=1 且 package_state=VALID/DELETED 的离线包，
 * 计算其占用的扇区范围并擦除。
 *
 * 首扇区索引表布局（1008 字节）：
 *   32 个条目 × 约 94 字节/条目
 *   每个条目的结构（参考 offLineRecorder.h）：
 *     offset 0: used            u8
 *     offset 1: package_state   u8
 *     offset 2: package_index   u16 LE
 *     offset 4: flash_addr      u32 LE
 *     offset 8: total_size      u32 LE
 *
 * 返回值: 实际擦除的扇区数量
 */
static u32 SPI_Flash_EraseByOfflineIndex(void)
{
    #define OFFLINE_ENTRY_SIZE      20U     /* 前 5 个有用字段的总和 */
    #define OFFLINE_MAX_ENTRIES     32U

    u8                  rawIndex[OFFLINE_MAX_ENTRIES * OFFLINE_ENTRY_SIZE];
    u8                  sectorMap[128];     /* 1024 扇区位图 = 128 字节 */
    u32                 i;
    u32                 erased;
    u16                 entry;
    u32                 secpos;
    u32                 flashAddr;
    u32                 totalSize;

    memset(sectorMap, 0, sizeof(sectorMap));

    /* 读取首扇区的索引表（前 32 条目 × 20 字节 = 640 字节）*/
    SPI_Flash_Read(rawIndex, 0UL, sizeof(rawIndex));

    /* 遍历所有条目，标记已使用的扇区 */
    for (entry = 0U; entry < OFFLINE_MAX_ENTRIES; entry++)
    {
        u16 offset = entry * OFFLINE_ENTRY_SIZE;
        u8  used   = rawIndex[offset];
        u8  state  = rawIndex[offset + 1U];

        /* 只处理有效或已删除的条目（跳过 EMPTY 和 WRITING）*/
        if (used == 0U)
            continue;

        /* 解析 flash_addr 和 total_size（小端序）*/
        flashAddr  = (u32)rawIndex[offset + 4U];
        flashAddr |= (u32)rawIndex[offset + 5U] << 8;
        flashAddr |= (u32)rawIndex[offset + 6U] << 16;
        flashAddr |= (u32)rawIndex[offset + 7U] << 24;

        totalSize  = (u32)rawIndex[offset + 8U];
        totalSize |= (u32)rawIndex[offset + 9U] << 8;
        totalSize |= (u32)rawIndex[offset + 10U] << 16;
        totalSize |= (u32)rawIndex[offset + 11U] << 24;

        if (totalSize == 0U)
            continue;

        /* 标记该包占用的所有扇区 */
        for (secpos = flashAddr / FLASH_SECTOR_SIZE;
             secpos <= (flashAddr + totalSize - 1U) / FLASH_SECTOR_SIZE;
             secpos++)
        {
            if (secpos < (FLASH_CAPACITY / FLASH_SECTOR_SIZE))
                sectorMap[secpos / 8U] |= (u8)(1U << (secpos % 8U));
        }
    }

    /* 只擦除位图中有标记的扇区 */
    erased = 0U;
    for (i = 0U; i < (FLASH_CAPACITY / FLASH_SECTOR_SIZE); i++)
    {
        if (sectorMap[i / 8U] & (u8)(1U << (i % 8U)))
        {
            SPI_Flash_Erase_Sector(i);
            erased++;
        }
    }

    return erased;
}

/*
 * SPI_Flash_IsValidOfflineIndex — 检查首扇区数据是否符合离线包索引表格式
 *
 * 验证规则：
 *   1. 每个条目的 used 字段必须为 0 或 1（不允许 0xFE/0xFF 等非法值）
 *   2. 当 used=1 时，package_state 必须在 1~3 范围内（WRITING/VALID/DELETED）
 *   3. 当 used=1 时，package_index 必须 < OFFLINE_MAX_PACKAGES (32)
 *   4. 所有 32 个条目中至少有 1 个 used=1（否则索引表无意义）
 *   5. used+package_state+package_index 这三个字段的累加和不能为 0xFF
 *      （防止全 0xFF 空扇区被误判为有效索引表）
 *
 * 返回值: 1=有效索引表, 0=无效
 */
static u8 SPI_Flash_IsValidOfflineIndex(void)
{
    #define OFFLINE_ENTRY_SIZE      20U
    #define OFFLINE_MAX_ENTRIES     32U

    u8   rawIndex[OFFLINE_MAX_ENTRIES * OFFLINE_ENTRY_SIZE];
    u16  entry;
    u8   hasValidEntry;
    u8   allZeroCheck;

    SPI_Flash_Read(rawIndex, 0UL, sizeof(rawIndex));

    hasValidEntry = 0U;
    allZeroCheck  = 0U;

    for (entry = 0U; entry < OFFLINE_MAX_ENTRIES; entry++)
    {
        u16 off = entry * OFFLINE_ENTRY_SIZE;
        u8  used          = rawIndex[off];
        u8  package_state = rawIndex[off + 1U];
        u16 package_index = (u16)rawIndex[off + 2U] |
                           ((u16)rawIndex[off + 3U] << 8);

        /* 累加校验和，如果所有字节都是 0xFF 则 total 会很大 */
        allZeroCheck |= used | package_state |
                       (u8)(package_index & 0xFFU) |
                       (u8)(package_index >> 8);

        /* used 必须是 0 或 1 */
        if (used > 1U)
            return 0U;

        if (used == 1U)
        {
            /* package_state 必须是 WRITING(1)/VALID(2)/DELETED(3) */
            if (package_state > 3U || package_state == 0U)
                return 0U;

            /* package_index 必须在有效范围内 */
            if (package_index >= OFFLINE_MAX_ENTRIES)
                return 0U;

            hasValidEntry = 1U;
        }
    }

    /* 所有字节都是 0xFF（全空扇区）→ 不是有效索引表 */
    if (allZeroCheck == 0U)
        return 0U;

    /* 必须有至少一个有效条目 */
    if (hasValidEntry == 0U)
        return 0U;

    return 1U;
}

/*
 * SPI_Flash_Erase_Auto — 自动识别并擦除已使用的 Flash 扇区
 *
 * 自动判别路径：
 *   ① 检查首扇区（sector 0）是否有数据且数据符合离线包索引表格式。
 *      是 → 走方案2（索引表精密擦除），只擦除已使用的扇区。
 *      否 → 走方案3（全片扫描擦除），逐扇区判空后只擦除非空扇区。
 *
 *   双重验证（首扇区非空 + 数据格式合法），避免误判。
 *
 * 返回值: 实际擦除的扇区数量
 */
u32 SPI_Flash_Erase_Auto(void)
{
    u32 erased;

    if ((!SPI_Flash_IsSectorEmpty(0U)) && SPI_Flash_IsValidOfflineIndex())
    {
        /* 首扇区有数据且符合索引表格式 → 走索引表精密擦除 */
        erased = SPI_Flash_EraseByOfflineIndex();
    }
    else
    {
        /* 首扇区为空或数据格式不符 → 全片扫描擦除脏扇区 */
        erased = SPI_Flash_EraseDirtySectors();
    }

    printf("SPI_Flash_Erase_Auto: 已擦除 %lu 个扇区 (%lu KB)\r\n",
           erased, erased * (FLASH_SECTOR_SIZE / 1024UL));

    return erased;
}
