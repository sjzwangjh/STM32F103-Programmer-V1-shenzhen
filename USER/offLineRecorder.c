/*
 * offLineRecorder.c - 离线数据包记录器公共控制层
 *
 * 本模块位于 STK500 协议层和具体离线存储/执行模块之间，主要负责:
 * 1. 管理工作模式: 模拟、在线、记录、在线+记录。
 * 2. 管理离线包记录会话: 从 START_PROG 到 STOP_PROG 保存上位机原始 STK500 数据包。
 * 3. 提供离线包查询接口: 获取包数量、包摘要、设置/读取当前激活包。

 *
 * 分层关系:
 * Stk500Protocol.c 负责解析命令和决定何时调用本模块。
 * offLineRecorder.c 负责统一工作模式、索引表、激活包和记录流程。
 * AVR/PIC 专用离线执行差异由各自模块处理, 本模块不解释芯片数据格式。
 */

#include "offLineRecorder.h"
#include "avrDeviceConst.h"
#include "picDeviceConst.h"
#include "Stk500Protocol.h"
#include "string.h"
#include "flash.h"
#include "eeprom.h"

/* 是否已经初始化过 Raw 包存储。 */
static uint8_t g_pgmStorageInited;

/*
 * Raw STK500 离线包布局:
 * sector 0: offline_package_index_t 索引表。
 * sector 1+: raw 包数据区, 每个包包含 package header + 多个 packet header + packet data。
 */
#define OFFLINE_RAW_DATA_START_ADDR     FLASH_SECTOR_SIZE

/* EEPROM 中保存当前激活离线包号的位置。 */
#define OFFLINE_ACTIVE_LOG_ADDR         0x0300U
#define OFFLINE_ACTIVE_LOG_SIZE         0x0100U
#define OFFLINE_ACTIVE_LOG_SLOT_COUNT   (OFFLINE_ACTIVE_LOG_SIZE / sizeof(offline_active_record_t))

/* SPI Flash 中的离线包索引表缓存。 */
static uint8_t g_rawIndexLoaded;
static offline_package_index_t g_offlinePackageIndex[OFFLINE_MAX_PACKAGES];

/* 当前正在记录的 raw STK500 离线包上下文。 */
static struct {
    uint8_t active;
    uint16_t index;
    uint32_t file_addr;
    uint32_t write_offset;
    uint32_t erased_until;
    uint32_t packet_count;
    uint32_t running_crc;
    offline_raw_package_header_t header;
} g_rawCapture;

/* Record-mode read-back scratch buffer for parsing the on-going package. */
static uint8_t g_readbackFrame[BUFFER_SIZE];

/* STK500 当前工作模式, 由协议层命令设置。 */
uint8_t g_stkWorkMode = STK500_WORK_MODE_ONLINE;

/* 当前激活的目标器件参数, 由协议层下发的 device identity 初始化。 */
offlineDeviceParams_t g_activeDeviceParams;


/* 延迟标记 Raw 包存储已经可用, 避免重复初始化。 */
static void offlinePgmerInitStorageOnce(void)
{
    if (g_pgmStorageInited == 0U)
        g_pgmStorageInited = 1U;
}

/* 对一段数据计算轻量级 32 位校验值, 用于索引/激活记录的完整性判断。 */
static uint32_t offlineCalcSum32(const uint8_t *data, uint16_t len)
{
    uint32_t sum = 0U;
    uint16_t i;

    if (data == 0) 
        return 0U;

    for (i = 0U; i < len; i++)
        sum = (sum << 5) + sum + data[i];

    return sum;
}

/* 将地址向上对齐到 SPI Flash 扇区边界。 */
#if !OFFLINE_SINGLE_PACKET_MODE
static uint32_t offlineAlignSector(uint32_t addr)
{
    return (addr + FLASH_SECTOR_SIZE - 1UL) & ~(FLASH_SECTOR_SIZE - 1UL);
}
#endif

/* 从 SPI Flash 加载 raw 离线包索引表, 同一次运行中只加载一次。 */
/* Validate one stored raw-package header from flash. */
/* Validate the immutable package-begin header. */
static uint8_t offlineRawBeginHeaderIsValid(const offline_raw_package_header_t *header,
                                            uint32_t flashAddr)
{
    if (header == 0)
        return 0U;

    if (header->magic != OFFLINE_RAW_MAGIC ||
        header->version != OFFLINE_RAW_VERSION ||
        header->header_size != sizeof(offline_raw_package_header_t) ||
        header->package_index >= OFFLINE_MAX_PACKAGES ||
        header->packet_area_offset != sizeof(offline_raw_package_header_t) ||
        flashAddr > FLASH_CAPACITY ||
        (FLASH_CAPACITY - flashAddr) < sizeof(offline_raw_package_header_t))
    {
        return 0U;
    }

    return 1U;
}

