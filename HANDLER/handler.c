/*
 * Handler 自动机模块 — 机械手信号控制实现
 *
 * 支持 SOT / BUSY / OK / NG 四路信号,
 * 通过状态机实现全自动编程器的机械手握手协议。
 *
 * 移植自: DFMProgrammer/Product_VET6/STM32F103VET6_DFM_Burner_Developer_V5/Src/handler.c
 * 原文件使用 STM32 HAL 库 (HAL_GPIO / HAL_Delay / HAL_GetTick),
 * 现改为本项目裸机寄存器风格:
 *   - 引脚操作: PORT_OUT / PORT_IN / PORT_RCC_CLK (定义于 sys.h 和 Hardware_Config.h)
 *   - 延时:     delay_ms (定义于 SYSTEM/sys/delay.h) 替代 HAL_Delay
 *   - 时间戳:   无符号 32-bit 毫秒计数器 (由 TIM6 1ms 中断提供, 或使用全局变量)
 *
 * 适配说明:
 *   1. SOT — PD1 (输入), 上/下拉 — PD0 (输出)
 *   2. BUSY — PA8 (输出)
 *   3. OK — PC6 (输出)
 *   4. NG — PC7 (输出)
 *   5. EOT — 本项目无独立引脚, EOT_SET/CLR 为空宏
 *   6. Flash 存储 — 调用 SPI_Flash_Mount/Write/Read (已在当前项目中实现)
 *   7. 统计存储 — 调用 SPI_FlashSaveStatistics / SPI_FlashLoadStatistics (待外部实现)
 */

#include "handler.h"
#include "flash.h"
#include "delay.h"
#include "timer.h"
#include "eeprom.h"
#include "usart.h"
#include <string.h>

/* ── 模块内全局变量 ───────────────────────────────────────────── */
static HandlerConfigerType usedHandler = {1, 1, 1, 1, 1, 10, 3000};
statisticsType usedStatistics = {0, 0, 0, 0, 0, 0};


#define HANDLER_CONFIG_RECORD_SIZE     15U
#define HANDLER_CONFIG_MAGIC0          0x48U
#define HANDLER_CONFIG_MAGIC1          0x43U
#define HANDLER_CONFIG_VERSION         0x01U

static const uint8_t handlerCfgDefault[HANDLER_CONFIG_DATA_SIZE] = {1U, 1U, 1U, 1U, 1U, 10U, 0U, 0xB8U, 0x0BU};
static uint8_t handlerCfgBuff[HANDLER_CONFIG_DATA_SIZE];
static uint8_t handlerCfgRecord[HANDLER_CONFIG_RECORD_SIZE];
#define HANDLER_STATISTICS_PAYLOAD_SIZE  24U
#define HANDLER_STATISTICS_RECORD_MAGIC  0x5354U
#define HANDLER_STATISTICS_RECORD_VERSION 1U
#define HANDLER_STATISTICS_CRC_OFFSET    30U

/* statisticsType has six 32-bit counters; do not persist its padded C layout. */
typedef char HandlerStatisticsPayloadMustBe24[(sizeof(statisticsType) == HANDLER_STATISTICS_PAYLOAD_SIZE) ? 1 : -1];
typedef char HandlerStatisticsRegionMustUseWholeSlots[
    (((HW_HANDLER_STATISTICS_EEPROM_END_ADDR - HW_HANDLER_STATISTICS_EEPROM_START_ADDR + 1UL) %
      HW_HANDLER_STATISTICS_RECORD_SIZE) == 0UL) ? 1 : -1];
typedef char HandlerStatisticsStartMustBePageAligned[
    ((HW_HANDLER_STATISTICS_EEPROM_START_ADDR % HW_HANDLER_STATISTICS_RECORD_SIZE) == 0UL) ? 1 : -1];

static uint8_t g_statisticsRecord[HW_HANDLER_STATISTICS_RECORD_SIZE];
static uint16_t g_statisticsNextSlot;
static uint16_t g_statisticsNextSequence;
#if HANDLER_DEBUG_TRACE
static void HandlerTraceU32(uint32_t value)
{
    uart1_WriteHex16((uint16_t)(value >> 16));
    uart1_WriteHex16((uint16_t)value);
}

static void HandlerTraceConfig(const char *tag, uint16_t crc)
{
    uart1_WriteString("[HND] ");
    uart1_WriteString(tag);
    uart1_WriteString(" crc=0x");
    uart1_WriteHex16(crc);
    uart1_WriteString("\r\n");
}

