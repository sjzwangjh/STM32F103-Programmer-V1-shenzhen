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
#include "offLineRecorder.h"
#include "offLineRecorder.h"
#include "Stk500Protocol.h"
#include "isp.h"
#include "flash.h"
#include <string.h>

/* 回放缓冲区大小（接收帧与发送帧均使用 BUFFER_SIZE） */
#define OFFLINE_REPLAY_FRAME_SIZE   BUFFER_SIZE
#define OFFLINE_REPLAY_TX_SIZE      BUFFER_SIZE
/* 标记无效数据包（最大 16 位值） */
#define OFFLINE_INVALID_PACKET      0xFFFFU

/* 熔丝位槽位索引（仅在单个包回放过程中使用的序号定义） */
#define OFFLINE_FUSE_LOW            0U   /* 低位熔丝 */
#define OFFLINE_FUSE_HIGH           1U   /* 高位熔丝 */
#define OFFLINE_FUSE_EXT            2U   /* 扩展熔丝 */
#define OFFLINE_FUSE_COUNT          3U   /* 熔丝总数 */

/** @brief 回放上下文结构体 */
typedef struct
{
    uint8_t initialized;                        /* 初始化标志 */
    uint16_t active_index;                      /* 当前激活的离线包索引 */
    uint32_t package_addr;                      /* SPI Flash 中的包基地址 */
    offline_raw_package_header_t header;        /* 包头部信息副本 */
} offline_replay_context_t;

/** @brief 熔丝位预期值记录结构（用于校验阶段比对） */
typedef struct
{
    uint8_t valid[OFFLINE_FUSE_COUNT];          /* 该槽位是否有编程操作 */
    uint8_t verified[OFFLINE_FUSE_COUNT];       /* 该槽位是否已被校验通过 */
    uint8_t value[OFFLINE_FUSE_COUNT];          /* 编程时写入的熔丝值 */
    uint16_t program_packet[OFFLINE_FUSE_COUNT]; /* 编程操作所在的数据包序号 */
} offline_fuse_expected_t;

