/*
 * SPI EEPROM 驱动模块
 * 基于 STM32F103VET6 的 SPI2 外设
 * 支持 FT25C64A（64Kbit / 8KB）等 SPI EEPROM 芯片
 */

#include "eeprom.h"
#include "spi.h"
#include <string.h>
#include <stdio.h>

/*
 * SPI_EEPROM_ClipLength - 截断长度以不超过 EEPROM 容量边界
 * addr: 起始地址
 * len:  请求长度
 * 返回值: 实际可用长度（超出容量则返回0或截断值）
 */
static u16 SPI_EEPROM_ClipLength(u32 addr, u16 len)
{
    u32 remain;

    if (addr >= SPI_EEPROM_CAPACITY)
        return 0;                           /* 起始地址超出容量，返回0 */

    remain = SPI_EEPROM_CAPACITY - addr;    /* 剩余可用空间 */
    if ((u32)len > remain)
        len = (u16)remain;                  /* 截断到可用空间长度 */

    return len;
}

/*
 * SPI_EEPROM_Init - 初始化 SPI EEPROM 模块
 * 配置 CS 和 WP 引脚为 GPIO 输出
 * 初始化 SPI2 外设，设置时钟速度为分频4
 * CS 和 WP 初始均为高电平（不选中 / 禁止写保护）
 */
void SPI_EEPROM_Init(void)
{
    /* 使能 CS 和 WP 引脚的 GPIO 时钟 */
    PORT_RCC_CLK(HW_SPI_EEPROM_CS);
    PORT_RCC_CLK(HW_SPI_EEPROM_WP);

    /* 配置 CS 和 WP 为推挽输出 */
    PORT_SET_DIR_PP(HW_SPI_EEPROM_CS);
    PORT_SET_DIR_PP(HW_SPI_EEPROM_WP);

    SPI_EEPROM_CS_H();          /* CS 初始为高（不选中） */
    SPI_EEPROM_WP_H();          /* WP 初始为高（禁止写保护） */

    SPI2_Init();                /* 初始化 SPI2 外设 */
    SPI2_SetSpeed(SPI_SPEED_4); /* 设置 SPI 时钟速度 */
}

/*
 * SPI_EEPROM_WriteEnable - 写使能（发送 WREN 指令）
 * 在每次写入或擦除操作前必须调用
 */
void SPI_EEPROM_WriteEnable(void)
{
    SPI_EEPROM_CS_L();
    SPI2_ReadWriteByte(SPI_EEPROM_CMD_WREN);  /* 发送写使能指令 */
    SPI_EEPROM_CS_H();
}

/*
 * SPI_EEPROM_WriteDisable - 写禁止（发送 WRDI 指令）
 */
void SPI_EEPROM_WriteDisable(void)
{
    SPI_EEPROM_CS_L();
    SPI2_ReadWriteByte(SPI_EEPROM_CMD_WRDI);  /* 发送写禁止指令 */
    SPI_EEPROM_CS_H();
}

/*
 * SPI_EEPROM_ReadStatusReg - 读状态寄存器
 * 返回值: 状态寄存器值
 *   bit0(WIP): 忙标志（1=正在编程/擦除）
 *   bit1(WEL): 写使能锁存（1=已使能）
 *   bit2~3(BP0~BP1): 块保护位
 *   bit7(WPEN): 写保护使能
 */
u8 SPI_EEPROM_ReadStatusReg(void)
{
    u8 sr;

    SPI_EEPROM_CS_L();
    SPI2_ReadWriteByte(SPI_EEPROM_CMD_RDSR);  /* 发送读状态寄存器指令 */
    sr = SPI2_ReadWriteByte(0xFF);             /* 读取状态寄存器值 */
    SPI_EEPROM_CS_H();

    return sr;
}

/*
 * SPI_EEPROM_WriteStatusReg - 写状态寄存器
 * sr: 要写入的状态寄存器值（用于设置块保护位等）
 */