static void HandlerTraceStatistics(const char *tag, uint16_t slot, uint16_t sequence)
{
    uart1_WriteString("[HND] ");
    uart1_WriteString(tag);
    uart1_WriteString(" slot=");
    uart1_WriteDec(slot);
    uart1_WriteString(" seq=0x");
    uart1_WriteHex16(sequence);
    uart1_WriteString(" total=0x");
    HandlerTraceU32(usedStatistics.realTotal);
    uart1_WriteString(" pass=0x");
    HandlerTraceU32(usedStatistics.realPassed);
    uart1_WriteString(" fail=0x");
    HandlerTraceU32(usedStatistics.realFaild);
    uart1_WriteString("\r\n");
}
#endif

static uint16_t HandlerCrc16(const uint8_t *pData, uint16_t len)
{
    uint16_t crc = 0xFFFFU;
    uint8_t bit;

    while (len-- != 0U)
    {
        crc ^= *pData++;
        for (bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 1U) != 0U)
                crc = (crc >> 1U) ^ 0xA001U;
            else
                crc >>= 1U;
        }
    }

    return crc;
}

static void HandlerConfigEncode(uint8_t *pData)
{
    pData[0] = usedHandler.sotLevel;
    pData[1] = usedHandler.eotLevel;
    pData[2] = usedHandler.busyLevel;
    pData[3] = usedHandler.passLevel;
    pData[4] = usedHandler.ngLevel;
    pData[5] = (uint8_t)usedHandler.delayMsBinToEot;
    pData[6] = (uint8_t)(usedHandler.delayMsBinToEot >> 8);
    pData[7] = (uint8_t)usedHandler.delayMsMinTestTime;
    pData[8] = (uint8_t)(usedHandler.delayMsMinTestTime >> 8);
}

static void HandlerConfigDecode(const uint8_t *pData)
{
    usedHandler.sotLevel = pData[0];
    usedHandler.eotLevel = pData[1];
    usedHandler.busyLevel = pData[2];
    usedHandler.passLevel = pData[3];
    usedHandler.ngLevel = pData[4];
    usedHandler.delayMsBinToEot = (uint16_t)pData[5] | ((uint16_t)pData[6] << 8);
    usedHandler.delayMsMinTestTime = (uint16_t)pData[7] | ((uint16_t)pData[8] << 8);
}

/*
 * 毫秒级时间戳获取
 *
 * 原 HAL 使用 HAL_GetTick() (基于 SysTick 1ms 中断)。
 * 当前项目使用 TIM6 1ms 溢出中断驱动 timer.c/timer.h,
 * 可增加一个全局毫秒计数器, 或直接使用 SysTick->VAL 近似。
 *
 * 此处提供一个本地实现, 使用 volatile 全局变量 g_msTick (需在 timer.c 中声明)。
 * 若实际项目中无此变量, 可用 PORT_RCC_CLK + delay_us 近似, 但精度差。
 */

/* ── 配置读写 ─────────────────────────────────────────────────── */

/**
 * @brief  从缓冲区更新 Handler 电平设置
 * @param  pBuff [输入] 包含 10 字节配置数据 (sot/eot/busy/pass/ng level + delay)
 */
void HandlerChangeLevel(uint8_t* pBuff)
{
    uint8_t* pLevel = pBuff;

    usedHandler.sotLevel         = *pLevel++;
    usedHandler.eotLevel         = *pLevel++;
    usedHandler.busyLevel        = *pLevel++;
    usedHandler.passLevel        = *pLevel++;
    usedHandler.ngLevel          = *pLevel++;
    usedHandler.delayMsBinToEot  = (*pLevel) | (*(pLevel + 1) << 8);
    pLevel += 2;
    usedHandler.delayMsMinTestTime = (*pLevel) | (*(pLevel + 1) << 8);

    /* 根据 SOT 电平设置上/下拉方向 */
    if (usedHandler.sotLevel == 0)
    {
        HANDLER_SOT_SET_UP_LOAD;   /* SOT 低有效 → 空闲时上拉 */
    }
    else
    {
        HANDLER_SOT_SET_DW_LOAD;   /* SOT 高有效 → 空闲时下拉 */
    }
}