/* 全局回放上下文 */
static offline_replay_context_t g_replay;
/* 接收帧缓冲区（存储从 SPI Flash 读取的原始 STK500 帧） */
static uint8_t g_replayFrame[OFFLINE_REPLAY_FRAME_SIZE];
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

    while (len-- != 0U)
        sum += *data++;
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
static uint8_t offlineReadPacket(uint32_t *cursor,
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
static uint8_t offlineExecuteFrame(uint16_t frameLen)
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
static uint8_t offlineIsDeferredRead(uint8_t cmd)
{
    return (cmd == STK_CMD_READ_FLASH_ISP ||
            cmd == STK_CMD_READ_EEPROM_ISP ||
            cmd == STK_CMD_READ_FUSE_ISP ||
            cmd == STK_CMD_READ_LOCK_ISP) ? 1U : 0U;
}

/**
 * @brief 从编程熔丝命令参数中解析出熔丝槽位
 * @param param 编程熔丝命令参数
 * @return 熔丝槽位索引（OFFLINE_FUSE_LOW/HIGH/EXT）或 -1（无效）
 *
 * 标准 AVR ISP 熔丝写入命令格式：
 *   AC A0 - 低位熔丝
 *   AC A8 - 高位熔丝
 *   AC A4 - 扩展熔丝
 */
static int8_t offlineProgramFuseSlot(const stkProgramFuseIsp_t *param)
{
    if (param->cmd[0] != 0xACU)
        return -1;
    if (param->cmd[1] == 0xA0U)
        return OFFLINE_FUSE_LOW;
    if (param->cmd[1] == 0xA8U)
        return OFFLINE_FUSE_HIGH;
    if (param->cmd[1] == 0xA4U)
        return OFFLINE_FUSE_EXT;
    return -1;
}

/**
 * @brief 从读取熔丝命令参数中解析出熔丝槽位
 * @param param 读取熔丝命令参数
 * @return 熔丝槽位索引（OFFLINE_FUSE_LOW/HIGH/EXT）或 -1（无效）
 *
 * 标准 AVR ISP 熔丝读取命令格式：
 *   50 00 - 低位熔丝
 *   58 08 - 高位熔丝
 *   50 08 - 扩展熔丝
 */
static int8_t offlineReadFuseSlot(const stkReadFuseIsp_t *param)
{
    if (param->cmd[0] == 0x50U && param->cmd[1] == 0x00U)
        return OFFLINE_FUSE_LOW;
    if (param->cmd[0] == 0x58U && param->cmd[1] == 0x08U)
        return OFFLINE_FUSE_HIGH;
    if (param->cmd[0] == 0x50U && param->cmd[1] == 0x08U)
        return OFFLINE_FUSE_EXT;
    return -1;
}

/**
 * @brief 第 1 道工序：编程回放
 * @return 0-成功 / 非 0-失败的数据包序号（从 1 开始）
 *
 * 遍历所有已记录的数据包，执行除"延迟读取"和"编程锁定位"之外的
 * 所有命令（主要包括：参数设置、地址加载、进入编程模式、
 * Flash/EEPROM 编程、熔丝编程等）。
 */
static uint16_t offlineReplayProgramPass(void)
{
    uint32_t cursor = g_replay.header.packet_area_offset;
    uint32_t i;
    offline_raw_packet_header_t packetHeader;

    for (i = 0U; i < g_replay.header.packet_count; i++)
    {
        uint8_t cmd;
        uint8_t status;

        if (offlineReadPacket(&cursor, i, &packetHeader) != 0U)
            return (uint16_t)(i + 1U);

        cmd = packetHeader.cmd;
        /* 跳过延迟读取（留给第 2 道工序校验）和锁定位编程（留给第 3 道工序） */
        if (offlineIsDeferredRead(cmd) ||
            cmd == STK_CMD_PROGRAM_LOCK_ISP)
        {
            continue;
        }

        status = offlineExecuteFrame(packetHeader.frame_len);
        if (status != STK_STATUS_CMD_OK)
            return (uint16_t)(i + 1U);
    }
    return 0U;
}

/**
 * @brief 第 2 道工序：校验回放
 * @return 0-成功 / 非 0-失败的数据包序号（从 1 开始）
 *
 * 本道工序专注于数据校验：
 *   - 重放地址加载和控制上下文命令（SET_PARAMETER、LOAD_ADDRESS、ENTER_PROGMODE）
 *   - 将 Flash/EEPROM 编程命令替换为 ispVerifyMemory() 进行读回校验
 *   - 记录熔丝编程操作的期望值，并在后续的 READ_FUSE_ISP 命令中比对实际值
 *   - 跳过擦除、正常读取、锁定位编程和退出编程模式等命令
 */
static uint16_t offlineReplayVerifyPass(void)
{
    uint32_t cursor = g_replay.header.packet_area_offset;
    uint32_t i;
    offline_raw_packet_header_t packetHeader;
    offline_fuse_expected_t expectedFuse;

    memset(&expectedFuse, 0, sizeof(expectedFuse));

    for (i = 0U; i < g_replay.header.packet_count; i++)
    {
        uint8_t cmd;
        void *param;

        if (offlineReadPacket(&cursor, i, &packetHeader) != 0U)
            return (uint16_t)(i + 1U);

        cmd = packetHeader.cmd;
        param = &g_replayFrame[STK_TXMSG_START + 1U];

        switch (cmd)
        {
        case STK_CMD_SET_PARAMETER:
        case STK_CMD_LOAD_ADDRESS:
        case STK_CMD_ENTER_PROGMODE_ISP:
            /* 重放地址/控制上下文命令 */
            if (offlineExecuteFrame(packetHeader.frame_len) != STK_STATUS_CMD_OK)
                return (uint16_t)(i + 1U);
            break;

        case STK_CMD_PROGRAM_FLASH_ISP:
            /* 将 Flash 编程替换为读回校验 */
            if (ispVerifyMemory((stkProgramFlashIsp_t *)param, 0U) != 0U)
                return (uint16_t)(i + 1U);
            break;

        case STK_CMD_PROGRAM_EEPROM_ISP:
            /* 将 EEPROM 编程替换为读回校验 */
            if (ispVerifyMemory((stkProgramFlashIsp_t *)param, 1U) != 0U)
                return (uint16_t)(i + 1U);
            break;

        case STK_CMD_PROGRAM_FUSE_ISP:
            {
                /* 记录熔丝编程的期望值，留待后续 READ_FUSE_ISP 验证 */
                int8_t slot = offlineProgramFuseSlot((stkProgramFuseIsp_t *)param);
                if (slot >= 0)
                {
                    expectedFuse.valid[(uint8_t)slot] = 1U;
                    expectedFuse.value[(uint8_t)slot] =
                        ((stkProgramFuseIsp_t *)param)->cmd[3];
                    expectedFuse.program_packet[(uint8_t)slot] = (uint16_t)i;
                }
            }
            break;

        case STK_CMD_READ_FUSE_ISP:
            {
                /* 通过先前记录的 READ_FUSE_ISP 命令读取实际熔丝值并与期望值比对 */
                stkReadFuseIsp_t *readParam = (stkReadFuseIsp_t *)param;
                int8_t slot = offlineReadFuseSlot(readParam);
                uint8_t actual;

                if (slot < 0)
                    return (uint16_t)(i + 1U);
                if (!expectedFuse.valid[(uint8_t)slot])
                    break;  /* 编程前的熔丝读取不是校验项，直接跳过 */

                actual = ispReadFuse(readParam);
                if (actual != expectedFuse.value[(uint8_t)slot])
                    return (uint16_t)(i + 1U);
                expectedFuse.verified[(uint8_t)slot] = 1U;
            }
            break;

        default:
            /* 擦除、正常读取、熔丝写入、锁定位编程和退出编程模式等
             * 命令在本次校验工序中不重复处理。
             */
            break;
        }
    }

    /* 检查所有已编程的熔丝槽位是否都已通过校验 */
    for (i = 0U; i < OFFLINE_FUSE_COUNT; i++)
    {
        if (expectedFuse.valid[i] && !expectedFuse.verified[i])
            return (uint16_t)(expectedFuse.program_packet[i] + 1U);
    }
    return 0U;
}

/**
 * @brief 第 3 道工序：锁定位编程与退出编程模式
 * @return 0-成功 / 非 0-失败的数据包序号（从 1 开始）
 *
 * 本道工序在所有编程和校验完成后执行：
 *   - 执行所有 PROGRAM_LOCK_ISP 命令（烧录锁定位）
 *   - 将最后一条 LEAVE_PROGMODE_ISP 命令记录下来，在循环结束后统一执行
 */
static uint16_t offlineReplayLockAndLeavePass(void)
{
    uint32_t cursor = g_replay.header.packet_area_offset;
    uint32_t i;
    uint16_t leavePacket = OFFLINE_INVALID_PACKET;
    uint16_t leaveFrameLen = 0U;
    uint8_t leaveFrame[32];
    offline_raw_packet_header_t packetHeader;

    for (i = 0U; i < g_replay.header.packet_count; i++)
    {
        if (offlineReadPacket(&cursor, i, &packetHeader) != 0U)
            return (uint16_t)(i + 1U);

        if (packetHeader.cmd == STK_CMD_PROGRAM_LOCK_ISP)
        {
            /* 执行锁定位编程 */
            stkProgramFuseIsp_t *param =
                (stkProgramFuseIsp_t *)&g_replayFrame[STK_TXMSG_START + 1U];
            if (ispProgramFuse(param) != STK_STATUS_CMD_OK)
                return (uint16_t)(i + 1U);
        }
        else if (packetHeader.cmd == STK_CMD_LEAVE_PROGMODE_ISP &&
                 packetHeader.frame_len <= sizeof(leaveFrame))
        {
            /* 暂存退出编程模式命令，留待最后执行 */
            memcpy(leaveFrame, g_replayFrame, packetHeader.frame_len);
            leaveFrameLen = packetHeader.frame_len;
            leavePacket = (uint16_t)i;
        }
    }

    /* 执行退出编程模式命令 */
    if (leavePacket != OFFLINE_INVALID_PACKET)
    {
        memcpy(g_replayFrame, leaveFrame, leaveFrameLen);
        if (offlineExecuteFrame(leaveFrameLen) != STK_STATUS_CMD_OK)
            return (uint16_t)(leavePacket + 1U);
    }

    return 0U;
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
        return 1U;
    /* 获取离线包的摘要信息 */
    if (offlinePgmerGetPackageSummary(activeIndex, &summary) != 0U ||
        summary.used == 0U ||
        summary.package_state != OFFLINE_PACKAGE_VALID)
    {
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

    if (!g_replay.initialized && offlinePgmer_init() != 0U)
        return 1U;

    /* 保存当前工作模式，切换为在线模式以执行回放 */
    savedWorkMode = stkGetWorkMode();
    (void)stkSetWorkMode(STK500_WORK_MODE_ONLINE);

    /* 三道工序依次执行，前一道成功才继续下一道 */
    result = offlineReplayProgramPass();
    if (result == 0U)
        result = offlineReplayVerifyPass();
    if (result == 0U)
        result = offlineReplayLockAndLeavePass();

    /* 恢复原始工作模式 */
    (void)stkSetWorkMode(savedWorkMode);
    return result;
}