void SPI_EEPROM_WriteStatusReg(u8 sr)
{
    SPI_EEPROM_WriteEnable();                 /* 写入前需要写使能 */
    SPI_EEPROM_CS_L();
    SPI2_ReadWriteByte(SPI_EEPROM_CMD_WRSR);  /* 发写状态寄存器指令 */
    SPI2_ReadWriteByte(sr);                   /* 写入状态寄存器值 */
    SPI_EEPROM_CS_H();
    SPI_EEPROM_WaitBusy();                    /* 等待操作完成 */
    SPI_EEPROM_WriteDisable();
}

/*
 * SPI_EEPROM_WaitBusy - 等待 EEPROM 内部操作完成
 * 轮询状态寄存器的 WIP 位（bit0），直到为0
 */
void SPI_EEPROM_WaitBusy(void)
{
    while (SPI_EEPROM_ReadStatusReg() & SPI_EEPROM_SR_WIP)
    {
        /* 等待 EEPROM 内部编程/擦除完成 */
    }
}

/*
 * SPI_EEPROM_ReadByte - 读取一个字节
 * addr: 要读取的地址（0x0000~0x1FFF）
 * 返回值: 该地址存储的数据字节
 */
u8 SPI_EEPROM_ReadByte(u32 addr)
{
    u8 data;

    addr &= SPI_EEPROM_MAX_ADDR;              /* 确保地址在合法范围内 */

    SPI_EEPROM_CS_L();
    SPI2_ReadWriteByte(SPI_EEPROM_CMD_READ);  /* 发送读数据指令 */
    SPI2_ReadWriteByte((u8)(addr >> 8));      /* 地址高8位 */
    SPI2_ReadWriteByte((u8)addr);             /* 地址低8位 */
    data = SPI2_ReadWriteByte(0xFF);          /* 读取数据字节 */
    SPI_EEPROM_CS_H();

    return data;
}

/*
 * SPI_EEPROM_WriteByte - 写入一个字节
 * addr: 要写入的地址
 * data: 要写入的数据
 */
void SPI_EEPROM_WriteByte(u32 addr, u8 data)
{
    addr &= SPI_EEPROM_MAX_ADDR;              /* 确保地址在合法范围内 */

    SPI_EEPROM_WriteEnable();                 /* 写使能 */
    SPI_EEPROM_CS_L();
    SPI2_ReadWriteByte(SPI_EEPROM_CMD_WRITE); /* 发送写数据指令 */
    SPI2_ReadWriteByte((u8)(addr >> 8));      /* 地址高8位 */
    SPI2_ReadWriteByte((u8)addr);             /* 地址低8位 */
    SPI2_ReadWriteByte(data);                 /* 写入数据 */
    SPI_EEPROM_CS_H();
    SPI_EEPROM_WaitBusy();                    /* 等待编程完成 */
    SPI_EEPROM_WriteDisable();
}

/*
 * SPI_EEPROM_Read - 读取连续数据
 * addr:  起始地址
 * pBuf:  输出缓冲区
 * len:   要读取的字节数
 */
void SPI_EEPROM_Read(u32 addr, u8 *pBuf, u16 len)
{
    if (pBuf == 0)
        return;

    len = SPI_EEPROM_ClipLength(addr, len);   /* 截断到有效范围 */
    if (len == 0)
        return;

    SPI_EEPROM_CS_L();
    SPI2_ReadWriteByte(SPI_EEPROM_CMD_READ);  /* 发送读数据指令 */
    SPI2_ReadWriteByte((u8)(addr >> 8));      /* 地址高8位 */
    SPI2_ReadWriteByte((u8)addr);             /* 地址低8位 */
    SPI2_ReadBuf(pBuf, len);                  /* 连续读取数据 */
    SPI_EEPROM_CS_H();
}