/**
 * @brief  将 Handler 配置保存到非易失存储 (SPI Flash)
 *
 * 原 HAL 版本使用 stm32 内部 Flash 或 SPI Flash;
 * 本项目 SPI Flash 接口为 SPI_Flash_Mount/Write/Read/DisMount。
 */
void HandlerSaveConfig(void)
{
    uint16_t crc;

    HandlerConfigEncode(handlerCfgBuff);
    handlerCfgRecord[0] = HANDLER_CONFIG_MAGIC0;
    handlerCfgRecord[1] = HANDLER_CONFIG_MAGIC1;
    handlerCfgRecord[2] = HANDLER_CONFIG_VERSION;
    handlerCfgRecord[3] = HANDLER_CONFIG_DATA_SIZE;
    memcpy(&handlerCfgRecord[4], handlerCfgBuff, HANDLER_CONFIG_DATA_SIZE);
    crc = HandlerCrc16(handlerCfgRecord, HANDLER_CONFIG_RECORD_SIZE - 2U);
    handlerCfgRecord[HANDLER_CONFIG_RECORD_SIZE - 2U] = (uint8_t)crc;
    handlerCfgRecord[HANDLER_CONFIG_RECORD_SIZE - 1U] = (uint8_t)(crc >> 8);

    SPI_EEPROM_Write(HW_HANDLER_PARAM_EEPROM_START_ADDR,
                     handlerCfgRecord,
                     HANDLER_CONFIG_RECORD_SIZE);
#if HANDLER_DEBUG_TRACE
    HandlerTraceConfig("cfg save", crc);
#endif
}

/**
 * @brief  从非易失存储读取 Handler 配置
 * @param  pByteBuff [输出/可选] 若 !=NULL, 将读取到的 10 字节拷贝到此缓冲区
 */
void HandlerReadConfig(uint8_t* pByteBuff)
{
    uint16_t storedCrc;
    uint16_t computedCrc;

    SPI_EEPROM_Read(HW_HANDLER_PARAM_EEPROM_START_ADDR,
                    handlerCfgRecord,
                    HANDLER_CONFIG_RECORD_SIZE);
    storedCrc = (uint16_t)handlerCfgRecord[HANDLER_CONFIG_RECORD_SIZE - 2U] |
                ((uint16_t)handlerCfgRecord[HANDLER_CONFIG_RECORD_SIZE - 1U] << 8);
    computedCrc = HandlerCrc16(handlerCfgRecord,
                                     HANDLER_CONFIG_RECORD_SIZE - 2U);
    if (handlerCfgRecord[0] == HANDLER_CONFIG_MAGIC0 &&
        handlerCfgRecord[1] == HANDLER_CONFIG_MAGIC1 &&
        handlerCfgRecord[2] == HANDLER_CONFIG_VERSION &&
        handlerCfgRecord[3] == HANDLER_CONFIG_DATA_SIZE &&
        storedCrc == computedCrc)
    {
        memcpy(handlerCfgBuff, &handlerCfgRecord[4], HANDLER_CONFIG_DATA_SIZE);
        HandlerConfigDecode(handlerCfgBuff);
#if HANDLER_DEBUG_TRACE
        HandlerTraceConfig("cfg load", storedCrc);
#endif
    }
    else
    {
        /* Empty, old-format, or corrupt data restores the known default configuration. */
        memcpy(handlerCfgBuff, handlerCfgDefault, HANDLER_CONFIG_DATA_SIZE);
        HandlerConfigDecode(handlerCfgBuff);
#if HANDLER_DEBUG_TRACE
    HandlerTraceConfig("cfg default", 0U);
#endif
    }

    if (pByteBuff != NULL)
        memcpy(pByteBuff, handlerCfgBuff, HANDLER_CONFIG_DATA_SIZE);
}

/* ── 统计函数 ──────────────────────────────────────────────────── */

/**
 * @brief  将当前统计计数器读出到缓冲区
 * @param  pBuff [输出] 大小 = sizeof(statisticsType)
 */
static uint16_t HandlerReadLe16(const uint8_t *pData)
{
    return (uint16_t)pData[0] | ((uint16_t)pData[1] << 8);
}

static void HandlerWriteLe16(uint8_t *pData, uint16_t value)
{
    pData[0] = (uint8_t)value;
    pData[1] = (uint8_t)(value >> 8);
}

