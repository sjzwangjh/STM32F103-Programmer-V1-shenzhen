/*
 * offLinePgmer.c - 离线 STK500 原始数据包回放引擎
 *
 * 回放策略（三道工序）：
 *   第 1 道：执行所有已记录的命令，但跳过 Flash/EEPROM 读取命令、
 *           熔丝/锁定位读取命令以及 PROGRAM_LOCK_ISP 命令。
 *   第 2 道：重放地址/控制上下文；将 PROGRAM_FLASH/EEPROM_ISP 命令
 *           替换为 ispVerifyMemory() 校验函数；通过已记录的
 *           READ_FUSE_ISP 命令验证熔丝字节。
 *   第 3 道：执行 PROGRAM_LOCK_ISP 命令，然后退出编程模式。
 *
 * offlinePgmer() 返回值：
 *   0  - 回放与校验全部通过
 *   >0 - 失败的原始数据包序号（从 1 开始计数）
 */

#include "offLinePgmer.h"
#include "offLineReplayAvr.h"
#include "offLineReplayPic.h"
#include "offLineRecorder.h"
#include "offLineRecorder.h"
#include "Stk500Protocol.h"
#include "isp.h"
#include "flash.h"
#include "usart.h"
#include <string.h>

/* 回放缓冲区大小（接收帧与发送帧均使用 BUFFER_SIZE） */
#define OFFLINE_REPLAY_FRAME_SIZE   BUFFER_SIZE
#define OFFLINE_REPLAY_TX_SIZE      BUFFER_SIZE
/* 标记无效数据包（最大 16 位值） */
#define OFFLINE_INVALID_PACKET      0xFFFFU

/* 熔丝位槽位索引（仅在单个包回放过程中使用的序号定义） */
/* 全局回放上下文 */
offline_replay_context_t g_replay;
/* 接收帧缓冲区（存储从 SPI Flash 读取的原始 STK500 帧） */
uint8_t g_replayFrame[OFFLINE_REPLAY_FRAME_SIZE];
/* 发送帧缓冲区（存储执行命令后生成的应答帧） */
static uint8_t g_replayTx[OFFLINE_REPLAY_TX_SIZE];

/**
 * @brief 计算 32 位累加和（用于校验数据帧完整性）
 * @param data 数据缓冲区指针
 * @param len  数据长度（字节数）
 * @return 32 位累加和
 */
static uint32_t offlineFrameSum32(const uint8_t *data, uint16_t len)
{
    uint32_t sum = 0U;

    /* Must match the record-side offlineCalcSum32() checksum exactly:
     * sum = (sum << 5) + sum + byte. */
    while (len-- != 0U)
        sum = (sum << 5) + sum + *data++;
    return sum;
}

/**
 * @brief 校验离线包头部是否合法
 * @param header      包头部指针
 * @param activeIndex 期望的激活包索引
 * @return 0-无效 / 1-有效
 *
 * 检查内容包括：魔数、版本、头部大小、包状态、索引匹配、
 * 数据包区域偏移与大小的范围有效性。
 */
static uint8_t offlineHeaderIsValid(const offline_raw_package_header_t *header,
                                    uint16_t activeIndex)
{
    if (header->magic != OFFLINE_RAW_MAGIC ||
        header->version != OFFLINE_RAW_VERSION ||
        header->header_size != sizeof(offline_raw_package_header_t) ||
        header->package_state != OFFLINE_PACKAGE_VALID ||
        header->package_index != activeIndex ||
        header->packet_area_offset < header->header_size ||
        header->total_size < header->packet_area_offset ||
        header->packet_area_size >
            (header->total_size - header->packet_area_offset))
    {
        return 0U;
    }
    return 1U;
}

/**
 * @brief 从 SPI Flash 中读取一个已存储的数据包
 * @param cursor       [输入/输出] 当前读取位置偏移（相对包基地址）
 * @param packetNo     期望读取的数据包序号
 * @param packetHeader [输出] 数据包头部信息
 * @return 0-成功 / 1-失败
 *
 * 读取流程：
 *   1. 参数校验与边界检查
 *   2. 从 SPI Flash 读取数据包头部
 *   3. 校验头部字段合法性（帧长度、序号、命令等）
 *   4. 读取帧数据到 g_replayFrame 缓冲区
 *   5. 使用累加和校验数据完整性
 *   6. 检查 STK500 帧格式（STX、TOKEN、命令等）
 *   7. 更新游标位置，指向下一个数据包
 */