/* Scan forward until a tail commit record is found after all raw packets. */
static uint8_t offlineRawFindCommit(uint32_t flashAddr,
                                    const offline_raw_package_header_t *header,
                                    offline_raw_package_commit_t *commit)
{
    offline_raw_packet_header_t packetHeader;
    uint32_t cursor;

    if (!offlineRawBeginHeaderIsValid(header, flashAddr) || commit == 0)
        return 0U;

    cursor = flashAddr + header->packet_area_offset;
    while ((FLASH_CAPACITY - cursor) >= sizeof(offline_raw_packet_header_t))
    {
        uint32_t magic;

        SPI_Flash_Read((uint8_t *)&magic, cursor, sizeof(magic));
        if (magic == OFFLINE_RAW_COMMIT_MAGIC)
        {
            SPI_Flash_Read((uint8_t *)commit,
                           cursor,
                           sizeof(offline_raw_package_commit_t));
            if (commit->version != OFFLINE_RAW_VERSION ||
                commit->commit_size != sizeof(offline_raw_package_commit_t) ||
                commit->package_index != header->package_index ||
                commit->package_state != OFFLINE_PACKAGE_VALID ||
                commit->packet_area_size !=
                    (uint32_t)(cursor - flashAddr - header->packet_area_offset) ||
                commit->total_size !=
                    (uint32_t)(cursor - flashAddr + sizeof(offline_raw_package_commit_t)) ||
                commit->total_size > (FLASH_CAPACITY - flashAddr))
            {
                return 0U;
            }
            return 1U;
        }

        SPI_Flash_Read((uint8_t *)&packetHeader,
                       cursor,
                       sizeof(offline_raw_packet_header_t));
        if (packetHeader.frame_len < 6U ||
            packetHeader.frame_len > BUFFER_SIZE ||
            packetHeader.cmd == 0U ||
            (FLASH_CAPACITY - cursor - sizeof(offline_raw_packet_header_t)) <
                packetHeader.frame_len)
        {
            return 0U;
        }

        cursor += sizeof(offline_raw_packet_header_t) + packetHeader.frame_len;
    }

    return 0U;
}

/* Scan raw package area and rebuild the RAM summary table. */
static void offlineRawLoadIndex(void)
{
    offline_raw_package_header_t header;
    offline_raw_package_commit_t commit;
    uint32_t addr;

    if (g_rawIndexLoaded)
        return;

    memset(g_offlinePackageIndex, 0, sizeof(g_offlinePackageIndex));

#if OFFLINE_SINGLE_PACKET_MODE
    addr = OFFLINE_RAW_DATA_START_ADDR;
    SPI_Flash_Read((uint8_t *)&header,
                   addr,
                   sizeof(offline_raw_package_header_t));
    if (offlineRawFindCommit(addr, &header, &commit))
    {
        offline_package_index_t *entry = &g_offlinePackageIndex[0];
        entry->used = 1U;
        entry->package_state = commit.package_state;
        entry->package_index = 0U;
        entry->flash_addr = addr;
        entry->total_size = commit.total_size;
        entry->packet_area_size = commit.packet_area_size;
        entry->packet_count = commit.packet_count;
        entry->crc32 = commit.crc32;
        memcpy(&entry->identity, &commit.identity, sizeof(stkDeviceIdentity_t));
    }
#else
    addr = OFFLINE_RAW_DATA_START_ADDR;
    while ((FLASH_CAPACITY - addr) >= sizeof(offline_raw_package_header_t))
    {
        SPI_Flash_Read((uint8_t *)&header,
                       addr,
                       sizeof(offline_raw_package_header_t));
        if (offlineRawFindCommit(addr, &header, &commit))
        {
            offline_package_index_t *entry =
                &g_offlinePackageIndex[commit.package_index];
            entry->used = 1U;
            entry->package_state = commit.package_state;
            entry->package_index = commit.package_index;
            entry->flash_addr = addr;
            entry->total_size = commit.total_size;
            entry->packet_area_size = commit.packet_area_size;
            entry->packet_count = commit.packet_count;
            entry->crc32 = commit.crc32;
            memcpy(&entry->identity, &commit.identity, sizeof(stkDeviceIdentity_t));
            addr = offlineAlignSector(addr + commit.total_size);
        }
        else
        {
            addr += FLASH_SECTOR_SIZE;
        }
    }
#endif

    g_rawIndexLoaded = 1U;
}

/* 将 raw 离线包索引表写回 SPI Flash。 */

/* 判断指定序号的 raw 离线包是否存在且有效。 */
static uint8_t offlineRawIsValidIndex(uint16_t index)
{
    offlineRawLoadIndex();
    if (index >= OFFLINE_MAX_PACKAGES)
        return 0U;
    return (g_offlinePackageIndex[index].used == 1U &&
            g_offlinePackageIndex[index].package_state == OFFLINE_PACKAGE_VALID) ? 1U : 0U;
}