static uint32_t HandlerReadLe32(const uint8_t *pData)
{
    return (uint32_t)pData[0] |
           ((uint32_t)pData[1] << 8) |
           ((uint32_t)pData[2] << 16) |
           ((uint32_t)pData[3] << 24);
}

static void HandlerWriteLe32(uint8_t *pData, uint32_t value)
{
    pData[0] = (uint8_t)value;
    pData[1] = (uint8_t)(value >> 8);
    pData[2] = (uint8_t)(value >> 16);
    pData[3] = (uint8_t)(value >> 24);
}

static void StatisticsEncode(uint8_t *pData)
{
    HandlerWriteLe32(&pData[0], usedStatistics.realTotal);
    HandlerWriteLe32(&pData[4], usedStatistics.realPassed);
    HandlerWriteLe32(&pData[8], usedStatistics.realFaild);
    HandlerWriteLe32(&pData[12], usedStatistics.logicTotal);
    HandlerWriteLe32(&pData[16], usedStatistics.logicPassed);
    HandlerWriteLe32(&pData[20], usedStatistics.logicFaild);
}

static void StatisticsDecode(const uint8_t *pData)
{
    usedStatistics.realTotal = HandlerReadLe32(&pData[0]);
    usedStatistics.realPassed = HandlerReadLe32(&pData[4]);
    usedStatistics.realFaild = HandlerReadLe32(&pData[8]);
    usedStatistics.logicTotal = HandlerReadLe32(&pData[12]);
    usedStatistics.logicPassed = HandlerReadLe32(&pData[16]);
    usedStatistics.logicFaild = HandlerReadLe32(&pData[20]);
}

static uint8_t StatisticsRecordIsValid(const uint8_t *pRecord)
{
    if (HandlerReadLe16(&pRecord[0]) != HANDLER_STATISTICS_RECORD_MAGIC ||
        pRecord[2] != HANDLER_STATISTICS_RECORD_VERSION ||
        pRecord[3] != HANDLER_STATISTICS_PAYLOAD_SIZE)
    {
        return 0U;
    }

    return (HandlerReadLe16(&pRecord[HANDLER_STATISTICS_CRC_OFFSET]) ==
            HandlerCrc16(pRecord, HANDLER_STATISTICS_CRC_OFFSET)) ? 1U : 0U;
}

static uint8_t StatisticsSequenceIsNewer(uint16_t candidate, uint16_t reference)
{
    uint16_t delta = (uint16_t)(candidate - reference);
    return (delta != 0U && delta < 0x8000U) ? 1U : 0U;
}

static void StatisticsLoadLatest(void)
{
    uint16_t slot;
    uint16_t latestSlot = 0U;
    uint16_t latestSequence = 0U;
    uint8_t found = 0U;

    for (slot = 0U; slot < HW_HANDLER_STATISTICS_RECORD_COUNT; slot++)
    {
        SPI_EEPROM_Read(HW_HANDLER_STATISTICS_EEPROM_START_ADDR +
                        ((uint32_t)slot * HW_HANDLER_STATISTICS_RECORD_SIZE),
                        g_statisticsRecord,
                        HW_HANDLER_STATISTICS_RECORD_SIZE);
        if (StatisticsRecordIsValid(g_statisticsRecord) == 0U)
            continue;

        if (found == 0U ||
            StatisticsSequenceIsNewer(HandlerReadLe16(&g_statisticsRecord[4]), latestSequence) != 0U)
        {
            latestSlot = slot;
            latestSequence = HandlerReadLe16(&g_statisticsRecord[4]);
            StatisticsDecode(&g_statisticsRecord[6]);
            found = 1U;
        }
    }

    if (found == 0U)
    {
        memset(&usedStatistics, 0, sizeof(usedStatistics));
        g_statisticsNextSlot = 0U;
        g_statisticsNextSequence = 0U;
#if HANDLER_DEBUG_TRACE
        HandlerTraceStatistics("stat empty", 0U, 0U);
#endif
    }
    else
    {
        g_statisticsNextSlot = (uint16_t)(latestSlot + 1U);
        if (g_statisticsNextSlot >= HW_HANDLER_STATISTICS_RECORD_COUNT)
            g_statisticsNextSlot = 0U;
        g_statisticsNextSequence = (uint16_t)(latestSequence + 1U);
#if HANDLER_DEBUG_TRACE
        HandlerTraceStatistics("stat load", latestSlot, latestSequence);
#endif
    }
}