uint8_t offlineReadPacket(uint32_t *cursor,
                                 uint32_t packetNo,
                                 offline_raw_packet_header_t *packetHeader)
{
    uint32_t packetAddr;
    uint32_t packageEnd;

    if (cursor == 0 || packetHeader == 0 ||
        packetNo >= g_replay.header.packet_count)
    {
        return 1U;
    }

    packageEnd = g_replay.header.total_size;
    if (*cursor > packageEnd ||
        (packageEnd - *cursor) < sizeof(offline_raw_packet_header_t))
    {
        return 1U;
    }

    packetAddr = g_replay.package_addr + *cursor;
    SPI_Flash_Read((uint8_t *)packetHeader,
                   packetAddr,
                   sizeof(offline_raw_packet_header_t));

    if (packetHeader->frame_len < 6U ||
        packetHeader->frame_len > OFFLINE_REPLAY_FRAME_SIZE ||
        packetHeader->seq != (uint16_t)packetNo ||
        packetHeader->cmd == 0U ||
        (packageEnd - *cursor - sizeof(offline_raw_packet_header_t)) <
            packetHeader->frame_len)
    {
        return 1U;
    }

    SPI_Flash_Read(g_replayFrame,
                   packetAddr + sizeof(offline_raw_packet_header_t),
                   packetHeader->frame_len);

    if (packetHeader->crc32 !=
        offlineFrameSum32(g_replayFrame, packetHeader->frame_len))
    {
        return 1U;
    }

    if (g_replayFrame[0] != STK_STX ||
        g_replayFrame[4] != STK_TOKEN ||
        g_replayFrame[STK_TXMSG_START] != packetHeader->cmd ||
        (((uint16_t)g_replayFrame[2] << 8) | g_replayFrame[3]) + 6U !=
            packetHeader->frame_len)
    {
        return 1U;
    }

    *cursor += sizeof(offline_raw_packet_header_t) + packetHeader->frame_len;
    return 0U;
}

/**
 * @brief 执行回放帧中的 STK500 命令
 * @param frameLen 帧长度
 * @return 命令执行状态（STK_STATUS_CMD_OK 或 STK_STATUS_CMD_FAILED）
 *
 * 将已加载到 g_replayFrame 中的命令帧提交给 STK500 协议栈处理，
 * 并返回应答帧中的状态字节。
 */
uint8_t offlineExecuteFrame(uint16_t frameLen)
{
    stkDataFrame_t dataFrame;

    memset(&dataFrame, 0, sizeof(dataFrame));
    memset(g_replayTx, 0, sizeof(g_replayTx));
    dataFrame.frame = g_replayFrame;
    dataFrame.frameLen = frameLen;
    dataFrame.txFrame = g_replayTx;
    dataFrame.txFrameSize = sizeof(g_replayTx);
    dataFrame.source = STK_DATA_SOURCE_FLASH_RECORD;

    stkEvaluateRxMessage(&dataFrame);
    if (dataFrame.txFrameLen < (STK_TXMSG_START + 2U))
        return STK_STATUS_CMD_FAILED;
    return g_replayTx[STK_TXMSG_START + 1U];
}

/**
 * @brief 判断命令是否为"延迟读取"类型（第 1 道工序跳过此类命令）
 * @param cmd STK500 命令码
 * @return 1-是延迟读取 / 0-不是
 *
 * 延迟读取包括 Flash 读取、EEPROM 读取、熔丝读取和锁定位读取。
 * 这些读取操作在第 2 道校验工序中由专门的校验函数替代执行。
 */
/* Dispatch to the AVR or PIC replay group by package architecture. */
static uint16_t offlineReplayProgramPass(void)
{
    if (g_replay.header.identity.arch == STK_MCU_ARCH_PIC)
        return picReplayProgramPass();
    return avrReplayProgramPass();
}

static uint16_t offlineReplayVerifyPass(void)
{
    if (g_replay.header.identity.arch == STK_MCU_ARCH_PIC)
        return picReplayVerifyPass();
    return avrReplayVerifyPass();
}

static uint16_t offlineReplayLockAndLeavePass(void)
{
    if (g_replay.header.identity.arch == STK_MCU_ARCH_PIC)
        return picReplayLockAndLeavePass();
    return avrReplayLockAndLeavePass();
}