/* 为新的 raw 离线包分配一个索引槽。 */
static uint16_t offlineRawAllocIndex(void)
{
#if OFFLINE_SINGLE_PACKET_MODE
    /* Single-package debug mode: always reuse slot 0. */
    offlineRawLoadIndex();
    memset(&g_offlinePackageIndex[0], 0, sizeof(g_offlinePackageIndex[0]));
    g_offlinePackageIndex[0].used = 1U;
    g_offlinePackageIndex[0].package_state = OFFLINE_PACKAGE_WRITING;
    g_offlinePackageIndex[0].package_index = 0U;
    return 0U;
#else
    uint16_t i;

    offlineRawLoadIndex();
    for (i = 0U; i < OFFLINE_MAX_PACKAGES; i++)
    {
        if (g_offlinePackageIndex[i].used != 1U ||
            g_offlinePackageIndex[i].package_state == OFFLINE_PACKAGE_DELETED)
        {
            memset(&g_offlinePackageIndex[i], 0, sizeof(g_offlinePackageIndex[i]));
            g_offlinePackageIndex[i].used = 1U;
            g_offlinePackageIndex[i].package_state = OFFLINE_PACKAGE_WRITING;
            g_offlinePackageIndex[i].package_index = i;
            return i;
        }
    }

    return 0xFFFFU;
#endif
}

/* 为新的 raw 离线包分配 SPI Flash 起始地址。 */
static uint32_t offlineRawAllocFlashAddr(void)
{
#if OFFLINE_SINGLE_PACKET_MODE
    /* Single-package debug mode: always overwrite the fixed data area. */
    return OFFLINE_RAW_DATA_START_ADDR;
#else
    uint16_t i;
    uint32_t end;
    uint32_t max_end = OFFLINE_RAW_DATA_START_ADDR;

    offlineRawLoadIndex();
    for (i = 0U; i < OFFLINE_MAX_PACKAGES; i++)
    {
        if (g_offlinePackageIndex[i].used == 1U &&
            g_offlinePackageIndex[i].package_state != OFFLINE_PACKAGE_DELETED)
        {
            end = g_offlinePackageIndex[i].flash_addr + g_offlinePackageIndex[i].total_size;
            if (end > max_end)
                max_end = end;
        }
    }

    return offlineAlignSector(max_end);
#endif
}

/* 写入 raw 包数据前按需擦除覆盖到的 SPI Flash 扇区。 */
static void offlineRawEraseForWrite(uint32_t addr, uint32_t len)
{
    uint32_t end = addr + len;

    while (g_rawCapture.erased_until < end)
    {
        SPI_Flash_Erase_Sector(g_rawCapture.erased_until / FLASH_SECTOR_SIZE);
        g_rawCapture.erased_until += FLASH_SECTOR_SIZE;
    }
}

/* 计算 EEPROM 激活包记录的校验值, crc32 字段本身不参与计算。 */
static uint32_t offlineActiveCalcCrc(const offline_active_record_t *rec)
{
    return offlineCalcSum32((const uint8_t *)rec,
                            (uint16_t)(sizeof(offline_active_record_t) - sizeof(uint32_t)));
}

static uint8_t offlineActiveRecordIsValid(const offline_active_record_t *rec)
{
    if (rec == 0)
        return 0U;

    if (rec->magic != OFFLINE_ACTIVE_MAGIC ||
        rec->version != OFFLINE_RAW_VERSION ||
        rec->crc32 != offlineActiveCalcCrc(rec))
    {
        return 0U;
    }

    return 1U;
}

static void offlineActiveEraseLog(void)
{
    uint8_t eraseBuf[SPI_EEPROM_PAGE_SIZE];
    uint32_t addr;

    memset(eraseBuf, 0xFF, sizeof(eraseBuf));
    for (addr = OFFLINE_ACTIVE_LOG_ADDR;
         addr < (OFFLINE_ACTIVE_LOG_ADDR + OFFLINE_ACTIVE_LOG_SIZE);
         addr += SPI_EEPROM_PAGE_SIZE)
    {
        SPI_EEPROM_Write(addr, eraseBuf, SPI_EEPROM_PAGE_SIZE);
    }
}

static uint8_t offlineActiveFindLatestRecord(offline_active_record_t *rec,
                                             uint16_t *slotIndex)
{
    offline_active_record_t slotRec;
    uint16_t i;
    uint8_t found = 0U;

    for (i = 0U; i < OFFLINE_ACTIVE_LOG_SLOT_COUNT; i++)
    {
        SPI_EEPROM_Read(OFFLINE_ACTIVE_LOG_ADDR +
                        ((uint32_t)i * sizeof(offline_active_record_t)),
                        (uint8_t *)&slotRec,
                        sizeof(slotRec));
        if (!offlineActiveRecordIsValid(&slotRec))
            continue;

        if (rec != 0)
            memcpy(rec, &slotRec, sizeof(slotRec));
        if (slotIndex != 0)
            *slotIndex = i;
        found = 1U;
    }

    return found;
}


/* 初始化离线编程器公共控制层。 */
void offlinePgmerInit(void)
{
    g_stkWorkMode      = STK500_WORK_MODE_ONLINE;
    g_pgmStorageInited = 0U;
    g_rawIndexLoaded   = 0U;
    memset(&g_rawCapture, 0, sizeof(g_rawCapture));
}