static void StatisticsSave(void)
{
    uint32_t address;
    uint16_t crc;

    address = HW_HANDLER_STATISTICS_EEPROM_START_ADDR +
              ((uint32_t)g_statisticsNextSlot * HW_HANDLER_STATISTICS_RECORD_SIZE);
    memset(g_statisticsRecord, 0xFF, sizeof(g_statisticsRecord));
    HandlerWriteLe16(&g_statisticsRecord[0], HANDLER_STATISTICS_RECORD_MAGIC);
    g_statisticsRecord[2] = HANDLER_STATISTICS_RECORD_VERSION;
    g_statisticsRecord[3] = HANDLER_STATISTICS_PAYLOAD_SIZE;
    HandlerWriteLe16(&g_statisticsRecord[4], g_statisticsNextSequence);
    StatisticsEncode(&g_statisticsRecord[6]);
    crc = HandlerCrc16(g_statisticsRecord, HANDLER_STATISTICS_CRC_OFFSET);
    HandlerWriteLe16(&g_statisticsRecord[HANDLER_STATISTICS_CRC_OFFSET], crc);

    /* Commit the magic last so an interrupted write leaves the previous slot valid. */
    SPI_EEPROM_WriteByte(address, 0xFFU);
    SPI_EEPROM_WriteByte(address + 1U, 0xFFU);
    SPI_EEPROM_Write(address + 2U,
                     &g_statisticsRecord[2],
                     HW_HANDLER_STATISTICS_RECORD_SIZE - 2U);
    SPI_EEPROM_Write(address, g_statisticsRecord, 2U);
#if HANDLER_DEBUG_TRACE
    HandlerTraceStatistics("stat save", g_statisticsNextSlot, g_statisticsNextSequence);
#endif

    g_statisticsNextSlot++;
    if (g_statisticsNextSlot >= HW_HANDLER_STATISTICS_RECORD_COUNT)
        g_statisticsNextSlot = 0U;
    g_statisticsNextSequence++;
}
void StatisticsReadParam(uint8_t* pBuff)
{
    if (pBuff == NULL)
        return;

    StatisticsLoadLatest();
    memcpy(pBuff, (uint8_t*)&usedStatistics, sizeof(statisticsType));
}

/**
 * @brief  将缓冲区的内容写入统计计数器
 * @param  pBuff [输入] 大小 = sizeof(statisticsType)
 */
void StatisticsUpdateParam(uint8_t* pBuff)
{
    if (pBuff == NULL)
        return;

    memcpy((uint8_t*)&usedStatistics, pBuff, sizeof(statisticsType));
    StatisticsSave();
}

/**
 * @brief  重置统计计数器 (全部清零)
 * @return 0 = 成功
 */
uint8_t StatisticResetParam(void)
{
    usedStatistics.logicTotal = 0U;
    usedStatistics.logicPassed = 0U;
    usedStatistics.logicFaild = 0U;
    StatisticsSave();
#if HANDLER_DEBUG_TRACE
    uart1_WriteString("[HND] stat reset\r\n");
#endif
    return 0U;
}

/* ── 初始化 ────────────────────────────────────────────────────── */

/**
 * @brief  Handler 模块初始化: 读取存储的配置 + 设置默认信号电平
 */
void Handler_Task_Init(void)
{
    /* 1. 初始化引脚方向 (已在 main.c 中通过 DutBus_Init 完成,
     *    但此处确保 handler 专用引脚已配置) */
    PORT_RCC_CLK(HW_HANDLER_OK);
    PORT_RCC_CLK(HW_HANDLER_NG);
    PORT_RCC_CLK(HW_HANDLER_BUSY);
    PORT_RCC_CLK(HW_HANDLER_UD);
    PORT_RCC_CLK(HW_HANDLER_START);
    PORT_SET_DIR_IN_PD(HW_HANDLER_START);   /* SOT input, idle low (sotLevel=1 high-active) */

    /* 2. 从非易失存储读取配置 (当前为占位, 使用默认值) */
    HandlerReadConfig(NULL);

    /* 3. 将控制引脚设为输出 */
    HANDLER_OK_INIT;
    HANDLER_NG_INIT;
    HANDLER_BUSY_INIT;
    HANDLER_EOT_INIT;
    HANDLER_UD_INIT;
    
    /* 4. 设置所有输出信号到默认状态 */
    SET_BIN_TO_DEFAULT;

    /* 5. 根据 SOT 电平设置上/下拉 */
    if (usedHandler.sotLevel == 0)
    {
        HANDLER_SOT_SET_UP_LOAD;   /* SOT 低有效 → 上拉, 空闲为高 */
    }
    else
    {
        HANDLER_SOT_SET_DW_LOAD;   /* SOT 高有效 → 下拉, 空闲为低 */
    }

    StatisticsLoadLatest();
}

