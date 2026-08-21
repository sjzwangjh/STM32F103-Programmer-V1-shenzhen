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
#include <string.h>

/* ── 模块内全局变量 ───────────────────────────────────────────── */
static HandlerConfigerType usedHandler = {1, 1, 1, 1, 1, 10, 3000};
statisticsType usedStatistics = {0, 0, 0, 0, 0, 0};

static uint8_t  handlerCfgBuff[16];

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
void HandlerUpdateFlash(void)
{
    /* 将 Handler 配置移入缓冲区 (10字节) */
    handlerCfgBuff[0] = usedHandler.sotLevel;
    handlerCfgBuff[1] = usedHandler.eotLevel;
    handlerCfgBuff[2] = usedHandler.busyLevel;
    handlerCfgBuff[3] = usedHandler.passLevel;
    handlerCfgBuff[4] = usedHandler.ngLevel;
    handlerCfgBuff[5] = (uint8_t)usedHandler.delayMsBinToEot;
    handlerCfgBuff[6] = (uint8_t)(usedHandler.delayMsBinToEot >> 8);
    handlerCfgBuff[7] = (uint8_t)usedHandler.delayMsMinTestTime;
    handlerCfgBuff[8] = (uint8_t)(usedHandler.delayMsMinTestTime >> 8);

    /* 写入 SPI EEPROM */
     SPI_EEPROM_Write(HW_HANDLER_PARAM_EEPROM_START_ADDR, handlerCfgBuff, 10);
}

/**
 * @brief  从非易失存储读取 Handler 配置
 * @param  pByteBuff [输出/可选] 若 !=NULL, 将读取到的 10 字节拷贝到此缓冲区
 */
void HandlerReadConfig(uint8_t* pByteBuff)
{

    /* 
     * TODO: 集成 SPI_Flash_Read
     *   SPI_Flash_Mount(0);
     *   result = SPI_Flash_Read(handlerCfgFileName, handlerCfgBuff, 10);
     *   if (result == 10) { ... }
     *   SPI_Flash_DisMount();
     *
     * 当前先用默认值代替
     */
    /* Read the 10-byte handler config from SPI EEPROM; fall back to defaults
     * when the area is all 0x00 or all 0xFF (erased / never written). */
    SPI_EEPROM_Read(HW_HANDLER_PARAM_EEPROM_START_ADDR, handlerCfgBuff, 10);
    if (handlerCfgBuff[0] != 0 && handlerCfgBuff[1] != 0 && handlerCfgBuff[2] != 0 &&
        handlerCfgBuff[3] != 0 && handlerCfgBuff[4] != 0 && handlerCfgBuff[5] != 0 &&
        handlerCfgBuff[6] != 0 && handlerCfgBuff[7] != 0 && handlerCfgBuff[8] != 0 &&
        handlerCfgBuff[0] != 0xFF && handlerCfgBuff[1] != 0xFF && handlerCfgBuff[2] != 0xFF &&
        handlerCfgBuff[3] != 0xFF && handlerCfgBuff[4] != 0xFF && handlerCfgBuff[5] != 0xFF &&
        handlerCfgBuff[6] != 0xFF && handlerCfgBuff[7] != 0xFF && handlerCfgBuff[8] != 0xFF)
    {
        usedHandler.sotLevel         = handlerCfgBuff[0];
        usedHandler.eotLevel         = handlerCfgBuff[1];
        usedHandler.busyLevel        = handlerCfgBuff[2];
        usedHandler.passLevel        = handlerCfgBuff[3];
        usedHandler.ngLevel          = handlerCfgBuff[4];
        usedHandler.delayMsBinToEot  = (handlerCfgBuff[5]) | (handlerCfgBuff[6] << 8);
        usedHandler.delayMsMinTestTime = (handlerCfgBuff[7]) | (handlerCfgBuff[8] << 8);
    }

    if (pByteBuff != NULL)
        memcpy(pByteBuff, handlerCfgBuff, 10);
}

/* ── 统计函数 ──────────────────────────────────────────────────── */

/**
 * @brief  将当前统计计数器读出到缓冲区
 * @param  pBuff [输出] 大小 = sizeof(statisticsType)
 */
void StatisticsReadParam(uint8_t* pBuff)
{
    memcpy(pBuff, (uint8_t*)&usedStatistics, sizeof(statisticsType));
}

/**
 * @brief  将缓冲区的内容写入统计计数器
 * @param  pBuff [输入] 大小 = sizeof(statisticsType)
 */
void StatisticsUpdataParam(uint8_t* pBuff)
{
    memcpy((uint8_t*)&usedStatistics, pBuff, sizeof(statisticsType));
}

/**
 * @brief  重置统计计数器 (全部清零)
 * @return 0 = 成功
 */
uint8_t StatisticResetParam(void)
{
    usedStatistics.logicTotal   = 0;
    usedStatistics.logicPassed  = 0;
    usedStatistics.logicFaild   = 0;

    /* TODO: SPI_FlashSaveStatistics(); 保存到非易失存储 */
    return 0;
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

    /* TODO: SPI_FlashLoadStatistics(); 加载历史统计数据 */
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

        /* Hold BIN for the configured delay, then clear BUSY and set EOT */
        delay_ms(usedHandler.delayMsBinToEot);

        HANDLER_BUSY_CLR;
        HANDLER_EOT_SET;

        /* Auto-save statistics every 5 parts (reserved) */
        if ((usedStatistics.logicTotal % 5) == 0)
        {
            /* TODO: statistics persistence */
        }

        handler_task_Step_index = 1;
        break;

    default:    /* unknown state: back to INIT */
        handler_task_Step_index = 0;
        break;
    }

    return _return;
}