/*
 * 用上位机下发的器件身份信息初始化当前目标器件。
 * AVR 器件会根据索引号从 avrDeviceConst 常量表补全运行参数。
 */
void offlinePgmerInitWith(stkDeviceIdentity_t* di)
{
    uint16_t i;

    g_activeDeviceParams.device_arch = di->arch;
    g_activeDeviceParams.device_index = di->index;
    for(i=0;i<STK_PARAM_ITEM_ID_LEN;i++)
    {
        g_activeDeviceParams.item_id[i] = di->itemId[i];
    }
    for(i=0;i<STK_PARAM_ITEM_DESC_LEN;i++)
    {
        g_activeDeviceParams.item_desc[i] = di->itemDesc[i];
    }

    if(g_activeDeviceParams.device_arch == STK_MCU_ARCH_AVR)
    {
        (void)avrFindDeviceByIndex(g_activeDeviceParams.device_index,
                &g_activeDeviceParams.device_params.avrParam);
    }
    else //STK_MCU_ARCH_PIC
    {
        (void)pic8FindDeviceByIndex(g_activeDeviceParams.device_index,
                &g_activeDeviceParams.device_params.picParam);
    }
}

/* 设置 STK500 扩展工作模式。 */
uint8_t stkSetWorkMode(uint8_t mode)
{
    if (mode > STK500_WORK_MODE_REPLAY)
        return 0U;

    g_stkWorkMode = mode;
    return 1U;
}


/* 获取当前 STK500 扩展工作模式。 */
uint8_t stkGetWorkMode(void)
{
    return g_stkWorkMode;
}


/*
 * 判断当前是否需要真实执行在线烧录。
 * 在线模式和在线+记录模式返回 1, 模拟/纯记录模式返回 0。
 */
uint8_t stkIsOnlineMode(void)
{
    return (g_stkWorkMode == STK500_WORK_MODE_ONLINE ||
            g_stkWorkMode == STK500_WORK_MODE_REPLAY) ? 1U : 0U;
}


/*
 * 判断当前是否需要记录离线数据。
 * 纯记录模式和在线+记录模式返回 1。
 */
uint8_t stkIsRecordMode(void)
{
    return (g_stkWorkMode == STK500_WORK_MODE_RECORD) ? 1U : 0U;
}


/*
 * 开始记录一个 raw STK500 离线包。
 * 通常由 CMD_SET_PROG_STATE: START_PROG 触发。
 */
uint8_t offlinePgmerRawBegin(const stkDeviceIdentity_t *identity)
{
    uint16_t idx;
    uint32_t addr;

    if (!stkIsRecordMode())
        return 0U;

    if (g_rawCapture.active)
        return 0U;

    offlinePgmerInitStorageOnce();
    offlineRawLoadIndex();

    idx = offlineRawAllocIndex();
    if (idx == 0xFFFFU)
        return 1U;

    addr = offlineRawAllocFlashAddr();
    memset(&g_rawCapture, 0, sizeof(g_rawCapture));
    g_rawCapture.active = 1U;
    g_rawCapture.index = idx;
    g_rawCapture.file_addr = addr;
    g_rawCapture.write_offset = sizeof(offline_raw_package_header_t);
    g_rawCapture.erased_until = addr;
    g_rawCapture.running_crc = 0U;

    memset(&g_rawCapture.header, 0, sizeof(g_rawCapture.header));
    g_rawCapture.header.magic = OFFLINE_RAW_MAGIC;
    g_rawCapture.header.version = OFFLINE_RAW_VERSION;
    g_rawCapture.header.header_size = sizeof(offline_raw_package_header_t);
    g_rawCapture.header.package_index = idx;
    g_rawCapture.header.package_state = OFFLINE_PACKAGE_WRITING;
    g_rawCapture.header.packet_area_offset = sizeof(offline_raw_package_header_t);
    if (identity != 0)
        memcpy(&g_rawCapture.header.identity, identity, sizeof(stkDeviceIdentity_t));

    g_offlinePackageIndex[idx].used = 1U;
    g_offlinePackageIndex[idx].package_state = OFFLINE_PACKAGE_WRITING;
    g_offlinePackageIndex[idx].package_index = idx;
    g_offlinePackageIndex[idx].flash_addr = addr;
    g_offlinePackageIndex[idx].total_size = sizeof(offline_raw_package_header_t);
    g_offlinePackageIndex[idx].packet_count = 0U;
    g_offlinePackageIndex[idx].crc32 = 0U;
    memcpy(&g_offlinePackageIndex[idx].identity,
           &g_rawCapture.header.identity,
           sizeof(stkDeviceIdentity_t));

    offlineRawEraseForWrite(addr, FLASH_SECTOR_SIZE);
    SPI_Flash_Write((const uint8_t *)&g_rawCapture.header,
                    addr,
                    sizeof(offline_raw_package_header_t));
    return 0U;
}

/*
 * 向当前 raw 离线包追加一个上位机发来的完整 STK500 帧。
 * frame 保存的是协议原始帧, 便于离线执行时复用在线编程同一套解析流程。
 */