/*
 * SPI_EEPROM_WritePage - 写入一页数据（最大 32 字节，不跨页）
 * addr:  起始地址（需在页内起始位置）
 * pBuf:  数据源缓冲区
 * len:   要写入的字节数（自动限制在当前页剩余空间内）
 */
void SPI_EEPROM_WritePage(u32 addr, const u8 *pBuf, u16 len)
{
    u16 pageRemain;

    if (pBuf == 0)
        return;

    len = SPI_EEPROM_ClipLength(addr, len);   /* 截断到有效范围 */
    if (len == 0)
        return;

    /* 计算当前页内剩余空间 */
    pageRemain = (u16)(SPI_EEPROM_PAGE_SIZE - (addr & (SPI_EEPROM_PAGE_SIZE - 1U)));
    if (len > pageRemain)
        len = pageRemain;                     /* 限制在一页内 */

    SPI_EEPROM_WriteEnable();                 /* 写使能 */
    SPI_EEPROM_CS_L();
    SPI2_ReadWriteByte(SPI_EEPROM_CMD_WRITE); /* 发送写数据指令 */
    SPI2_ReadWriteByte((u8)(addr >> 8));      /* 地址高8位 */
    SPI2_ReadWriteByte((u8)addr);             /* 地址低8位 */
    SPI2_WriteBuf(pBuf, len);                 /* 连续写入数据 */
    SPI_EEPROM_CS_H();
    SPI_EEPROM_WaitBusy();                    /* 等待编程完成 */
    SPI_EEPROM_WriteDisable();
}

/*
 * SPI_EEPROM_Write - 写入连续数据（自动处理跨页）
 * 如果写入跨越页边界，自动拆分为多次页写入
 * addr:  起始地址
 * pBuf:  数据源缓冲区
 * len:   要写入的字节数
 */
void SPI_EEPROM_Write(u32 addr, const u8 *pBuf, u16 len)
{
    u16 writeLen;
    u16 pageOffset;

    if (pBuf == 0)
        return;

    len = SPI_EEPROM_ClipLength(addr, len);   /* 截断到有效范围 */
    while (len > 0)
    {
        /* 计算当前页内偏移和本次可写入长度 */
        pageOffset = (u16)(addr & (SPI_EEPROM_PAGE_SIZE - 1U));
        writeLen = (u16)(SPI_EEPROM_PAGE_SIZE - pageOffset);
        if (writeLen > len)
            writeLen = len;

        SPI_EEPROM_WritePage(addr, pBuf, writeLen);  /* 写入一页 */

        addr  += writeLen;                    /* 地址前进 */
        pBuf  += writeLen;                    /* 指针前进 */
        len   -= writeLen;                    /* 剩余字节数减少 */
    }
}

/*
 * SPI_EEPROM_EraseAll - 全片擦除
 * 将 EEPROM 所有存储单元写入 0xFF
 * 注：EEPROM 不支持硬件整片擦除指令，
 *     需通过逐页写入 0xFF 实现
 */
void SPI_EEPROM_EraseAll(void)
{
    u32 addr;
    u8 pageBuf[SPI_EEPROM_PAGE_SIZE];
    u16 i;

    /* 构造一页全 0xFF 的缓冲区 */
    for (i = 0; i < SPI_EEPROM_PAGE_SIZE; i++)
        pageBuf[i] = 0xFF;

    /* 逐页写入 0xFF */
    for (addr = 0; addr < SPI_EEPROM_CAPACITY; addr += SPI_EEPROM_PAGE_SIZE)
        SPI_EEPROM_WritePage(addr, pageBuf, SPI_EEPROM_PAGE_SIZE);
}

/*
 * SPI_EEPROM_ReadID - 读取厂商 ID 和设备 ID
 * 通过发送 0x9F（JEDEC ID）指令获取
 * mid: 输出厂商 ID 指针（可为 0）
 * did: 输出设备 ID 指针（可为 0）
 */
