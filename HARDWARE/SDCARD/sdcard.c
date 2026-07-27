/*
 * SD 卡驱动 — 适用于 STM32F103VET6 的 SDIO 接口
 *
 * 【功能概要】
 *   - SDIO 命令收发（含 CRC 忽略选项）
 *   - 单块读写（轮询 FIFO）
 *   - 多块 DMA 读写（DMA2_Channel5，32 位字传输）
 *   - 4-bit 失败后自动回退到 1-bit
 *   - 总线速度扫描调优
 *
 * 【DMA 映射】
 *   DMA2_Channel5 → SDIO FIFO（读/写共用同一个通道，靠 DCTRL.DTDIR 区分方向）
 */

#include "sdcard.h"
#include "delay.h"
#include <stdio.h>
#include <string.h>

/* 调试打印开关 — 通过 DEBUG_SD 宏控制 */
#if DEBUG_SD
#define SD_DEBUG_PRINT(...)     printf(__VA_ARGS__)
#else
#define SD_DEBUG_PRINT(...)     ((void)0)
#endif

/* 全局 SD 卡信息结构体 */
SDCardInfo_t SDCard_Info;

/* ========== SDIO 命令索引（CMD 号） ========== */
#define SD_CMD0             0U      /* GO_IDLE_STATE — 复位卡到空闲 */
#define SD_CMD2             2U      /* ALL_SEND_CID — 广播读取 CID */
#define SD_CMD3             3U      /* SEND_RELATIVE_ADDR — 获取 RCA */
#define SD_CMD7             7U      /* SELECT_DESELECT_CARD — 选中/取消选中 */
#define SD_CMD8             8U      /* SEND_IF_COND — 检测 SD V2 卡 */
#define SD_CMD9             9U      /* SEND_CSD — 读取 CSD 寄存器 */
#define SD_CMD12            12U     /* STOP_TRANSMISSION — 停止多块传输 */
#define SD_CMD13            13U     /* SEND_STATUS — 查询卡状态 */
#define SD_CMD16            16U     /* SET_BLOCKLEN — 设置块长度（非 SDHC 卡用） */
#define SD_CMD17            17U     /* READ_SINGLE_BLOCK — 读单块 */
#define SD_CMD18            18U     /* READ_MULTIPLE_BLOCK — 读多块 */
#define SD_CMD24            24U     /* WRITE_BLOCK — 写单块 */
#define SD_CMD25            25U     /* WRITE_MULTIPLE_BLOCK — 写多块 */
#define SD_CMD55            55U     /* APP_CMD — 告诉卡下一帧是 ACMD */
#define SD_ACMD6            6U      /* SET_BUS_WIDTH — 设置总线宽度（1/4 bit） */
#define SD_ACMD51           51U     /* SEND_SCR — 读取 SCR 寄存器 */
#define SD_ACMD41           41U     /* SD_SEND_OP_COND — 初始化/设置 OCR */

/* 通用超时：2 秒 */
#define SD_TIMEOUT_MS       2000U

/* 速度扫描分频表 — 从慢到快 */
static const u16 g_sd_clk_scan_divs[] = { 80U, 60U, 40U, 28U, 20U, 14U, 10U, 8U };

/* 响应类型定义 */
#define SD_RESP_NONE        0U                              /* 无响应 */
#define SD_RESP_SHORT       SDIO_CMD_WAITRESP_0             /* 短响应（48 bit） */
#define SD_RESP_LONG        (SDIO_CMD_WAITRESP_0 | SDIO_CMD_WAITRESP_1) /* 长响应（136 bit） */

/* 命令中断标志清除掩码 */
#define SDIO_CMD_CLEAR_MASK   (SDIO_ICR_CCRCFAILC | SDIO_ICR_CTIMEOUTC | SDIO_ICR_CMDRENDC | SDIO_ICR_CMDSENTC)
/* 数据中断标志清除掩码 */
#define SDIO_DATA_CLEAR_MASK  (SDIO_ICR_DCRCFAILC | SDIO_ICR_DTIMEOUTC | SDIO_ICR_TXUNDERRC | SDIO_ICR_RXOVERRC | SDIO_ICR_DATAENDC | SDIO_ICR_DBCKENDC | SDIO_ICR_STBITERRC)

/*
 * SD_GetBits — 从 CSD/CID 等响应中提取指定 bit 区间的值
 *
 *   参数:
 *     resp  — 响应数组（4 个 u32，按 big-endian 位序存储）
 *     msb   — 最高位位置（0 ~ 127）
 *     lsb   — 最低位位置
 *   返回: 提取的位段值
 */
static u32 SD_GetBits(const u32 *resp, u8 msb, u8 lsb)
{
    u32 value;
    u8 bit;
    u8 width;

    value = 0;
    width = (u8)(msb - lsb + 1U);
    while (width--)
    {
        bit = (u8)(lsb + width);
        value <<= 1;
        value |= (resp[(127U - bit) / 32U] >> (bit & 31U)) & 1U;
    }

    return value;
}

/*
 * SD_LogicalToCardAddr — 将逻辑扇区号转换为卡地址
 *
 *   SDSC 卡（V1/V2）: 地址 = 扇区号 × 块大小（字节地址）
 *   SDHC 卡（V2HC）:  地址 = 扇区号（块地址）
 */
static u32 SD_LogicalToCardAddr(u32 sector)
{
    if (SDCard_Info.type == SD_TYPE_V2HC)
        return sector;              /* SDHC：块地址 */
    return sector * SD_BLOCK_SIZE;  /* SDSC：字节地址 */
}

/*
 * SD_SendCmdInternal — 发送 SDIO 命令的低层实现
 *
 *   参数:
 *     idx       — 命令号（0~63）
 *     arg       — 命令参数（32 bit）
 *     respType  — SD_RESP_NONE / SHORT / LONG
 *     resp      — 输出响应缓冲区（可为 NULL）
 *     ignoreCrc — 1 = 忽略 CRC 错误（CMD0/ACMD41 等需要）
 *   返回:
 *     SD_OK / SD_ERROR / SD_TIMEOUT
 */