uint8_t offlinePgmerRawAppendRxPacket(const uint8_t *frame, uint16_t frameLen)
{
    offline_raw_packet_header_t ph;
    uint32_t write_addr;

    if (!stkIsRecordMode() || !g_rawCapture.active)
        return 0U;

    if (frame == 0 || frameLen == 0U)
        return 1U;

    memset(&ph, 0, sizeof(ph));
    ph.frame_len = frameLen;
    ph.payload_len = (frameLen >= 6U) ? (uint16_t)(frameLen - 6U) : frameLen;
    ph.cmd = (frameLen > 5U) ? frame[5] : 0U;
    ph.flags = OFFLINE_RAW_PACKET_RX;
    ph.seq = g_rawCapture.packet_count;
    ph.crc32 = offlineCalcSum32(frame, frameLen);

    /* The host sends SET_PARAMETER(DEVICE_IDENTITY) after SET_PROG_STATE,
     * so the package identity captured at Begin may still be stale.
     * Refresh it from the recorded identity frame so replay dispatches
     * to the correct AVR/PIC group. */
    if (ph.cmd == STK_CMD_SET_PARAMETER && frameLen >= 12U &&
        frame[6] == STK_PARAM_DEVICE_IDENTITY)
    {
        g_rawCapture.header.identity.arch = frame[7];
        g_rawCapture.header.identity.index = (uint16_t)frame[8] |
                                             ((uint16_t)frame[9] << 8);
    }

    write_addr = g_rawCapture.file_addr + g_rawCapture.write_offset;
    offlineRawEraseForWrite(write_addr, (uint32_t)sizeof(ph) + frameLen);
    SPI_Flash_Write((const uint8_t *)&ph, write_addr, sizeof(ph));
    write_addr += sizeof(ph);
    SPI_Flash_Write(frame, write_addr, frameLen);

    g_rawCapture.write_offset += (uint32_t)sizeof(ph) + frameLen;
    g_rawCapture.packet_count++;
    g_rawCapture.running_crc += ph.crc32;
    return 0U;
}

/*
 * 结束 raw STK500 离线包记录。
 * 这里会回写包头, 并把索引状态从 WRITING 改成 VALID。
 */
uint8_t offlinePgmerRawEnd(void)
{
    offline_raw_package_commit_t commit;
    uint16_t idx;
    uint32_t write_addr;

    if (!g_rawCapture.active)
        return 0U;

    idx = g_rawCapture.index;
    memset(&commit, 0, sizeof(commit));
    commit.magic = OFFLINE_RAW_COMMIT_MAGIC;
    commit.version = OFFLINE_RAW_VERSION;
    commit.commit_size = sizeof(offline_raw_package_commit_t);
    commit.package_index = idx;
    commit.package_state = OFFLINE_PACKAGE_VALID;
    memcpy(&commit.identity,
           &g_rawCapture.header.identity,
           sizeof(stkDeviceIdentity_t));
    commit.packet_count = g_rawCapture.packet_count;
    commit.packet_area_size =
        g_rawCapture.write_offset - sizeof(offline_raw_package_header_t);
    commit.total_size =
        g_rawCapture.write_offset + sizeof(offline_raw_package_commit_t);
    commit.crc32 = g_rawCapture.running_crc;

    write_addr = g_rawCapture.file_addr + g_rawCapture.write_offset;
    offlineRawEraseForWrite(write_addr, sizeof(offline_raw_package_commit_t));
    SPI_Flash_Write((const uint8_t *)&commit,
                    write_addr,
                    sizeof(offline_raw_package_commit_t));

    g_offlinePackageIndex[idx].used = 1U;
    g_offlinePackageIndex[idx].package_state = OFFLINE_PACKAGE_VALID;
    g_offlinePackageIndex[idx].package_index = idx;
    g_offlinePackageIndex[idx].flash_addr = g_rawCapture.file_addr;
    g_offlinePackageIndex[idx].total_size = commit.total_size;
    g_offlinePackageIndex[idx].packet_area_size = commit.packet_area_size;
    g_offlinePackageIndex[idx].packet_count = commit.packet_count;
    g_offlinePackageIndex[idx].crc32 = commit.crc32;
    memcpy(&g_offlinePackageIndex[idx].identity,
           &commit.identity,
           sizeof(stkDeviceIdentity_t));

    memset(&g_rawCapture, 0, sizeof(g_rawCapture));
    return 0U;
}

/*
 * Record-mode read-back: parse the in-progress offline package stored on the
 * board flash and return the data recorded by the matching PROGRAM frame.
 * readCmd   : STK_CMD_READ_FLASH_ISP / READ_EEPROM_ISP / READ_FUSE_ISP / READ_LOCK_ISP
 * addr      : current LOAD_ADDRESS value (used for flash/eeprom)
 * readFrame : the incoming READ STK500 frame (used for fuse/lock slot match)
 * out/outCap: destination buffer and capacity
 * Returns the number of bytes filled (0 = no matching record, caller fills 0xFF). */