/**
 * @brief 初始化离线回放模块
 * @return 0-成功 / 1-失败
 *
 * 从 EEPROM 中获取当前激活的离线记录索引，
 * 解析出对应的 SPI Flash 地址，加载并校验离线包头部信息。
 */
uint8_t offlinePgmer_init(void)
{
    offline_package_index_t summary;
    uint16_t activeIndex;

    memset(&g_replay, 0, sizeof(g_replay));

    /* 获取当前激活的离线包索引 */
    if (offlinePgmerGetActivePackage(&activeIndex) != 0U)
    {
#if DEBUG_HARDWARE_CONFIG
        uart1_WriteString("REPLAY init: no active package\r\n");
#endif
        return 1U;
    }
    /* 获取离线包的摘要信息 */
    if (offlinePgmerGetPackageSummary(activeIndex, &summary) != 0U ||
        summary.used == 0U ||
        summary.package_state != OFFLINE_PACKAGE_VALID)
    {
#if DEBUG_HARDWARE_CONFIG
        uart1_WriteString("REPLAY init: invalid summary\r\n");
#endif
        return 1U;
    }

    /* 从 SPI Flash 中读取离线包头部 */
    SPI_Flash_Read((uint8_t *)&g_replay.header,
                   summary.flash_addr,
                   sizeof(g_replay.header));
    /* 校验头部合法性，并与摘要信息进行交叉验证 */
    if (!offlineHeaderIsValid(&g_replay.header, activeIndex) ||
        g_replay.header.total_size != summary.total_size ||
        g_replay.header.packet_count != summary.packet_count ||
        g_replay.header.crc32 != summary.crc32)
    {
#if DEBUG_HARDWARE_CONFIG
        uart1_WriteString("REPLAY init: invalid header\r\n");
#endif
        memset(&g_replay, 0, sizeof(g_replay));
        return 1U;
    }

    g_replay.active_index = activeIndex;
    g_replay.package_addr = summary.flash_addr;
    g_replay.initialized = 1U;
    return 0U;
}

/**
 * @brief 执行完整的 AVR ISP 离线回放工作流
 * @return 0-全部通过 / 1-初始化失败 / >1-失败的数据包序号
 *
 * 三道工序依次执行：
 *   1. 编程回放 - 执行实际编程操作
 *   2. 校验回放 - 验证编程结果的正确性
 *   3. 锁定位与退出 - 烧录锁定位并退出编程模式
 *
 * 执行过程中将工作模式临时切换为在线模式以确保命令正常处理，
 * 执行完毕后恢复原始工作模式。
 */
uint16_t offlinePgmer(void)
{
    uint16_t result;
    uint8_t savedWorkMode;

    /* Always re-read the active package so a re-recorded package is used. */
    if (offlinePgmer_init() != 0U)
        return 1U;

#if DEBUG_HARDWARE_CONFIG
    uart1_WriteString("REPLAY enter, initialized=");
    uart1_WriteDec(g_replay.initialized);
    uart1_WriteString("\r\n");
#endif

    /* 保存当前工作模式，切换为在线模式以执行回放 */
    savedWorkMode = stkGetWorkMode();
    (void)stkSetWorkMode(STK500_WORK_MODE_REPLAY);

    /* 三道工序依次执行，前一道成功才继续下一道 */
    result = offlineReplayProgramPass();
#if DEBUG_HARDWARE_CONFIG
    uart1_WriteString("REPLAY program=");
    uart1_WriteDec(result);
    uart1_WriteString("\r\n");
#endif
    if (result == 0U)
    {
        result = offlineReplayVerifyPass();
#if DEBUG_HARDWARE_CONFIG
        uart1_WriteString("REPLAY verify=");
        uart1_WriteDec(result);
        uart1_WriteString("\r\n");
#endif
    }
    {
        /* Always run the lock/leave pass so the target is powered down
         * even when an earlier pass failed. */
        uint16_t lockResult = offlineReplayLockAndLeavePass();
        if (result == 0U && lockResult != 0U)
            result = lockResult;
#if DEBUG_HARDWARE_CONFIG
        uart1_WriteString("REPLAY lockleave=");
        uart1_WriteDec(lockResult);
        uart1_WriteString("\r\n");
#endif
    }

    /* 恢复原始工作模式 */
    (void)stkSetWorkMode(savedWorkMode);
#if DEBUG_HARDWARE_CONFIG
    uart1_WriteString("REPLAY done=");
    uart1_WriteDec(result);
    uart1_WriteString("\r\n");
#endif
    return result;
}