static u8 SD_SendCmdInternal(u8 idx, u32 arg, u16 respType, u32 *resp, u8 ignoreCrc)
{
    u32 sta;
    u32 timeout;

    /* 清命令相关中断标志 */
    SDIO->ICR = SDIO_CMD_CLEAR_MASK;
    SDIO->ARG = arg;
    SDIO->CMD = (idx & 0x3FU) | respType | SDIO_CMD_CPSMEN;

    timeout = SD_TIMEOUT_MS * 100U;     /* 200 ms */
    while (timeout--)
    {
        sta = SDIO->STA;

        if (respType == SD_RESP_NONE)
        {
            /* 无响应命令：等待 CMDSENT（命令已发送） */
            if (sta & SDIO_STA_CMDSENT)
            {
                SDIO->ICR = SDIO_CMD_CLEAR_MASK;
                return SD_OK;
            }
        }
        else
        {
            /* 有响应命令：等待 CMDREND（响应已接收） */
            if (sta & SDIO_STA_CMDREND)
            {
                if (resp != 0)
                {
                    resp[0] = SDIO->RESP1;
                    if (respType == SD_RESP_LONG)
                    {
                        /* 长响应：136 bit，分别对应 RESP1~4 */
                        resp[1] = SDIO->RESP2;
                        resp[2] = SDIO->RESP3;
                        resp[3] = SDIO->RESP4;
                    }
                }
                SDIO->ICR = SDIO_CMD_CLEAR_MASK;
                return SD_OK;
            }

            /* CRC 错误（在某些命令中是正常的，如 CMD8 前的 CMD0） */
            if ((sta & SDIO_STA_CCRCFAIL) != 0U)
            {
                if (ignoreCrc != 0U)
                {
                    /* 允许 CRC 错误时仍可取回响应值 */
                    if (resp != 0)
                        resp[0] = SDIO->RESP1;
                    SDIO->ICR = SDIO_CMD_CLEAR_MASK;
                    return SD_OK;
                }
                SDIO->ICR = SDIO_CMD_CLEAR_MASK;
                return SD_ERROR;
            }
        }

        /* 命令超时退出 */
        if (sta & SDIO_STA_CTIMEOUT)
        {
            SDIO->ICR = SDIO_CMD_CLEAR_MASK;
            return SD_TIMEOUT;
        }
    }

    SDIO->ICR = SDIO_CMD_CLEAR_MASK;
    return SD_TIMEOUT;
}

/*
 * SD_SendCmd — 标准命令发送（CRC 检查使能）
 */
static u8 SD_SendCmd(u8 idx, u32 arg, u16 respType, u32 *resp)
{
    return SD_SendCmdInternal(idx, arg, respType, resp, 0);
}

/*
 * SD_SendCmdNoCrc — 忽略 CRC 错误的命令发送
 */
static u8 SD_SendCmdNoCrc(u8 idx, u32 arg, u16 respType, u32 *resp)
{
    return SD_SendCmdInternal(idx, arg, respType, resp, 1);
}

/*
 * SD_SendAppCmd — 发送应用程序专用命令（ACMD）
 *
 *   先发 CMD55（宣告下一帧是 ACMD），再发实际的 ACMD。
 *   ACMD41 在 V2 卡初始化时 CRC 可能无效，使用 SD_SendCmdNoCrc。
 */
static u8 SD_SendAppCmd(u16 rca, u8 acmd, u32 arg, u32 *resp)
{
    u8 res;
    u32 dummy;

    /* 第一步：CMD55 — APP_CMD */
    res = SD_SendCmd(SD_CMD55, (u32)rca << 16, SD_RESP_SHORT, &dummy);
    if (res != SD_OK)
        return res;

    /* 第二步：ACMD 本体 — ACMD41 需要忽略 CRC */
    if (acmd == SD_ACMD41)
        return SD_SendCmdNoCrc(acmd, arg, SD_RESP_SHORT, resp);

    return SD_SendCmd(acmd, arg, SD_RESP_SHORT, resp);
}

/*
 * SD_ConfigData — 配置 SDIO 数据通道
 *
 *   参数:
 *     len     — 传输总字节数
 *     blkSize — 块大小编码（9 = 512 字节）
 *     dir     — 方向（0 = 写, 1 = 读）
 */
static void SD_ConfigData(u32 len, u8 blkSize, u8 dir)
{
    SDIO->ICR = SDIO_DATA_CLEAR_MASK;
    SDIO->DTIMER = 0x1FFFFFFFU;         /* 数据超时：最大 */
    SDIO->DLEN = len;                   /* 传输总字节数 */
    SDIO->DCTRL = ((u32)blkSize << 4);  /* 块大小编码 */
    if (dir != 0U)
        SDIO->DCTRL |= SDIO_DCTRL_DTDIR;/* 数据传输方向：外设→内存（读） */
    SDIO->DCTRL |= SDIO_DCTRL_DTEN;    /* 使能数据通道 */
}

/*
 * SD_SetBusConfig — 设置 SDIO 总线宽度和工作时钟
 *
 *   参数:
 *     width   — 1 或 4
 *     clkDiv  — SDIO_CLKCR 分频值
 */
static void SD_SetBusConfig(u8 width, u16 clkDiv)
{
    if (width == 4U)
    {
        SDCard_Info.bus_width = 4U;
        SDIO->CLKCR = (u32)clkDiv | SDIO_CLKCR_CLKEN | SDIO_CLKCR_WIDBUS_0;
    }
    else
    {
        SDCard_Info.bus_width = 1U;
        SDIO->CLKCR = (u32)clkDiv | SDIO_CLKCR_CLKEN;
    }
}

/*
 * SD_SetBusWidth — 使用工作时钟切换总线宽度（SD_CLK_WORK_DIV）
 */
static void SD_SetBusWidth(u8 width)
{
    SD_SetBusConfig(width, SD_CLK_WORK_DIV);
}

/*
 * SD_ReadCardStatus — 查询卡状态（CMD13）
 */
static u8 SD_ReadCardStatus(u32 *resp)
{
    return SD_SendCmd(SD_CMD13, (u32)SDCard_Info.rca << 16, SD_RESP_SHORT, resp);
}

/*
 * SD_ReadScr — 读取 SCR 寄存器（ACMD51）
 *
 *   SCR 是 8 字节的配置寄存器，包含总线宽度等信息。
 *   使用轮询 FIFO 方式读取 2 个 32 位字。
 */
static u8 SD_ReadScr(u8 scr[8])
{
    u32 *p32;
    u32 remainWords;
    u32 sta;
    u32 timeout;
    u8 res;

    if (scr == 0)
        return SD_ERROR;

    SD_ConfigData(8U, 3U, 1U);              /* 8 字节, 块大小=8(3) */
    res = SD_SendAppCmd(SDCard_Info.rca, SD_ACMD51, 0U, 0);
    if (res != SD_OK)
        return res;

    p32 = (u32 *)scr;
    remainWords = 2U;                       /* 8 字节 = 2 个字 */
    timeout = SD_TIMEOUT_MS * 1000U;

    /* 轮询 FIFO，等待数据到达 */
    while ((remainWords > 0U) && (timeout-- != 0U))
    {
        sta = SDIO->STA;

        if (sta & (SDIO_STA_DCRCFAIL | SDIO_STA_DTIMEOUT | SDIO_STA_RXOVERR))
        {
            SDIO->ICR = SDIO_DATA_CLEAR_MASK;
            return SD_ERROR;
        }

        if (sta & SDIO_STA_RXDAVL)
        {
            *p32++ = SDIO->FIFO;
            remainWords--;
        }
    }

    if (remainWords != 0U)
    {
        SDIO->ICR = SDIO_DATA_CLEAR_MASK;
        return SD_TIMEOUT;
    }

    /* 等待 DATAEND */
    timeout = SD_TIMEOUT_MS * 100U;
    while (timeout-- != 0U)
    {
        sta = SDIO->STA;
        if (sta & SDIO_STA_DATAEND)
        {
            SDIO->ICR = SDIO_DATA_CLEAR_MASK;
            return SD_OK;
        }
        if (sta & (SDIO_STA_DCRCFAIL | SDIO_STA_DTIMEOUT | SDIO_STA_RXOVERR))
        {
            SDIO->ICR = SDIO_DATA_CLEAR_MASK;
            return SD_ERROR;
        }
    }

    SDIO->ICR = SDIO_DATA_CLEAR_MASK;
    return SD_TIMEOUT;
}