uint16_t offlinePgmerRawReadBack(uint8_t readCmd, uint32_t addr,
                                  const uint8_t *readFrame,
                                  uint8_t *out, uint16_t outCap)
{
    uint32_t cursor;
    uint32_t end;
    uint32_t curAddr = 0U;
    uint16_t filled = 0U;
    offline_raw_packet_header_t ph;

    if (!g_rawCapture.active || readFrame == 0 || out == 0 || outCap == 0U)
        return 0U;


    end = g_rawCapture.file_addr + g_rawCapture.write_offset;
    cursor = g_rawCapture.file_addr + sizeof(offline_raw_package_header_t);

    while ((end - cursor) >= sizeof(ph))
    {
        uint8_t cmd;
        uint16_t frameLen;

        SPI_Flash_Read((uint8_t *)&ph, cursor, sizeof(ph));
        if (ph.frame_len < 6U || ph.frame_len > BUFFER_SIZE ||
            (end - cursor - sizeof(ph)) < ph.frame_len)
            break;

        SPI_Flash_Read(g_readbackFrame, cursor + sizeof(ph), ph.frame_len);
        frameLen = ph.frame_len;
        cmd = ph.cmd;

        if (cmd == STK_CMD_LOAD_ADDRESS && frameLen >= 10U)
        {
            curAddr = ((uint32_t)g_readbackFrame[6] << 24) |
                      ((uint32_t)g_readbackFrame[7] << 16) |
                      ((uint32_t)g_readbackFrame[8] << 8) |
                      g_readbackFrame[9];
        }
        else if (cmd == STK_CMD_PROGRAM_FLASH_ISP && readCmd == STK_CMD_READ_FLASH_ISP)
        {
            /* Flash LOAD_ADDRESS is a word address; byte offset = words * 2.
             * avrdude may read back a large block spanning several pages, so
             * splice every matching page into the caller buffer. */
            uint16_t n = ((uint16_t)g_readbackFrame[6] << 8) | g_readbackFrame[7];
            if (n != 0U && curAddr >= addr && (uint16_t)(15U + n) <= frameLen)
            {
                uint32_t byteOff = (curAddr - addr) * 2U;
                if (byteOff < outCap)
                {
                    uint16_t cp = (uint16_t)((n > (outCap - (uint16_t)byteOff)) ?
                                              (outCap - (uint16_t)byteOff) : n);
                    memcpy(&out[byteOff], &g_readbackFrame[15], cp);
                    if ((uint16_t)(byteOff + cp) > filled)
                        filled = (uint16_t)(byteOff + cp);
                }
            }
        }
        else if (cmd == STK_CMD_PROGRAM_EEPROM_ISP && readCmd == STK_CMD_READ_EEPROM_ISP)
        {
            /* EEPROM uses byte addresses. */
            uint16_t n = ((uint16_t)g_readbackFrame[6] << 8) | g_readbackFrame[7];
            if (n != 0U && curAddr >= addr && (uint16_t)(15U + n) <= frameLen)
            {
                uint32_t byteOff = curAddr - addr;
                if (byteOff < outCap)
                {
                    uint16_t cp = (uint16_t)((n > (outCap - (uint16_t)byteOff)) ?
                                              (outCap - (uint16_t)byteOff) : n);
                    memcpy(&out[byteOff], &g_readbackFrame[15], cp);
                    if ((uint16_t)(byteOff + cp) > filled)
                        filled = (uint16_t)(byteOff + cp);
                }
            }
        }
        else if (cmd == STK_CMD_PROGRAM_FUSE_ISP && readCmd == STK_CMD_READ_FUSE_ISP)
        {
            /* AVR program-fuse opcode: 0xAC A0/A8/A4 <x> <data>. The data bits
             * ("i" in the avrdude opcode) live in the 4th byte. avrdude verifies
             * with its bitmask, so returning byte 3 works for 8-bit fuses and
             * small-bitmask parts (efuse) alike. */
            uint8_t wslot = (uint8_t)((g_readbackFrame[7] == 0xA0U) ? 1U :
                              (g_readbackFrame[7] == 0xA8U) ? 2U :
                              (g_readbackFrame[7] == 0xA4U) ? 3U : 0U);
            uint8_t rslot = 0U;
            if (readFrame[7] == 0x50U && readFrame[8] == 0x00U) rslot = 1U;
            else if (readFrame[7] == 0x58U && readFrame[8] == 0x08U) rslot = 2U;
            else if (readFrame[7] == 0x50U && readFrame[8] == 0x08U) rslot = 3U;
            if (wslot != 0U && wslot == rslot)
            {
                out[0] = g_readbackFrame[9];
                return 1U;
            }
        }
        else if (cmd == STK_CMD_PROGRAM_LOCK_ISP && readCmd == STK_CMD_READ_LOCK_ISP)
        {
            if (g_readbackFrame[6] == 0xACU && g_readbackFrame[7] == 0xE0U)
            {
                out[0] = g_readbackFrame[9];
                return 1U;
            }
        }
        else if (cmd == STK_CMD_PROGRAM_FLASH_HVSP && readCmd == STK_CMD_READ_FLASH_HVSP)
        {
            /* HVSP flash: word LOAD_ADDRESS like ISP; data starts at frame[10]. */
            uint16_t n = ((uint16_t)g_readbackFrame[6] << 8) | g_readbackFrame[7];
            if (n != 0U && curAddr >= addr && (uint16_t)(10U + n) <= frameLen)
            {
                uint32_t byteOff = (curAddr - addr) * 2U;
                if (byteOff < outCap)
                {
                    uint16_t cp = (uint16_t)((n > (outCap - (uint16_t)byteOff)) ?
                                              (outCap - (uint16_t)byteOff) : n);
                    memcpy(&out[byteOff], &g_readbackFrame[10], cp);
                    if ((uint16_t)(byteOff + cp) > filled)
                        filled = (uint16_t)(byteOff + cp);
                }
            }
        }
        else if (cmd == STK_CMD_PROGRAM_EEPROM_HVSP && readCmd == STK_CMD_READ_EEPROM_HVSP)
        {
            /* HVSP EEPROM: byte addresses, data starts at frame[10]. */
            uint16_t n = ((uint16_t)g_readbackFrame[6] << 8) | g_readbackFrame[7];
            if (n != 0U && curAddr >= addr && (uint16_t)(10U + n) <= frameLen)
            {
                uint32_t byteOff = curAddr - addr;
                if (byteOff < outCap)
                {
                    uint16_t cp = (uint16_t)((n > (outCap - (uint16_t)byteOff)) ?
                                              (outCap - (uint16_t)byteOff) : n);
                    memcpy(&out[byteOff], &g_readbackFrame[10], cp);
                    if ((uint16_t)(byteOff + cp) > filled)
                        filled = (uint16_t)(byteOff + cp);
                }
            }
        }
        else if (cmd == STK_CMD_PROGRAM_FUSE_HVSP && readCmd == STK_CMD_READ_FUSE_HVSP)
        {
            /* HVSP fuse: [6]=fuseAddress(0=lfuse,1=hfuse,2=efuse), [7]=value. */
            if (g_readbackFrame[6] == readFrame[6])
            {
                out[0] = g_readbackFrame[7];
                return 1U;
            }
        }
        else if (cmd == STK_CMD_PROGRAM_LOCK_HVSP && readCmd == STK_CMD_READ_LOCK_HVSP)
        {
            out[0] = g_readbackFrame[7];
            return 1U;
        }
        else if (cmd == STK_CMD_PROGRAM_FLASH_ICSP && readCmd == STK_CMD_READ_FLASH_ICSP)
        {
            /* ICSP flash: word LOAD_ADDRESS like ISP; data starts at frame[10]. */
            uint16_t n = (uint16_t)g_readbackFrame[6] | ((uint16_t)g_readbackFrame[7] << 8);
            if (n != 0U && curAddr >= addr && (uint16_t)(10U + n * 2U) <= frameLen)
            {
                uint32_t wordOff = curAddr - addr;
                if (wordOff < (outCap / 2U))
                {
                    uint16_t cpWords = (uint16_t)((n > (outCap / 2U - (uint16_t)wordOff)) ?
                                                  (outCap / 2U - (uint16_t)wordOff) : n);
                    uint16_t cpBytes = (uint16_t)(cpWords * 2U);
                    memcpy(&out[(uint16_t)wordOff * 2U], &g_readbackFrame[10], cpBytes);
                    if ((uint16_t)((uint16_t)wordOff * 2U + cpBytes) > filled)
                        filled = (uint16_t)((uint16_t)wordOff * 2U + cpBytes);
                }
            }
        }
        else if (cmd == STK_CMD_PROGRAM_EEPROM_ICSP && readCmd == STK_CMD_READ_EEPROM_ICSP)
        {
            /* ICSP EEPROM: byte addresses, data starts at frame[10]. */
            uint16_t n = (uint16_t)g_readbackFrame[6] | ((uint16_t)g_readbackFrame[7] << 8);
            if (n != 0U && curAddr >= addr && (uint16_t)(10U + n) <= frameLen)
            {
                uint32_t byteOff = curAddr - addr;
                if (byteOff < outCap)
                {
                    uint16_t cp = (uint16_t)((n > (outCap - (uint16_t)byteOff)) ?
                                              (outCap - (uint16_t)byteOff) : n);
                    memcpy(&out[byteOff], &g_readbackFrame[10], cp);
                    if ((uint16_t)(byteOff + cp) > filled)
                        filled = (uint16_t)(byteOff + cp);
                }
            }
        }
        else if (cmd == STK_CMD_PROGRAM_USER_ID_ICSP && readCmd == STK_CMD_READ_USER_ID_ICSP)
        {
            /* ICSP userid: word LOAD_ADDRESS, data starts at frame[10]. */
            uint16_t n = (uint16_t)g_readbackFrame[6] | ((uint16_t)g_readbackFrame[7] << 8);
            if (n != 0U && curAddr >= addr && (uint16_t)(10U + n * 2U) <= frameLen)
            {
                uint32_t wordOff = curAddr - addr;
                if (wordOff < (outCap / 2U))
                {
                    uint16_t cpWords = (uint16_t)((n > (outCap / 2U - (uint16_t)wordOff)) ?
                                                  (outCap / 2U - (uint16_t)wordOff) : n);
                    uint16_t cpBytes = (uint16_t)(cpWords * 2U);
                    memcpy(&out[(uint16_t)wordOff * 2U], &g_readbackFrame[10], cpBytes);
                    if ((uint16_t)((uint16_t)wordOff * 2U + cpBytes) > filled)
                        filled = (uint16_t)((uint16_t)wordOff * 2U + cpBytes);
                }
            }
        }
        else if (cmd == STK_CMD_PROGRAM_CONFIG_ICSP && readCmd == STK_CMD_READ_CONFIG_ICSP)
        {
            /* ICSP config is index-addressed; copy the recorded words in order. */
            uint16_t n = (uint16_t)g_readbackFrame[6] | ((uint16_t)g_readbackFrame[7] << 8);
            if (n != 0U && (uint16_t)(10U + n * 2U) <= frameLen)
            {
                uint16_t cpBytes = (uint16_t)((n * 2U > outCap) ? outCap : n * 2U);
                memcpy(out, &g_readbackFrame[10], cpBytes);
                if (cpBytes > filled)
                    filled = cpBytes;
            }
        }

        cursor += sizeof(ph) + frameLen;
    }

    return filled;
}