void SPI_EEPROM_ReadID(u8 *mid, u8 *did)
{
    SPI_EEPROM_CS_L();
    SPI2_ReadWriteByte(0x9F);               /* 读 JEDEC ID 指令 */
    if (mid != 0)
        *mid = SPI2_ReadWriteByte(0xFF);    /* 读厂商 ID */
    if (did != 0)
        *did = SPI2_ReadWriteByte(0xFF);    /* 读设备 ID */
    SPI_EEPROM_CS_H();
}

/*
 * SPI_EEPROM_DebugDemo - SPI EEPROM 调试示例
 * 可在 main() 完成 SPI_EEPROM_Init() 后手动调用。
 * 示例流程：
 * 1. 读取厂商 ID / 设备 ID
 * 2. 写入测试数据
 * 3. 回读并比较
 * 每一步都会通过 USART1 打印调试信息。
 */
void SPI_EEPROM_DebugDemo(void)
{
    static const u8 txBuf[16] =
    {
        0x45, 0x45, 0x50, 0x52, 0x4F, 0x4D, 0x5F, 0x44,
        0x45, 0x4D, 0x4F, 0x5F, 0x36, 0x34, 0x41, 0x21
    };
    u8 rxBuf[sizeof(txBuf)];
    u8 mid;
    u8 did;
    u8 compareOk;
    u16 i;

    memset(rxBuf, 0, sizeof(rxBuf));
    mid = 0;
    did = 0;
    compareOk = 0;

    /* 步骤1: 初始化 EEPROM */
    printf("【EEPROM调试】开始 SPI EEPROM 调试...\r\n");
    SPI_EEPROM_Init();
    printf("【EEPROM调试】初始化完成\r\n");

    /* 步骤2: 读取厂商 ID 和设备 ID */
    SPI_EEPROM_ReadID(&mid, &did);
    printf("【EEPROM调试】读取ID: 厂商=0x%02X, 设备=0x%02X\r\n", mid, did);

    /* 步骤3: 写入 16 字节测试数据到地址 0x0000 */
    printf("【EEPROM调试】写入数据到地址 0x0000: ");
    for (i = 0; i < sizeof(txBuf); i++)
        printf("%02X ", txBuf[i]);
    printf("\r\n");
    SPI_EEPROM_Write(0x0000U, txBuf, sizeof(txBuf));
    printf("【EEPROM调试】写入完成\r\n");

    /* 步骤4: 从地址 0x0000 回读数据 */
    SPI_EEPROM_Read(0x0000U, rxBuf, sizeof(rxBuf));
    printf("【EEPROM调试】回读数据: ");
    for (i = 0; i < sizeof(rxBuf); i++)
        printf("%02X ", rxBuf[i]);
    printf("\r\n");

    /* 步骤5: 比较写入和回读数据 */
    if (memcmp(txBuf, rxBuf, sizeof(txBuf)) == 0)
    {
        compareOk = 1;
        printf("【EEPROM调试】比较结果: 一致，读写测试通过！\r\n");
    }
    else
    {
        printf("【EEPROM调试】比较结果: 不一致，读写测试失败！\r\n");
    }

    /* 步骤6: 全片擦除后验证 */
    printf("【EEPROM调试】开始全片擦除...\r\n");
    SPI_EEPROM_EraseAll();
    printf("【EEPROM调试】全片擦除完成\r\n");

    memset(rxBuf, 0, sizeof(rxBuf));
    SPI_EEPROM_Read(0x0000U, rxBuf, sizeof(rxBuf));
    printf("【EEPROM调试】擦除后读取 0x0000: ");
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
        printf("【EEPROM调试】擦除验证通过，全部为 0xFF\r\n");
    else
        printf("【EEPROM调试】擦除验证失败，存在非 0xFF 数据\r\n");

    printf("【EEPROM调试】结束\r\n");

    (void)compareOk;
}

