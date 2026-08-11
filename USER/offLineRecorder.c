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
#define OFFLINE_RAW_INDEX_ADDR          0UL
#define OFFLINE_RAW_DATA_START_ADDR     FLASH_SECTOR_SIZE

/* EEPROM 中保存当前激活离线包号的位置。 */
#define OFFLINE_ACTIVE_EEPROM_ADDR      0U

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
static uint32_t offlineAlignSector(uint32_t addr)
{
    return (addr + FLASH_SECTOR_SIZE - 1UL) & ~(FLASH_SECTOR_SIZE - 1UL);
}

/* 从 SPI Flash 加载 raw 离线包索引表, 同一次运行中只加载一次。 */
static void offlineRawLoadIndex(void)
{
    if (g_rawIndexLoaded)
        return;

    SPI_Flash_Read((uint8_t *)g_offlinePackageIndex,
                   OFFLINE_RAW_INDEX_ADDR,
                   (uint16_t)sizeof(g_offlinePackageIndex));
    g_rawIndexLoaded = 1U;
}

/* 将 raw 离线包索引表写回 SPI Flash。 */
static void offlineRawSaveIndex(void)
{
    SPI_Flash_Erase_Sector(OFFLINE_RAW_INDEX_ADDR / FLASH_SECTOR_SIZE);
    SPI_Flash_Write_NoCheck((const uint8_t *)g_offlinePackageIndex,
                            OFFLINE_RAW_INDEX_ADDR,
                            (uint16_t)sizeof(g_offlinePackageIndex));
}

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
}

/* 为新的 raw 离线包分配 SPI Flash 起始地址。 */
static uint32_t offlineRawAllocFlashAddr(void)
{
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
    uint8_t *pByte = (uint8_t *)&(g_activeDeviceParams.item_id);

    g_activeDeviceParams.device_arch = di->arch;
    g_activeDeviceParams.device_index = di->index;
    for(i=0;i<STK_PARAM_ITEM_ID_LEN;i++)
    {
        pByte[i] = di->itemId[i];
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
        (void)picFindDeviceByIndex(g_activeDeviceParams.device_index,
                &g_activeDeviceParams.device_params.picParam);
    }
}

/* 设置 STK500 扩展工作模式。 */
uint8_t stkSetWorkMode(uint8_t mode)
{
    if (mode > STK500_WORK_MODE_ONLINE_RECORD)
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
            g_stkWorkMode == STK500_WORK_MODE_ONLINE_RECORD) ? 1U : 0U;
}


/*
 * 判断当前是否需要记录离线数据。
 * 纯记录模式和在线+记录模式返回 1。
 */
uint8_t stkIsRecordMode(void)
{
    return (g_stkWorkMode == STK500_WORK_MODE_RECORD ||
            g_stkWorkMode == STK500_WORK_MODE_ONLINE_RECORD) ? 1U : 0U;
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

    g_offlinePackageIndex[idx].flash_addr = addr;
    g_offlinePackageIndex[idx].total_size = sizeof(offline_raw_package_header_t);
    g_offlinePackageIndex[idx].packet_count = 0U;
    g_offlinePackageIndex[idx].crc32 = 0U;
    memcpy(&g_offlinePackageIndex[idx].identity,
           &g_rawCapture.header.identity,
           sizeof(stkDeviceIdentity_t));
    offlineRawSaveIndex();

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
    uint16_t idx;

    if (!g_rawCapture.active)
        return 0U;

    idx = g_rawCapture.index;
    g_rawCapture.header.package_state = OFFLINE_PACKAGE_VALID;
    g_rawCapture.header.packet_count = g_rawCapture.packet_count;
    g_rawCapture.header.packet_area_size =
        g_rawCapture.write_offset - sizeof(offline_raw_package_header_t);
    g_rawCapture.header.total_size = g_rawCapture.write_offset;
    g_rawCapture.header.crc32 = g_rawCapture.running_crc;

    SPI_Flash_Write((const uint8_t *)&g_rawCapture.header,
                    g_rawCapture.file_addr,
                    sizeof(offline_raw_package_header_t));

    g_offlinePackageIndex[idx].used = 1U;
    g_offlinePackageIndex[idx].package_state = OFFLINE_PACKAGE_VALID;
    g_offlinePackageIndex[idx].package_index = idx;
    g_offlinePackageIndex[idx].flash_addr = g_rawCapture.file_addr;
    g_offlinePackageIndex[idx].total_size = g_rawCapture.write_offset;
    g_offlinePackageIndex[idx].packet_count = g_rawCapture.packet_count;
    g_offlinePackageIndex[idx].crc32 = g_rawCapture.running_crc;
    memcpy(&g_offlinePackageIndex[idx].identity,
           &g_rawCapture.header.identity,
           sizeof(stkDeviceIdentity_t));
    offlineRawSaveIndex();

    memset(&g_rawCapture, 0, sizeof(g_rawCapture));
    return 0U;
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

    offlinePgmerInitStorageOnce();
    if (!offlineRawIsValidIndex(index))
        return 1U;

    memset(&rec, 0, sizeof(rec));
    rec.magic = OFFLINE_ACTIVE_MAGIC;
    rec.version = OFFLINE_RAW_VERSION;
    rec.active_index = index;
    rec.active_flash_addr = g_offlinePackageIndex[index].flash_addr;
    rec.active_crc32 = g_offlinePackageIndex[index].crc32;
    rec.crc32 = offlineActiveCalcCrc(&rec);

    SPI_EEPROM_Write(OFFLINE_ACTIVE_EEPROM_ADDR,
                     (const uint8_t *)&rec,
                     sizeof(rec));
    return 0U;
}

/* 从 EEPROM 读取当前激活包编号。 */
uint8_t offlinePgmerGetActivePackage(uint16_t *index)
{
    offline_active_record_t rec;

    if (index == 0)
        return 1U;

    SPI_EEPROM_Read(OFFLINE_ACTIVE_EEPROM_ADDR,
                    (uint8_t *)&rec,
                    sizeof(rec));
    if (rec.magic != OFFLINE_ACTIVE_MAGIC ||
        rec.version != OFFLINE_RAW_VERSION ||
        rec.crc32 != offlineActiveCalcCrc(&rec))
    {
        return 1U;
    }

    *index = rec.active_index;
    return 0U;
}