/* 获取 raw 离线包总体信息, 供上位机查询当前 Flash 中已有多少个离线包。 */
uint8_t offlinePgmerGetOfflineInfo(offline_package_info_t *info)
{
    uint16_t i;
    uint16_t count = 0U;

    if (info == 0)
        return 1U;

    offlinePgmerInitStorageOnce();
    offlineRawLoadIndex();
    for (i = 0U; i < OFFLINE_MAX_PACKAGES; i++)
    {
        if (offlineRawIsValidIndex(i))
            count++;
    }

    info->package_count = count;
    info->max_count = OFFLINE_MAX_PACKAGES;
    if (offlinePgmerGetActivePackage(&info->active_index) != 0U)
        info->active_index = 0xFFFFU;

    return 0U;
}

/* 获取指定序号 raw 离线包的摘要信息。 */
uint8_t offlinePgmerGetPackageSummary(uint16_t index, offline_package_index_t *summary)
{
    if (summary == 0)
        return 1U;

    offlinePgmerInitStorageOnce();
    offlineRawLoadIndex();
    if (index >= OFFLINE_MAX_PACKAGES)
        return 1U;

    memcpy(summary, &g_offlinePackageIndex[index], sizeof(offline_package_index_t));
    return 0U;
}

/*
 * 设置离线模式下默认执行的激活包。
 * 激活包编号写入 EEPROM, 掉电后仍可保留。
 */