/* 前向声明 */
static u8 SD_ReadSingleBlockOnce(u8 *buf, u32 sector);

/*
 * SD_PrintScr — 调试打印 SCR 寄存器内容
 */
static void SD_PrintScr(const u8 scr[8])
{
    u8 busWidthFlags;

    if (scr == 0)
        return;

    busWidthFlags = (u8)(scr[1] & 0x0FU);
    SD_DEBUG_PRINT("[SD] SCR=%02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                   scr[0], scr[1], scr[2], scr[3], scr[4], scr[5], scr[6], scr[7]);
    SD_DEBUG_PRINT("[SD] SCR bus width flags=0x%02X (1-bit=%d, 4-bit=%d)\r\n",
                   busWidthFlags,
                   (busWidthFlags & 0x01U) ? 1 : 0,
                   (busWidthFlags & 0x04U) ? 1 : 0);
}

/*
 * SD_VerifySectorSet — 对指定扇区集合做两轮读验证
 *
 *   用于速度扫描时快速确认给定时钟配置下数据通路是否可靠。
 */
static u8 SD_VerifySectorSet(const u32 *sectors, u8 sectorCount)
{
    u8 verifyBuf[SD_BLOCK_SIZE];
    u8 res;
    u8 i;
    u8 round;

    if (sectors == 0 || sectorCount == 0U)
        return SD_ERROR;

    for (i = 0U; i < sectorCount; i++)
    {
        for (round = 0U; round < 2U; round++)
        {
            res = SD_ReadSingleBlockOnce(verifyBuf, sectors[i]);
            if (res != SD_OK)
            {
                SD_DEBUG_PRINT("[SD] verify sector set fail: sector=%lu round=%u res=%u CLKCR=0x%08lX\r\n",
                               sectors[i], round, res, SDIO->CLKCR);
                return res;
            }
        }
    }

    return SD_OK;
}

/*
 * SD_TuneBusSpeedForSectors — 速度扫描调优
 *
 *   对指定的一组扇区，从慢到快逐档测试可读性，
 *   选择最后一档通过的分频值减一档作为工作时钟。
 *   若 SD_SPEED_SCAN_APPLY_RESULT 为 0，则只扫描不生效。
 *
 *   参数:
 *     width       — 总线宽度（1 或 4）
 *     sectors     — 测试扇区集合
 *     sectorCount — 测试扇区数量
 *   返回:
 *     SD_OK  = 至少一档通过
 *     SD_ERROR = 所有档都失败
 */
u8 SD_TuneBusSpeedForSectors(u8 width, const u32 *sectors, u8 sectorCount)
{
    u8 res;
    u8 i;
    u8 lastPassIndex;
    u8 selectedIndex;
    u8 hasPass;
    u16 finalDiv;

    hasPass = 0U;
    lastPassIndex = 0U;
    finalDiv = SD_CLK_WORK_DIV;

    if (sectors == 0 || sectorCount == 0U)
        return SD_ERROR;

    for (i = 0U; i < (u8)(sizeof(g_sd_clk_scan_divs) / sizeof(g_sd_clk_scan_divs[0])); i++)
    {
        SD_SetBusConfig(width, g_sd_clk_scan_divs[i]);
        delay_ms(1);

        res = SD_VerifySectorSet(sectors, sectorCount);
        SD_DEBUG_PRINT("[SD] speed scan width=%u div=%u res=%u CLKCR=0x%08lX\r\n",
                       width, g_sd_clk_scan_divs[i], res, SDIO->CLKCR);

        if (res == SD_OK)
        {
            hasPass = 1U;
            lastPassIndex = i;
        }
        else
        {
            break;  /* 第一次失败就停止扫描 */
        }
    }

    if (hasPass == 0U)
    {
        SD_SetBusConfig(width, finalDiv);
        SD_DEBUG_PRINT("[SD] speed scan no-pass, restore work div=%u\r\n", finalDiv);
        return SD_ERROR;
    }

    /* 选最后一档通过的前一档（留余量） */
    if (lastPassIndex == 0U)
        selectedIndex = 0U;
    else
        selectedIndex = (u8)(lastPassIndex - 1U);

#if SD_SPEED_SCAN_APPLY_RESULT
    finalDiv = g_sd_clk_scan_divs[selectedIndex];
    SD_DEBUG_PRINT("[SD] speed scan apply width=%u div=%u (highest-pass-index=%u, selected-index=%u)\r\n",
                   width, finalDiv, lastPassIndex, selectedIndex);
#else
    SD_DEBUG_PRINT("[SD] speed scan keep work div=%u (highest-pass-div=%u, selected-div=%u)\r\n",
                   finalDiv, g_sd_clk_scan_divs[lastPassIndex], g_sd_clk_scan_divs[selectedIndex]);
#endif

    SD_SetBusConfig(width, finalDiv);
    return SD_OK;
}

/*
 * SD_FallbackTo1Bit — 4-bit 数据通路失败时回退到 1-bit
 *
 *   通过 ACMD6 通知卡切换到 1-bit，并更新 MCU 侧总线配置。
 */
static void SD_FallbackTo1Bit(void)
{
    if (SDCard_Info.bus_width == 4U)
    {
        u32 resp;
        u8 res;

        SD_DEBUG_PRINT("[SD] data path failed in 4-bit, fallback to 1-bit\r\n");
        res = SD_SendAppCmd(SDCard_Info.rca, SD_ACMD6, 0U, &resp);
        SD_DEBUG_PRINT("[SD] ACMD6 revert-to-1bit res=%d resp=0x%08lX\r\n", res, resp);
        SD_SetBusWidth(1U);
        SDIO->ICR = SDIO_DATA_CLEAR_MASK;
        delay_ms(1);
    }
}

/*
 * SD_ResetDataPath — 复位数据通道控制/状态寄存器
 */
static void SD_ResetDataPath(void)
{
    SDIO->DCTRL = 0U;
    SDIO->ICR = SDIO_DATA_CLEAR_MASK;
    delay_ms(1);
}

/*
 * SD_ReadSingleBlockOnce — 单块读取（一次尝试，不含重试逻辑）
 *
 *   轮询 SDIO FIFO 方式读取 512 字节。
 */
static u8 SD_ReadSingleBlockOnce(u8 *buf, u32 sector)
{
    u32 *p32;
    u32 remainWords;
    u32 sta;
    u32 timeout;
    u32 addr;
    u8 res;

    res = SD_WaitReady();
    if (res != SD_OK)
        return res;

    addr = SD_LogicalToCardAddr(sector);
    SD_ConfigData(SD_BLOCK_SIZE, 9, 1);    /* 512 字节, 块大小编码=9, 读方向 */

    res = SD_SendCmd(SD_CMD17, addr, SD_RESP_SHORT, 0);
    if (res != SD_OK)
        return res;

    p32 = (u32 *)buf;
    remainWords = SD_BLOCK_SIZE / 4U;      /* 512/4 = 128 个字 */
    timeout = SD_TIMEOUT_MS * 1000U;       /* 2 秒超时 */

    while ((remainWords > 0U) && (timeout-- != 0U))
    {
        sta = SDIO->STA;

        if (sta & (SDIO_STA_DCRCFAIL | SDIO_STA_DTIMEOUT | SDIO_STA_RXOVERR))
        {
            SDIO->ICR = SDIO_DATA_CLEAR_MASK;
            return SD_ERROR;
        }

        if (sta & SDIO_STA_RXDAVL)
        {
            *p32++ = SDIO->FIFO;
            remainWords--;
        }
    }

    if (remainWords != 0U)
    {
        SDIO->ICR = SDIO_DATA_CLEAR_MASK;
        return SD_TIMEOUT;
    }

    /* 等待 DATAEND */
    timeout = SD_TIMEOUT_MS * 100U;
    while (timeout-- != 0U)
    {
        sta = SDIO->STA;
        if (sta & SDIO_STA_DATAEND)
        {
            SDIO->ICR = SDIO_DATA_CLEAR_MASK;
            return SD_OK;
        }
        if (sta & (SDIO_STA_DCRCFAIL | SDIO_STA_DTIMEOUT | SDIO_STA_RXOVERR))
        {
            SDIO->ICR = SDIO_DATA_CLEAR_MASK;
            return SD_ERROR;
        }
    }

    SDIO->ICR = SDIO_DATA_CLEAR_MASK;
    return SD_TIMEOUT;
}

/*
 * SD_VerifyDataPath — 读取一个扇区验证数据通路
 *
 *   用于初始化或模式切换后确认数据通路工作正常。
 */
static u8 SD_VerifyDataPath(u32 sector, const char *tag)
{
    u8 verifyBuf[SD_BLOCK_SIZE];
    u8 res;

    res = SD_ReadSingleBlockOnce(verifyBuf, sector);
    SD_DEBUG_PRINT("[SD] %s verify sector=%lu res=%d STA=0x%08lX CLKCR=0x%08lX\r\n",
                   tag, sector, res, SDIO->STA, SDIO->CLKCR);
    return res;
}

/*
 * SD_WriteSingleBlockOnce — 单块写入（一次尝试）
 *
 *   轮询写入 SDIO FIFO 方式写入 512 字节。
 */
static u8 SD_WriteSingleBlockOnce(const u8 *buf, u32 sector)
{
    const u32 *p32;
    u32 remainWords;
    u32 sta;
    u32 timeout;
    u32 addr;
    u8 res;

    res = SD_WaitReady();
    if (res != SD_OK)
        return res;

    addr = SD_LogicalToCardAddr(sector);
    SD_ConfigData(SD_BLOCK_SIZE, 9, 0);    /* 512 字节, 块大小编码=9, 写方向 */

    res = SD_SendCmd(SD_CMD24, addr, SD_RESP_SHORT, 0);
    if (res != SD_OK)
        return res;

    p32 = (const u32 *)buf;
    remainWords = SD_BLOCK_SIZE / 4U;      /* 128 个字 */
    timeout = SD_TIMEOUT_MS * 1000U;

    /* 同时等待：①字写完；②DATAEND */
    while ((timeout-- != 0U) && ((remainWords > 0U) || ((SDIO->STA & SDIO_STA_DATAEND) == 0U)))
    {
        sta = SDIO->STA;

        if (sta & (SDIO_STA_DCRCFAIL | SDIO_STA_DTIMEOUT | SDIO_STA_TXUNDERR))
        {
            SDIO->ICR = SDIO_DATA_CLEAR_MASK;
            return SD_ERROR;
        }

        if ((remainWords > 0U) && (sta & SDIO_STA_TXFIFOHE))
        {
            SDIO->FIFO = *p32++;
            remainWords--;
        }
        else if ((remainWords == 0U) && (sta & SDIO_STA_DATAEND))
        {
            SDIO->ICR = SDIO_DATA_CLEAR_MASK;
            return SD_OK;
        }
    }

    SDIO->ICR = SDIO_DATA_CLEAR_MASK;
    return (remainWords == 0U) ? SD_OK : SD_TIMEOUT;
}

/*
 * SD_PowerOn — 上电并配置 SDIO 引脚和时钟
 *
 *   引脚映射：
 *     PC8~PC11 → DAT0~DAT3
 *     PC12     → CLK
 *     PD2      → CMD
 *   初始时钟用 SD_CLK_INIT_DIV（低速，~400 KHz 以下）
 */
static u8 SD_PowerOn(void)
{
    RCC->APB2ENR |= (1U << 4) | (1U << 5);     /* GPIOC + GPIOD 时钟 */
    RCC->AHBENR  |= (1U << 10);                 /* SDIO 时钟 */

    /* PC8~PC11: DAT0~DAT3, PC12: CLK, PD2: CMD */
    GPIOC->CRH &= ~((u32)0xFFFFFU);
    GPIOC->CRH |=  ((u32)0xBBBBU);
    GPIOC->CRH |=  ((u32)0xBU << 16);
    GPIOD->CRL &= ~((u32)0xF << 8);
    GPIOD->CRL |=  ((u32)0xB << 8);
    SD_DEBUG_PRINT("[SD] GPIOC->CRH=0x%08lX GPIOD->CRL=0x%08lX\r\n", GPIOC->CRH, GPIOD->CRL);

    SDIO->POWER = 0x03U;                /* 上电 */
    SDIO->CLKCR = SD_CLK_INIT_DIV | SDIO_CLKCR_CLKEN;  /* 低速初始化时钟 */
    SDIO->ARG = 0;
    SDIO->CMD = 0;
    SDIO->DCTRL = 0;
    SDIO->DTIMER = 0x1FFFFFFFU;         /* 最长超时 */

    /* 默认状态 */
    SDCard_Info.type = SD_TYPE_UNKNOWN;
    SDCard_Info.rca = 0;
    SDCard_Info.bus_width = 1;

    delay_ms(2);
    return SD_OK;
}

/*
 * SD_Init — SD 卡完整初始化流程
 *
 *   步骤概述：
 *     1. SD_PowerOn() → GPIO + SDIO 上电
 *     2. CMD0  → 复位卡
 *     3. CMD8  → 检测 V2 卡（2.7~3.6V 支持）
 *     4. ACMD41 → 初始化并获取 OCR（区分 V2HC / V2 / V1）
 *     5. CMD2  → 获取 CID
 *     6. CMD3  → 获取 RCA
 *     7. CMD9  → 读取 CSD
 *     8. CMD7  → 选中卡
 *     9. 切换到 1-bit 工作模式并验证
 *    10. 若启用 4-bit 则尝试 ACMD6 切换并验证，失败回退
 *    11. 可选的速度扫描调优
 *    12. CMD16 → 设置块大小（非 SDHC 卡）
 *    13. SD_GetCardInfo() → 解析总容量
 */
u8 SD_Init(void)
{
    u32 resp[4];
    u32 timeout;
    u8 scr[8];
    u8 res;

    SD_DEBUG_PRINT("[SD] SD_Init start\r\n");
    SD_PowerOn();
    SD_DEBUG_PRINT("[SD] SD_PowerOn done, CLKCR=0x%08lX\r\n", SDIO->CLKCR);

    /* ---- CMD0：复位卡 ---- */
    res = SD_SendCmd(SD_CMD0, 0, SD_RESP_NONE, 0);
    SD_DEBUG_PRINT("[SD] CMD0 res=%d\r\n", res);
    if (res != SD_OK)
        return res;

    /* ---- CMD8：发送接口条件，区分 SD V2 ---- */
    res = SD_SendCmd(SD_CMD8, 0x1AAU, SD_RESP_SHORT, resp);
    SD_DEBUG_PRINT("[SD] CMD8 res=%d resp=0x%08lX\r\n", res, resp[0]);
    if ((res == SD_OK) && ((resp[0] & 0xFFFU) == 0x1AAU))
    {
        /* 支持 2.7~3.6V 的 SD V2 卡 */
        SD_DEBUG_PRINT("[SD] card type probe: SD V2.x path\r\n");
        timeout = SD_TIMEOUT_MS;
        do
        {
            res = SD_SendAppCmd(0, SD_ACMD41, 0x40300000U, resp);
            if ((timeout == SD_TIMEOUT_MS) || ((timeout % 100U) == 0U) || (res != SD_OK))
                SD_DEBUG_PRINT("[SD] ACMD41(V2) res=%d resp=0x%08lX timeout_left=%lu\r\n", res, resp[0], timeout);
            if (res != SD_OK)
                return res;
            delay_ms(1);
        } while (((resp[0] & 0x80000000U) == 0U) && (--timeout != 0U));

        if (timeout == 0U)
        {
            SD_DEBUG_PRINT("[SD] ACMD41(V2) timeout\r\n");
            return SD_TIMEOUT;
        }

        /* 根据 OCR bit30 区分 SDHC/SDXC */
        SDCard_Info.type = ((resp[0] & 0x40000000U) != 0U) ? SD_TYPE_V2HC : SD_TYPE_V2;
        SD_DEBUG_PRINT("[SD] ACMD41 final OCR=0x%08lX\r\n", resp[0]);
        SD_DEBUG_PRINT("[SD] card type=%s\r\n", (SDCard_Info.type == SD_TYPE_V2HC) ? "SDHC" : "SD V2");
    }
    else
    {
        /* V1 卡或 MMC */
        SD_DEBUG_PRINT("[SD] card type probe: SD V1.x path\r\n");
        timeout = SD_TIMEOUT_MS;
        do
        {
            res = SD_SendAppCmd(0, SD_ACMD41, 0x00300000U, resp);
            if ((timeout == SD_TIMEOUT_MS) || ((timeout % 100U) == 0U) || (res != SD_OK))
                SD_DEBUG_PRINT("[SD] ACMD41(V1) res=%d resp=0x%08lX timeout_left=%lu\r\n", res, resp[0], timeout);
            if (res != SD_OK)
                return res;
            delay_ms(1);
        } while (((resp[0] & 0x80000000U) == 0U) && (--timeout != 0U));

        if (timeout == 0U)
        {
            SD_DEBUG_PRINT("[SD] ACMD41(V1) timeout\r\n");
            return SD_TIMEOUT;
        }

        SDCard_Info.type = SD_TYPE_V1;
        SD_DEBUG_PRINT("[SD] card type=SD V1\r\n");
    }

    /* ---- CMD2：获取 CID ---- */
    res = SD_SendCmd(SD_CMD2, 0, SD_RESP_LONG, SDCard_Info.cid);
    SD_DEBUG_PRINT("[SD] CMD2 res=%d\r\n", res);
    if (res != SD_OK)
        return res;

    /* ---- CMD3：获取 RCA（相对卡地址） ---- */
    res = SD_SendCmd(SD_CMD3, 0, SD_RESP_SHORT, resp);
    SD_DEBUG_PRINT("[SD] CMD3 res=%d resp=0x%08lX\r\n", res, resp[0]);
    if (res != SD_OK)
        return res;
    SDCard_Info.rca = (u16)(resp[0] >> 16);
    SD_DEBUG_PRINT("[SD] RCA=0x%04X\r\n", SDCard_Info.rca);

    /* ---- CMD9：读取 CSD ---- */
    res = SD_SendCmd(SD_CMD9, (u32)SDCard_Info.rca << 16, SD_RESP_LONG, SDCard_Info.csd);
    SD_DEBUG_PRINT("[SD] CMD9 res=%d\r\n", res);
    if (res != SD_OK)
        return res;

    /* ---- CMD7：选中卡 ---- */
    res = SD_SendCmd(SD_CMD7, (u32)SDCard_Info.rca << 16, SD_RESP_SHORT, resp);
    SD_DEBUG_PRINT("[SD] CMD7 res=%d resp=0x%08lX\r\n", res, resp[0]);
    if (res != SD_OK)
        return res;

    /* 1-bit 模式验证 */
    SD_SetBusWidth(1U);
    SD_DEBUG_PRINT("[SD] enter 1-bit mode, CLKCR=0x%08lX\r\n", SDIO->CLKCR);

    res = SD_VerifyDataPath(0U, "1-bit");
    if (res != SD_OK)
    {
        SD_DEBUG_PRINT("[SD] 1-bit data path verify failed\r\n");
        return res;
    }

    /* 读取 SCR */
    memset(scr, 0, sizeof(scr));
    res = SD_ReadScr(scr);
    SD_DEBUG_PRINT("[SD] ACMD51(SCR) res=%d\r\n", res);
    if (res == SD_OK)
        SD_PrintScr(scr);

/* ---- 4-bit 模式切换（可选） ---- */
#if SD_USE_4BIT_MODE
    res = SD_SendAppCmd(SDCard_Info.rca, SD_ACMD6, 2, resp);
    SD_DEBUG_PRINT("[SD] ACMD6 res=%d resp=0x%08lX\r\n", res, resp[0]);
    if (res == SD_OK)
    {
        delay_ms(2);
        SD_SetBusConfig(4U, SD_CLK_4BIT_VERIFY_DIV);
        SD_DEBUG_PRINT("[SD] bus width switched to 4-bit verify clock, CLKCR=0x%08lX\r\n", SDIO->CLKCR);
        res = SD_ReadCardStatus(resp);
        SD_DEBUG_PRINT("[SD] CMD13(after 4-bit switch) res=%d resp=0x%08lX\r\n", res, resp[0]);

        res = SD_VerifyDataPath(0U, "4-bit");
        if (res != SD_OK)
        {
            SD_DEBUG_PRINT("[SD] 4-bit data path verify failed\r\n");
            SD_FallbackTo1Bit();
        }
        else
        {
            SD_SetBusConfig(4U, SD_CLK_WORK_DIV);
            SD_DEBUG_PRINT("[SD] 4-bit verify passed, switch to work clock, CLKCR=0x%08lX\r\n", SDIO->CLKCR);
        }
    }
    else
    {
        SD_DEBUG_PRINT("[SD] ACMD6 failed, keep 1-bit mode\r\n");
    }
#endif

/* ---- 速度扫描（可选） ---- */
#if SD_ENABLE_SPEED_SCAN
    {
        const u32 initScanSectors[1] = {0U};
        res = SD_TuneBusSpeedForSectors(SDCard_Info.bus_width, initScanSectors, 1U);
        SD_DEBUG_PRINT("[SD] speed scan done, bus_width=%u final CLKCR=0x%08lX res=%d\r\n",
                       SDCard_Info.bus_width, SDIO->CLKCR, res);
    }
#else
    SD_SetBusConfig(SDCard_Info.bus_width, SD_CLK_WORK_DIV);
    SD_DEBUG_PRINT("[SD] speed scan disabled, use fixed CLKCR=0x%08lX\r\n", SDIO->CLKCR);
#endif

    /* ---- CMD16：设置块大小（仅 SDSC 需要） ---- */
    if (SDCard_Info.type != SD_TYPE_V2HC)
    {
        res = SD_SendCmd(SD_CMD16, SD_BLOCK_SIZE, SD_RESP_SHORT, resp);
        SD_DEBUG_PRINT("[SD] CMD16 res=%d resp=0x%08lX\r\n", res, resp[0]);
        if (res != SD_OK)
            return res;
    }

    /* ---- 解析卡信息 ---- */
    res = SD_GetCardInfo();
    SD_DEBUG_PRINT("[SD] SD_GetCardInfo res=%d, type=%d, blk_cnt=%lu, blk_size=%lu, capacity=%lu\r\n",
                   res, SDCard_Info.type, SDCard_Info.blk_cnt, SDCard_Info.blk_size, SDCard_Info.capacity);
    return res;
}

/*
 * SD_Detect — 检测 SD 卡是否已初始化
 *
 *   返回 SD_OK 表示卡已就绪，SD_ERROR 表示未初始化。
 */
u8 SD_Detect(void)
{
    if (SDCard_Info.type == SD_TYPE_UNKNOWN)
        return SD_ERROR;
    return SD_OK;
}

/*
 * SD_ReadSingleBlock — 单块读取（含重试+4-bit 回退）
 *
 *   在 SD_SINGLE_BLOCK_RETRY 次重试内尝试读取一个扇区。
 *   若为 4-bit 模式且首轮失败，自动回退到 1-bit。
 */
u8 SD_ReadSingleBlock(u8 *buf, u32 sector)
{
    u8 retry;
    u8 res;

    if (buf == 0)
        return SD_ERROR;

    for (retry = 0U; retry < SD_SINGLE_BLOCK_RETRY; retry++)
    {
        res = SD_ReadSingleBlockOnce(buf, sector);
        if (res == SD_OK)
            return SD_OK;

        if ((SDCard_Info.bus_width == 4U) && (retry == 0U))
            SD_FallbackTo1Bit();

        SD_ResetDataPath();
    }

    return res;
}

/*
 * SD_WriteSingleBlock — 单块写入（含重试+4-bit 回退）
 */
u8 SD_WriteSingleBlock(const u8 *buf, u32 sector)
{
    u8 retry;
    u8 res;

    if (buf == 0)
        return SD_ERROR;

    for (retry = 0U; retry < SD_SINGLE_BLOCK_RETRY; retry++)
    {
        res = SD_WriteSingleBlockOnce(buf, sector);
        if (res == SD_OK)
            return SD_OK;

        if ((SDCard_Info.bus_width == 4U) && (retry == 0U))
            SD_FallbackTo1Bit();

        SD_ResetDataPath();
    }

    return res;
}

/*
 * SD_ReadBlocks — 多块读取（轮询方式）
 *
 *   通过多次调用 SD_ReadSingleBlock 读取 count 个扇区。
 */
u8 SD_ReadBlocks(u8 *buf, u32 sector, u32 count)
{
    u32 i;
    u8 res;

    for (i = 0; i < count; i++)
    {
        res = SD_ReadSingleBlock(buf + i * SD_BLOCK_SIZE, sector + i);
        if (res != SD_OK)
            return res;
    }
    return SD_OK;
}

/*
 * SD_WriteBlocks — 多块写入（轮询方式）
 *
 *   通过多次调用 SD_WriteSingleBlock 写入 count 个扇区。
 */
u8 SD_WriteBlocks(const u8 *buf, u32 sector, u32 count)
{
    u32 i;
    u8 res;

    for (i = 0; i < count; i++)
    {
        res = SD_WriteSingleBlock(buf + i * SD_BLOCK_SIZE, sector + i);
        if (res != SD_OK)
            return res;
    }
    return SD_OK;
}

/*
 * SD_StopTransfer — 停止多块传输（CMD12）
 */
void SD_StopTransfer(void)
{
    (void)SD_SendCmd(SD_CMD12, 0, SD_RESP_SHORT, 0);
}

/*
 * SD_WaitReady — 等待卡就绪（CMD13 轮询，检查 bit8: READY_FOR_DATA）
 *
 *   返回 SD_OK 表示卡可接收新命令。
 */
u8 SD_WaitReady(void)
{
    u32 resp;
    u32 timeout;
    u8 res;

    timeout = SD_TIMEOUT_MS * 100U;
    while (timeout-- != 0U)
    {
        res = SD_SendCmd(SD_CMD13, (u32)SDCard_Info.rca << 16, SD_RESP_SHORT, &resp);
        if (res == SD_OK)
        {
            if ((resp & (1U << 8)) != 0U)   /* READY_FOR_DATA 位 */
                return SD_OK;
        }
        delay_us(10);
    }

    return SD_TIMEOUT;
}

/*
 * SD_GetCardInfo — 从 CSD 寄存器解析卡容量
 *
 *   CSD 结构版本 0（SDSC）:
 *     容量 = (C_SIZE+1) × 2^(C_SIZE_MULT+2) × 2^READ_BL_LEN
 *   CSD 结构版本 1（SDHC/SDXC）:
 *     容量 = (C_SIZE+1) × 512KB
 */
u8 SD_GetCardInfo(void)
{
    u32 c_size;
    u32 c_size_mult;
    u32 read_bl_len;
    u32 blocknr;
    u32 block_len;
    u32 csd_structure;

    SDCard_Info.blk_size = SD_BLOCK_SIZE;

    /* CSD[127:126] = CSD 结构版本 */
    csd_structure = SD_GetBits(SDCard_Info.csd, 127, 126);
    if (csd_structure == 0U)
    {
        /* CSD v1.0 — SDSC 卡 */
        read_bl_len = SD_GetBits(SDCard_Info.csd, 83, 80);   /* READ_BL_LEN */
        c_size      = SD_GetBits(SDCard_Info.csd, 73, 62);   /* C_SIZE */
        c_size_mult = SD_GetBits(SDCard_Info.csd, 49, 47);   /* C_SIZE_MULT */

        block_len = 1UL << read_bl_len;
        blocknr   = (c_size + 1UL) * (1UL << (c_size_mult + 2UL));
        SDCard_Info.capacity = blocknr * block_len;           /* 总字节数 */
        SDCard_Info.blk_cnt  = SDCard_Info.capacity / SD_BLOCK_SIZE; /* 512 字节扇区数 */
    }
    else if (csd_structure == 1U)
    {
        /* CSD v2.0 — SDHC/SDXC 卡 */
        c_size = SD_GetBits(SDCard_Info.csd, 69, 48);        /* C_SIZE */
        SDCard_Info.blk_cnt = (c_size + 1UL) * 1024UL;       /* 每份 512KB */
        SDCard_Info.capacity = SDCard_Info.blk_cnt * SD_BLOCK_SIZE;
    }
    else
    {
        return SD_ERROR;    /* 不支持的 CSD 版本 */
    }

    return SD_OK;
}

/* =================================================================
 * SDIO DMA 传输接口
 *
 *   DMA2_Channel5 同时用于 SDIO 读和写。
 *   通过 DCTRL.DTDIR 控制方向。
 *   数据宽度固定为 32 bit（字），与 SDIO FIFO 对齐。
 *
 *   状态机：
 *     IDLE  → Start() → READ/WRITE → IsFinished() → IDLE
 *     错误时 SD_DMA_FinishWith() 直接回到 IDLE
 * ================================================================= */
#define SD_DMA_STATE_IDLE       0U      /* DMA 空闲 */
#define SD_DMA_STATE_READ       1U      /* DMA 读传输中 */
#define SD_DMA_STATE_WRITE      2U      /* DMA 写传输中 */

static volatile u8 s_sdDmaState = SD_DMA_STATE_IDLE;    /* DMA 状态 */
static volatile u8 s_sdDmaResult = SD_OK;                /* 最近一次传输结果 */
static volatile u8 s_sdDmaNeedStop = 0U;                 /* 完成时是否需要发 CMD12 */

/*
 * SD_DMA_Init — 初始化 DMA2_Channel5 供 SDIO 使用
 *
 *   使能 DMA2 时钟，配置外设地址为 SDIO_FIFO。
 */
void SD_DMA_Init(void)
{
    RCC->AHBENR |= RCC_AHBENR_DMA2EN;
    delay_ms(1);

    DMA2_Channel5->CCR = 0;
    DMA2_Channel5->CPAR = (u32)&SDIO->FIFO;
    DMA2->IFCR = DMA_IFCR_CTCIF5;
    s_sdDmaState = SD_DMA_STATE_IDLE;
    s_sdDmaResult = SD_OK;
    s_sdDmaNeedStop = 0U;
}

/*
 * SD_DMA_StopDataPath — 停止 DMA 并复位 SDIO 数据通道
 */
static void SD_DMA_StopDataPath(void)
{
    DMA2_Channel5->CCR &= (u16)~DMA_CCR1_EN;
    SDIO->DCTRL = 0U;
    SDIO->ICR = SDIO_DATA_CLEAR_MASK;
    DMA2->IFCR = DMA_IFCR_CTCIF5;
}

/*
 * SD_DMA_FinishWith — 以指定结果结束当前 DMA 传输
 *
 *   停止数据通道，必要时发 CMD12 停止多块传输。
 */
static void SD_DMA_FinishWith(u8 result)
{
    SD_DMA_StopDataPath();
    if (s_sdDmaNeedStop != 0U)
        SD_StopTransfer();
    s_sdDmaNeedStop = 0U;
    s_sdDmaResult = result;
    s_sdDmaState = SD_DMA_STATE_IDLE;
}

/*
 * SD_DMA_Config — 配置 DMA2_Ch5 传输参数
 *
 *   固定 32-bit 宽度，外设地址始终为 SDIO_FIFO。
 *   isWrite = 0 → 读（外设→内存），1 → 写（内存→外设）。
 */
static void SD_DMA_Config(u32 memAddr, u32 wordCount, u8 isWrite)
{
    DMA2_Channel5->CCR &= (u16)~DMA_CCR1_EN;
    DMA2->IFCR = DMA_IFCR_CTCIF5;
    DMA2_Channel5->CPAR = (u32)&SDIO->FIFO;
    DMA2_Channel5->CMAR = memAddr;
    DMA2_Channel5->CNDTR = wordCount;

    /* SDIO FIFO 是 32 位宽度，DMA 外设/内存宽度都配置为 word。 */
    DMA2_Channel5->CCR = DMA_CCR1_MINC |
                         DMA_CCR1_PSIZE_1 |
                         DMA_CCR1_MSIZE_1 |
                         DMA_CCR1_PL_1;
    if (isWrite != 0U)
        DMA2_Channel5->CCR |= DMA_CCR1_DIR;
}

/*
 * SD_DMA_StartCommon — DMA 多块传输的公共启动逻辑
 *
 *   参数:
 *     buf     — 数据缓冲区
 *     sector  — 起始扇区号
 *     count   — 扇区数量
 *     isWrite — 0=读, 1=写
 *   返回: SD_OK 或错误码
 */
static u8 SD_DMA_StartCommon(u8 *buf, u32 sector, u32 count, u8 isWrite)
{
    u32 addr;
    u32 byteCount;
    u32 wordCount;
    u8 res;

    if (buf == 0 || count == 0U)
        return SD_ERROR;
    if (s_sdDmaState != SD_DMA_STATE_IDLE)
        return SD_ERROR;

    res = SD_WaitReady();
    if (res != SD_OK)
        return res;

    byteCount = count * SD_BLOCK_SIZE;      /* 总字节数 */
    wordCount = byteCount / 4U;             /* 总字数 */
    addr = SD_LogicalToCardAddr(sector);

    s_sdDmaResult = SD_OK;
    s_sdDmaNeedStop = 1U;                   /* 多块传输结束后需要 CMD12 */
    SDIO->ICR = SDIO_DATA_CLEAR_MASK;
    SD_DMA_Config((u32)buf, wordCount, isWrite);

    DMA2_Channel5->CCR |= DMA_CCR1_EN;      /* 使能 DMA */

    SDIO->DTIMER = 0x1FFFFFFFU;
    SDIO->DLEN = byteCount;                 /* 传输总字节数 */
    if (isWrite != 0U)
        SDIO->DCTRL = (9U << 4) | SDIO_DCTRL_DMAEN | SDIO_DCTRL_DTEN;          /* 写：DMA+传输使能 */
    else
        SDIO->DCTRL = (9U << 4) | SDIO_DCTRL_DTDIR | SDIO_DCTRL_DMAEN | SDIO_DCTRL_DTEN; /* 读：方向+DMA+传输使能 */

    /* 发多块读/写命令 */
    res = SD_SendCmd((isWrite != 0U) ? SD_CMD25 : SD_CMD18, addr, SD_RESP_SHORT, 0);
    if (res != SD_OK)
    {
        SD_DMA_StopDataPath();
        s_sdDmaNeedStop = 0U;
        return res;
    }

    s_sdDmaState = (isWrite != 0U) ? SD_DMA_STATE_WRITE : SD_DMA_STATE_READ;
    return SD_OK;
}

/*
 * SD_ReadBlocks_DMA_Start — 启动 DMA 多块读
 */
u8 SD_ReadBlocks_DMA_Start(u8 *buf, u32 sector, u32 count)
{
    return SD_DMA_StartCommon(buf, sector, count, 0U);
}

/*
 * SD_WriteBlocks_DMA_Start — 启动 DMA 多块写
 */
u8 SD_WriteBlocks_DMA_Start(const u8 *buf, u32 sector, u32 count)
{
    return SD_DMA_StartCommon((u8 *)buf, sector, count, 1U);
}

/*
 * SD_DMA_IsFinishedCommon — 检查 DMA 传输是否完成
 *
 *   轮询 SDIO 状态和 DMA 传输完成标志。
 *   错误或完成时内部调用 SD_DMA_FinishWith 清理状态。
 */
static u8 SD_DMA_IsFinishedCommon(u8 isWrite)
{
    u32 sta;
    u32 errMask;

    if (s_sdDmaState == SD_DMA_STATE_IDLE)
        return 1U;

    sta = SDIO->STA;
    errMask = (isWrite != 0U) ?
              (SDIO_STA_DCRCFAIL | SDIO_STA_DTIMEOUT | SDIO_STA_TXUNDERR) :
              (SDIO_STA_DCRCFAIL | SDIO_STA_DTIMEOUT | SDIO_STA_RXOVERR);

    if ((sta & errMask) != 0U)
    {
        SD_DMA_FinishWith((sta & SDIO_STA_DTIMEOUT) ? SD_TIMEOUT : SD_ERROR);
        return 1U;
    }

    /* 等待 DMA TC + SDIO DATAEND */
    if (((DMA2->ISR & DMA_ISR_TCIF5) == 0U) || ((sta & SDIO_STA_DATAEND) == 0U))
        return 0U;

    SD_DMA_FinishWith(SD_OK);
    return 1U;
}

/*
 * SD_ReadBlocks_DMA_IsFinished — 查询 DMA 多块读是否完成
 */
u8 SD_ReadBlocks_DMA_IsFinished(void)
{
    if (s_sdDmaState != SD_DMA_STATE_READ)
        return 1U;
    return SD_DMA_IsFinishedCommon(0U);
}

/*
 * SD_WriteBlocks_DMA_IsFinished — 查询 DMA 多块写是否完成
 */
u8 SD_WriteBlocks_DMA_IsFinished(void)
{
    if (s_sdDmaState != SD_DMA_STATE_WRITE)
        return 1U;
    return SD_DMA_IsFinishedCommon(1U);
}

/*
 * SD_DMA_GetResult — 获取最近一次 DMA 传输结果
 */
u8 SD_DMA_GetResult(void)
{
    return s_sdDmaResult;
}

/*
 * SD_ReadBlocks_DMA — DMA 多块读（阻塞兼容接口）
 *
 *   内部包含 4-bit 回退逻辑。
 */
u8 SD_ReadBlocks_DMA(u8 *buf, u32 sector, u32 count)
{
    u8 res;
    u8 retry;

    if(buf == 0) return SD_ERROR;

    for (retry = 0U; retry < 2U; retry++)
    {
        res = SD_ReadBlocks_DMA_Start(buf, sector, count);
        if(res == SD_OK)
        {
            while(SD_ReadBlocks_DMA_IsFinished() == 0U)
            {
            }
            res = SD_DMA_GetResult();
            if(res == SD_OK)
                return SD_OK;
        }

        if ((SDCard_Info.bus_width != 4U) || (retry != 0U))
            return res;

        SD_FallbackTo1Bit();
        SD_ResetDataPath();
    }

    return res;
}

/*
 * SD_WriteBlocks_DMA — DMA 多块写（阻塞兼容接口）
 *
 *   内部包含 4-bit 回退逻辑。
 */
u8 SD_WriteBlocks_DMA(const u8 *buf, u32 sector, u32 count)
{
    u8 res;
    u8 retry;

    if(buf == 0) return SD_ERROR;

    for (retry = 0U; retry < 2U; retry++)
    {
        res = SD_WriteBlocks_DMA_Start(buf, sector, count);
        if(res == SD_OK)
        {
            while(SD_WriteBlocks_DMA_IsFinished() == 0U)
            {
            }
            res = SD_DMA_GetResult();
            if(res == SD_OK)
                return SD_OK;
        }

        if ((SDCard_Info.bus_width != 4U) || (retry != 0U))
            return res;

        SD_FallbackTo1Bit();
        SD_ResetDataPath();
    }

    return res;
}