/* ── 设置 BIN (用于外部快速设置) ──────────────────────────────── */

/**
 * @brief  设置 BIN 信号, 等效于执行状态3 (输出 PASS/FAIL)
 * @param  bin  0 = PASS; 非0 = FAIL
 */
void HandlerSetBin(uint8_t bin)
{
    HandlerTask(3, bin);
}

/* ── 机械手时序状态机 ─────────────────────────────────────────── */

/**
 * @brief  Handler 时序状态机 (主循环每 1ms 调用一次)
 *
 * 状态码:
 *   0 = INIT: 设置默认信号, 准备等待 SOT
 *   1 = WAIT_SOT: 等待 SOT 信号变有效 → 芯片就位
 *   2 = WAIT_SOT_END: 等待 SOT 信号变无效 → 芯片脱离
 *   3 = SEND_BIN: 根据 bin 参数输出 PASS 或 FAIL, 启动 EOT 延时
 *
 * @param  stateIndex  0xFF = 自由运行 (状态机自动推进);
 *                     其他值 = 强制跳转到指定状态
 * @param  bin         BIN 值 (0 = PASS, 非0 = FAIL), 仅在状态3使用
 * @return 1 = 检测到 SOT (SOT 边沿已触发), 0 = 等待/空闲
 */
uint16_t HandlerTask(uint8_t stateIndex, uint8_t bin)
{
    uint16_t nowSot;
    static uint16_t oldSot = 0;
    uint16_t _return = 0;

    static uint8_t  handler_task_Step_index = 0;     /* 当前状态索引 */

    /* 强制设置新状态 (外部调用) */
    if (stateIndex != 0xFF)
    {
        handler_task_Step_index = stateIndex;
    }

    switch (handler_task_Step_index)
    {
    case 0:     /* INIT: default signals, latch current SOT level */
        SET_BIN_TO_DEFAULT;
        HANDLER_EOT_CLR;
        oldSot = HANDLER_SOT_GET;
        handler_task_Step_index = 1;
        break;

    case 1:     /* WAIT_SOT: trigger offline test on SOT active edge */
        nowSot = HANDLER_SOT_GET;
        if (nowSot != oldSot)               /* level change: detect edge */
        {
            oldSot = nowSot;
            if ((nowSot && usedHandler.sotLevel == 1) ||
                (nowSot == 0 && usedHandler.sotLevel == 0))
            {
                /* Trigger: BUSY=1, return 1 to start the offline test */
                HANDLER_BUSY_SET;
                HANDLER_EOT_CLR;
                HANDLER_OK_CLR;
                HANDLER_NG_CLR;
                _return = 1;
                handler_task_Step_index = 2;
            }
        }
        break;

    case 2:     /* TEST_RUN: offline test in progress (BUSY stays 1),
                 * result arrives via HandlerSetBin() -> forced state 3 */
        break;

    case 3:     /* SEND_BIN: output PASS(OK)/FAIL(NG), hold, then BUSY clear */
        if (bin == 0)
        {
            HANDLER_OK_SET;
            HANDLER_NG_CLR;
            usedStatistics.realPassed++;
            usedStatistics.logicPassed++;
        }
        else
        {
            HANDLER_OK_CLR;
            HANDLER_NG_SET;
            usedStatistics.realFaild++;
            usedStatistics.logicFaild++;
        }

        usedStatistics.realTotal++;
        usedStatistics.logicTotal++;
        if ((usedStatistics.logicTotal % HW_HANDLER_STATISTICS_SAVE_INTERVAL) == 0U)
        {
            StatisticsSave();
        }

        /* Hold BIN for the configured delay, then clear BUSY and set EOT */
        delay_ms(usedHandler.delayMsBinToEot);

        HANDLER_BUSY_CLR;
        HANDLER_EOT_SET;

        handler_task_Step_index = 1;
        break;

    default:    /* unknown state: back to INIT */
        handler_task_Step_index = 0;
        break;
    }

    return _return;
}