uint8_t offlinePgmerSetActivePackage(uint16_t index)
{
    offline_active_record_t rec;
    uint16_t lastSlot = 0U;
    uint16_t writeSlot = 0U;

    offlinePgmerInitStorageOnce();
    offlineRawLoadIndex();
    if (!offlineRawIsValidIndex(index))
        return 1U;

    memset(&rec, 0, sizeof(rec));
    rec.magic = OFFLINE_ACTIVE_MAGIC;
    rec.version = OFFLINE_RAW_VERSION;
    rec.active_index = index;
    rec.active_flash_addr = g_offlinePackageIndex[index].flash_addr;
    rec.active_crc32 = g_offlinePackageIndex[index].crc32;
    rec.crc32 = offlineActiveCalcCrc(&rec);

    if (OFFLINE_ACTIVE_LOG_SLOT_COUNT == 0U)
        return 1U;

    if (offlineActiveFindLatestRecord(0, &lastSlot))
        writeSlot = (uint16_t)(lastSlot + 1U);

    if (writeSlot >= OFFLINE_ACTIVE_LOG_SLOT_COUNT)
    {
        offlineActiveEraseLog();
        writeSlot = 0U;
    }

    SPI_EEPROM_Write(OFFLINE_ACTIVE_LOG_ADDR +
                     ((uint32_t)writeSlot * sizeof(offline_active_record_t)),
                     (const uint8_t *)&rec,
                     sizeof(rec));
    return 0U;
}

/* 从 EEPROM 读取当前激活包编号。 */
uint8_t offlinePgmerGetActivePackage(uint16_t *index)
{
    if (index == 0)
        return 1U;

#if OFFLINE_SINGLE_PACKET_MODE
    /* Single-package debug mode: the one package is always active. */
    *index = 0U;
    return 0U;
#else
    offline_active_record_t rec;

    if (!offlineActiveFindLatestRecord(&rec, 0))
        return 1U;

    *index = rec.active_index;
    return 0U;
#endif
}



