/*
 * ICSP 编程驱动实现 - PIC10/12/16 系列串行编程
 *
 * 层次结构:
 *   A. 电源与引脚控制层        —— 在 icsp.h 中以宏方式保留
 *   B. ICSP 位时序收发层       —— 6-bit 命令 / 16-cycle 数据帧
 *   C. 编程模式控制层          —— HVP/LVP 进入与退出
 *   D. 器件原语与地址层        —— Config/UserID/DeviceID 等寻址与读写
 *   E. 校验与安全层            —— 校验/一致性检查
 *   F. Family 分发入口层       —— 对上提供统一 PIC8 入口
 *
 * 电源/引脚操作已全部通过 icsp.h 宏内联，消除函数调用开销。
 *
 * ---- 地址优化说明 ----
 * ICSP 的 PC (Program Counter) 只能通过 Increment Address 递增,
 * 不能递减也无法直接跳转。本模块通过跟踪当前 PC 位置
 * (g_picCurrentArea / g_picCurrentAddress), 实现"智能寻址":
 *   - 目标地址 >= 当前地址: 仅用 Increment 到达, 避免复位重来
 *   - 目标地址 < 当前地址: 走完整路径 (复位+递增)
 * 此优化大幅减少连续 Flash/EEPROM/Config 访问时的冗余地址操作。
 */

#include "sys.h"
#include "Hardware_Config.h"
#include "delay.h"
#include "timer.h"
#include "usart.h"
#include "icsp.h"
#include <string.h>


/* ================================================================= */
/* 静态变量                                                             */
/* ================================================================= */
static uint8_t      g_picCmdWidth;                /* 命令位宽: 6 */
static uint8_t      g_picDataWidth;               /* 数据位宽: 12/14/16 */
static const pic_prog_params_t  *icsp_pdev = NULL; /* 当前器件 */
static uint8_t      g_picEnterPreferLvp;           /* 最近一次进入编程模式时是否偏好LVP */
static uint8_t      g_picProgmodeActive;           /* 当前是否已进入编程模式 */
static pic_saved_param_t g_picSavedParam;          /* 擦除前保存的关键参数 */
static uint8_t      g_picSavedParamValid;          /* 关键参数是否已完成保存 */

/*
 * ---- 新增: ICSP 当前 PC 位置跟踪 ----
 *
 * 作用: 跟踪当前 PC 处于哪个区域以及具体地址值,
 *       使后续寻址能基于当前位置做"增量式"导航,
 *       避免每次都从复位位置重新递增。
 *
 * ICSP_AREA_NONE   = 未知 / 未进入编程模式
 * ICSP_AREA_PROGRAM = 程序存储器区
 * ICSP_AREA_CONFIG = 配置空间区 (含 Config, UserID, DeviceID, OSCCAL, EEPROM)
 *
 * EEPROM 虽然使用 CMD_LOAD_DATA / CMD_READ_DATA 访问,
 * 但其地址定位通过 CMD_LOAD_CFG + Increment 实现, 与配置空间共用 PC 导航,
 * 因此统一归入 ICSP_AREA_CONFIG。
 */
#define ICSP_AREA_NONE      0
#define ICSP_AREA_PROGRAM   1
#define ICSP_AREA_CONFIG    2

static uint8_t      g_picCurrentArea;              /* 当前PC所在区域 */
static uint32_t     g_picCurrentAddress;           /* 当前PC地址值 */


/* ================================================================= */
/* B层: ICSP 时序位操作层                                               */
/* ================================================================= */

/*
 * ICSP 命令定义 (6-bit, LSB first)
 *
 * 核实依据:
 * 1. PIC10F200/202/204/206 Programming Specification, DS41228F
 * 2. PIC12F629/675 & PIC16F630/676 Programming Specification, DS41191D
 * 3. PIC16F627A/628A/648A Programming Specification, DS41196B
 * 4. PIC16F1825/1829 Programming Specification, DS41390D
 *
 * 说明:
 * - baseline 12-bit 核命令集最小, 主要是 Program/Read/Increment/Begin/End/Bulk Erase
 * - 标准 14-bit mid-range 增加了 Load Configuration / Data EEPROM / End Programming
 * - enhanced mid-range 进一步增加 Reset Address 与 Row Erase Program Memory
 */

/* ===== 标准 14-bit / enhanced mid-range 共用命令 ===== */
#define CMD_LOAD_CFG            0x00    /* 000000: Load Configuration */
#define CMD_LOAD_PROG           0x02    /* 000010: Load Data for Program Memory */
#define CMD_LOAD_DATA           0x03    /* 000011: Load Data for Data Memory */
#define CMD_READ_PROG           0x04    /* 000100: Read Data from Program Memory */
#define CMD_READ_DATA           0x05    /* 000101: Read Data from Data Memory */
#define CMD_INC_ADDR            0x06    /* 000110: Increment Address */
#define CMD_BEGIN_PROG          0x08    /* 001000: Begin Internally Timed Programming */
#define CMD_ERASE_PROG          0x09    /* 001001: Bulk Erase Program Memory */
#define CMD_END_PROG            0x0A    /* 001010: End Externally Timed Programming */
#define CMD_ERASE_DATA          0x0B    /* 001011: Bulk Erase Data Memory */
#define CMD_BEGIN_PROG_EXT      0x18    /* 011000: Begin Externally Timed Programming */

/* ===== enhanced mid-range 扩展命令 ===== */
#define CMD_ROW_ERASE_PROG      0x11    /* 010001: Row Erase Program Memory */
#define CMD_RESET_ADDR          0x16    /* 010110: Reset Address */

/* ===== baseline 12-bit 核命令 ===== */
#define CMD12_LOAD_PROG         0x02    /* 000010: Load Data for Program Memory */
#define CMD12_READ_PROG         0x04    /* 000100: Read Data from Program Memory */
#define CMD12_INC_ADDR          0x06    /* 000110: Increment Address */
#define CMD12_BEGIN_PROG        0x08    /* 001000: Begin Programming (externally timed) */
#define CMD12_BULK_ERASE        0x09    /* 001001: Bulk Erase Program Memory */
#define CMD12_END_PROG          0x0E    /* 001110: End Programming */

/* 热点路径快速宏:
 * 当 ICSP_CLK_DELAY 被定义为空时，尽量避免热点路径中的额外函数调用。
 */
#define ICSP_IS_BASELINE_FAST() \
    (icsp_pdev != NULL && icsp_pdev->common.core_family == PIC8_CORE_BASELINE_12BIT)

#define ICSP_SUPPORTS_CONFIG_SPACE_FAST() \
    (icsp_pdev != NULL && icsp_pdev->common.has_load_config_cmd != 0U && \
     icsp_pdev->common.config_space_base != 0U)

#define ICSP_SUPPORTS_LVP_FAST() \
    (icsp_pdev != NULL && icsp_pdev->common.lvp_mode != PIC8_LVP_NONE)

#define ICSP_CMD_GAP_FAST()      //ICSP_DELAY_US(1)
#define ICSP_LOAD_PROG_CMD_FAST() (ICSP_IS_BASELINE_FAST() ? CMD12_LOAD_PROG : CMD_LOAD_PROG)
#define ICSP_READ_PROG_CMD_FAST() (ICSP_IS_BASELINE_FAST() ? CMD12_READ_PROG : CMD_READ_PROG)
#define ICSP_INC_ADDR_CMD_FAST()  (ICSP_IS_BASELINE_FAST() ? CMD12_INC_ADDR : CMD_INC_ADDR)
#define ICSP_END_PROG_CMD_FAST()  (ICSP_IS_BASELINE_FAST() ? CMD12_END_PROG : CMD_END_PROG)
#define ICSP_USE_EXT_PROG_FAST()  (ICSP_IS_BASELINE_FAST())

/*
 * 增强型宏: 递增 ICSP 地址, 同时更新跟踪变量 g_picCurrentAddress
 *
 * 设计要点:
 * - 每执行一次 ICSP Increment 硬件命令, PC 增加 1
 * - 同步递增 g_picCurrentAddress 确保跟踪变量与实际 PC 一致
 * - 此处不检查 g_picCurrentArea, 由调用方保证在正确的区域内调用
 */
#define ICSP_INCREMENT_ADDRESS_FAST() \
    do{ \
        icspLoadCmd(ICSP_INC_ADDR_CMD_FAST()); \
        ICSP_CMD_GAP_FAST(); \
        g_picCurrentAddress++; \
    }while(0)

#define ICSP_BEGIN_PROGRAM_FAST(waitUs) \
    do{ \
        icspLoadCmd(ICSP_USE_EXT_PROG_FAST() ? CMD12_BEGIN_PROG : CMD_BEGIN_PROG); \
        ICSP_DELAY_US(waitUs); \
        if(ICSP_USE_EXT_PROG_FAST()){ \
            icspLoadCmd(ICSP_END_PROG_CMD_FAST()); \
            ICSP_CMD_GAP_FAST(); \
        } \
    }while(0)

/* Forward declarations used by helper routines below. */
static uint8_t icspWriteConfigWordAt(uint32_t targetAddr, uint16_t value, uint16_t waitUs);
static uint8_t icspReadConfigWordAt(uint32_t targetAddr, uint16_t *value);
static uint32_t icspGetBaselineConfigPhys(void);
static uint32_t icspGetBaselinePcSpace(void);
static uint8_t  icspRestartAndSyncCodeBase(void);
static uint8_t  icspEnsureBaselineAtConfig(void);
/*
 * Clock-calibrated microsecond busy-wait (SYSCLK fixed at 72MHz).
 * Each loop iteration costs ~4 CPU cycles, so 18 iterations give ~1us.
 * All device wait values (wait_pgm_us / wait_erase_us / wait_cfg_us ...)
 * are passed through ICSP_DELAY_US() so the struct values control the
 * real programming delay time in microseconds.
 */
void icspDelayUs(uint32_t us)
{
    volatile uint32_t n = us * 18UL;
    while (n-- != 0UL)
    {
    }
}

/* ---- 运行时位时钟速度控制 (说明见 icsp.h 的 ICSP_CLK_DELAY) ---- */
#if !ICSP_CLK_FAST
uint16_t g_icspPhasePad = 0U;

/*
 * 设定 ICSP 位时钟 (读/写时序同步生效)
 * 每 bit 周期 ≈ 16 + 8*pad 个 CPU 周期 (72MHz 下约 222ns + 111ns*pad),
 * 常数为反汇编标定估值, 如需精确可用示波器实测后微调
 * @param  hz  目标位时钟 (Hz), 过快自动落到最快档, 过慢钳位到最慢档 (~35kHz)
 * @return 实际达到的近似频率 (Hz)
 */
uint32_t icspSetIcspClock(uint32_t hz)
{
    uint32_t per;                       /* 目标 bit 周期 (CPU 周期数) */
    uint32_t pad;

    if (hz == 0UL)
        hz = 1UL;
    per = 72000000UL / hz;
    pad = (per > 16UL) ? ((per - 12UL) >> 3) : 0UL;   /* +4 舍入 */
    if (pad > 255UL)
        pad = 255UL;
    g_icspPhasePad = (uint16_t)pad;
    return 72000000UL / (16UL + (pad << 3));
}
#endif


/* ================================================================= */
/* 内部辅助函数                                                         */
/* ================================================================= */

/**
 * @brief  根据位宽获取对应的位掩码
 * @param  width 位宽 (0=视为16位)
 * @return 全1位掩码, 如 width=8 返回 0x00FF
 */
static uint16_t icspGetBitMask(uint8_t width)
{
    if (width >= 16U)
        return 0xFFFFU;
    if (width == 0U)
        return 0xFFFFU;
    return (uint16_t)((1UL << width) - 1UL);
}

/**
 * @brief  获取擦除前保存的配置字存储槽位指针
 * @param  idx 配置字索引 (0~3, 对应 config_word ~ config4_word)
 * @return 指向对应配置字的指针, 索引无效返回 NULL
 */
static uint16_t *icspGetSavedConfigSlot(uint8_t idx)
{
    switch (idx)
    {
    case 0: return &g_picSavedParam.config_word;
    case 1: return &g_picSavedParam.config2_word;
    case 2: return &g_picSavedParam.config3_word;
    case 3: return &g_picSavedParam.config4_word;
    default: return NULL;
    }
}

/**
 * @brief  获取擦除前保存的校准字存储槽位指针
 * @param  idx 校准字索引 (0=osccal, 1=cal_word1, 2=cal_word2)
 * @return 指向对应校准字的指针, 索引无效返回 NULL
 */
static uint16_t *icspGetSavedCalSlot(uint8_t idx)
{
    switch (idx)
    {
    case 0: return &g_picSavedParam.osccal_word;
    case 1: return &g_picSavedParam.cal_word1;
    case 2: return &g_picSavedParam.cal_word2;
    default: return NULL;
    }
}

/**
 * @brief  获取有效的配置字个数
 *         优先使用 common.config_word_count, 若为0则回退到子结构体判断
 * @return 配置字个数, 失败返回0
 */
static uint8_t icspGetConfigWordCountEffective(void)
{
    if (icsp_pdev == NULL)
        return 0U;
    if (icsp_pdev->common.config_word_count != 0U)
        return icsp_pdev->common.config_word_count;
    /* baseline 器件: 若 config_shadow_addr 或 config_addr 非零, 则至少有1个配置字 */
    if (ICSP_IS_BASELINE_FAST() &&
        (icsp_pdev->baseLine.config_shadow_addr != 0U || icsp_pdev->common.config_addr != 0U))
        return 1U;
    return 0U;
}

/**
 * @brief  获取有效的 OSCCAL 校准字个数
 *         优先使用 common.osccal_word_count, 若为0则根据 baseline 子结构体判断
 * @return 校准字个数, 失败返回0
 */
static uint8_t icspGetOsccalWordCountEffective(void)
{
    if (icsp_pdev == NULL)
        return 0U;
    if (icsp_pdev->common.osccal_word_count != 0U)
        return icsp_pdev->common.osccal_word_count;
    /* baseline 器件: 若 osccal_addr 或 osccal_base 非零, 则至少有1个校准字 */
    if (ICSP_IS_BASELINE_FAST() &&
        (icsp_pdev->baseLine.osccal_addr != 0U || icsp_pdev->common.osccal_base != 0U))
        return 1U;
    return 0U;
}

/**
 * @brief  根据索引获取配置字的绝对地址
 *         优先使用 DCRDef 中的 dcr_addr, 否则按架构规则计算
 * @param  idx  配置字索引 (0开始)
 * @param  addr 输出: 配置字在编程空间中的地址
 * @return ICSP_OK=成功, ICSP_ERR=索引越界或参数无效
 */
static uint8_t icspGetConfigAddressByIndex(uint8_t idx, uint32_t *addr)
{
    uint8_t count;

    if (icsp_pdev == NULL || addr == NULL)
        return ICSP_ERR;
    count = icspGetConfigWordCountEffective();
    if (idx >= count)
        return ICSP_ERR;

    /* 优先使用 DCRDef 中记录的物理地址 */
    if (!ICSP_IS_BASELINE_FAST() &&
        icsp_pdev->common.config_dcr[idx].dcr_addr != 0U)
    {
        *addr = icsp_pdev->common.config_dcr[idx].dcr_addr;
        return ICSP_OK;
    }

    /* baseline 器件: 使用 config_shadow_addr 或按偏移计算 */
    if (ICSP_IS_BASELINE_FAST())
    {
        *addr = icspGetBaselineConfigPhys();
        return ICSP_OK;
    }

    /* 14-bit / Enhanced: config_addr + 索引偏移 */
    *addr = icsp_pdev->common.config_addr + idx;
    return ICSP_OK;
}

/**
 * @brief  根据索引获取 OSCCAL 校准字的绝对地址
 *         通过 osccal_addr (baseline) 或 osccal_base + idx (14-bit) 查找
 * @param  idx  校准字索引 (0开始)
 * @param  addr 输出: 校准字在编程空间中的地址
 * @return ICSP_OK=成功, ICSP_ERR=索引越界或参数无效
 */
static uint8_t icspGetOsccalAddressByIndex(uint8_t idx, uint32_t *addr)
{
    uint8_t count;

    if (icsp_pdev == NULL || addr == NULL)
        return ICSP_ERR;
    count = icspGetOsccalWordCountEffective();
    if (idx >= count)
        return ICSP_ERR;

    /* baseline 器件: 使用 osccal_addr 独立地址或 osccal_base */
    if (ICSP_IS_BASELINE_FAST())
    {
        *addr = (icsp_pdev->baseLine.osccal_addr != 0U) ?
                icsp_pdev->baseLine.osccal_addr :
                icsp_pdev->common.osccal_base;
        return (*addr != 0U) ? ICSP_OK : ICSP_ERR;
    }

    /* 14-bit / Enhanced: osccal_base + 偏移 */
    if (icsp_pdev->common.osccal_base != 0U &&
        idx < icsp_pdev->common.osccal_word_count)
    {
        *addr = icsp_pdev->common.osccal_base + idx;
        return ICSP_OK;
    }

    return ICSP_ERR;
}

/**
 * @brief  根据索引获取额外校准数据的绝对地址
 *         优先使用 cal_data_base, 否则回退到 midRange 子结构体中的 cal_word 地址
 * @param  idx  额外校准数据索引 (0开始)
 * @param  addr 输出: 校准数据在编程空间中的地址
 * @return ICSP_OK=成功, ICSP_ERR=索引越界或参数无效
 */
static uint8_t icspGetExtraCalAddressByIndex(uint8_t idx, uint32_t *addr)
{
    if (icsp_pdev == NULL || addr == NULL)
        return ICSP_ERR;

    /* 优先使用通用 cal_data_base */
    if (icsp_pdev->common.cal_data_base != 0U &&
        idx < icsp_pdev->common.cal_data_word_count)
    {
        *addr = icsp_pdev->common.cal_data_base + idx;
        return ICSP_OK;
    }

    /* mid-range 器件的专用校准字地址 */
    if (icsp_pdev->common.core_family == PIC8_CORE_MIDRANGE_14BIT)
    {
        if (idx == 0U && icsp_pdev->midRange.cal_word1_addr != 0U)
        {
            *addr = icsp_pdev->midRange.cal_word1_addr;
            return ICSP_OK;
        }
        if (idx == 1U && icsp_pdev->midRange.cal_word2_addr != 0U)
        {
            *addr = icsp_pdev->midRange.cal_word2_addr;
            return ICSP_OK;
        }
    }

    return ICSP_ERR;
}

/**
 * @brief  在程序区指定地址读取一个程序字
 *         先通过 icspSetProgramAddress 定位 PC, 再读取
 * @param  targetAddr 目标程序字地址
 * @param  value      输出: 读取到的数据
 * @return ICSP_OK=成功, ICSP_ERR=失败
 */
static uint8_t icspReadProgramWordAt(uint32_t targetAddr, uint16_t *value)
{
    if (value == NULL)
        return ICSP_ERR;
    if (icspSetProgramAddress(targetAddr) != ICSP_OK)
        return ICSP_ERR;
    *value = icspReadWord();
    return ICSP_OK;
}

/**
 * @brief  在程序区指定地址写入一个程序字
 *         baseline 器件直接从程序区访问, 无需 LoadConfig
 * @param  targetAddr 目标程序字地址
 * @param  value      要写入的数据
 * @param  waitUs     编程等待时间 (us)
 * @return ICSP_OK=成功, ICSP_ERR=失败
 */
static uint8_t icspWriteProgramWordAt(uint32_t targetAddr, uint16_t value, uint16_t waitUs)
{
    if (icspSetProgramAddress(targetAddr) != ICSP_OK)
        return ICSP_ERR;

    icspLoadCmd(ICSP_LOAD_PROG_CMD_FAST());
    ICSP_CMD_GAP_FAST();
    icspLoadData(value, g_picDataWidth);
    ICSP_BEGIN_PROGRAM_FAST(waitUs);
    return ICSP_OK;
}

/**
 * @brief  根据架构类型, 用绝对地址读取一个字
 *         baseline 走程序区路径, 14-bit/Enhanced 走配置空间路径
 * @param  targetAddr 目标绝对地址
 * @param  value      输出: 读取到的数据
 * @return ICSP_OK=成功, ICSP_ERR=失败
 */
static uint8_t icspReadWordByAbsoluteAddress(uint32_t targetAddr, uint16_t *value)
{
    if (icsp_pdev == NULL || value == NULL)
        return ICSP_ERR;

    if (ICSP_IS_BASELINE_FAST())
        return icspReadProgramWordAt(targetAddr, value);

    return icspReadConfigWordAt(targetAddr, value);
}

/**
 * @brief  根据架构类型, 用绝对地址写入一个字
 *         baseline 走程序区路径, 14-bit/Enhanced 走配置空间路径
 * @param  targetAddr 目标绝对地址
 * @param  value      要写入的数据
 * @param  waitUs     编程等待时间 (us)
 * @return ICSP_OK=成功, ICSP_ERR=失败
 */
static uint8_t icspWriteWordByAbsoluteAddress(uint32_t targetAddr, uint16_t value, uint16_t waitUs)
{
    if (icsp_pdev == NULL)
        return ICSP_ERR;

    if (ICSP_IS_BASELINE_FAST())
        return icspWriteProgramWordAt(targetAddr, value, waitUs);

    return icspWriteConfigWordAt(targetAddr, value, waitUs);
}

/**
 * @brief  构建擦除后恢复配置字的值
 *         根据 DCRDef 的 impl_mask 和 unimpl_val,
 *         将保存的值与未实现位填充值合并, 生成正确的恢复值。
 *
 * 恢复逻辑: (savedValue & implMask) | unimplFill
 * 其中 unimplFill 根据 unimpl_val 决定:
 *   - unimpl_val=1 → 未实现位填1 (大多数器件)
 *   - unimpl_val=0 → 未实现位填擦除默认值
 *
 * @param  idx        配置字索引
 * @param  savedValue 擦除前保存的配置字原始值
 * @return 恢复时应写入的配置字值 (已合并未实现位)
 */

/**
 * @brief  检查配置字当前是否处于已擦除状态
 *         用读取值与 DCRDef 中的 impl_mask/default_value 比较,
 *         判断是否已被擦除 (全1) 或仍然保留原值。
 * @param  idx       配置字索引
 * @param  readValue 从器件读取到的当前配置字值
 * @return 1=已擦除, 0=未擦除 (仍有数据)
 */

/**
 * @brief  检查校准字当前是否处于已擦除状态 (全1)
 *         校准字没有 DCRDef 掩码, 直接用全1比较
 * @param  readValue 从器件读取到的当前校准字值
 * @return 1=已擦除 (全1), 0=未擦除
 */
static uint8_t icspIsCalWordErased(uint16_t readValue)
{
    uint16_t widthMask;

    if (icsp_pdev == NULL)
        return 0U;

    widthMask = icspGetBitMask(g_picDataWidth);
    return (((readValue & widthMask) == widthMask) ? 1U : 0U);
}

/**
 * @brief  清除擦除前保存的参数, 全部填充 0xFF (擦除态)
 *         并将有效标志 g_picSavedParamValid 置为 0 (无效)
 */
static void icspClearSavedParam(void)
{
    memset(&g_picSavedParam, 0xFF, sizeof(g_picSavedParam));
    g_picSavedParamValid = 0U;
}

/**
 * @brief  在整片擦除前, 备份关键参数 (配置字/OSCCAL/校准数据)
 *         依次备份: 所有配置字 → OSCCAL → 额外校准数据
 *         备份成功后设置 g_picSavedParamValid = 1
 * @return ICSP_OK=成功, ICSP_ERR=备份失败 (器件异常)
 */
/* OSCCAL erase pre-check helpers:
 * - OSCCAL inside the code area can be hidden by code protection (reads 0x0000)
 *   or missing at the MCU entry (reads all-ones); erasing would lose the factory
 *   calibration permanently, so such devices are rejected before the erase.
 * - "in code area": address before config_space_base (baseline has none).
 * - "entry word": baseline parts keep OSCCAL at the Reset Vector; 14-bit parts
 *   when OSCCAL sits at code_end-1 (e.g. 12F629/675 at 0x3FF). */
static uint8_t icspIsOsccalInCodeArea(uint32_t osccalAddr)
{
    if (icsp_pdev == NULL || osccalAddr == 0U)
        return 0U;
    if (icsp_pdev->common.config_space_base == 0U)
        return 1U;
    return (osccalAddr < icsp_pdev->common.config_space_base) ? 1U : 0U;
}

static uint8_t icspIsOsccalEntryWord(uint32_t osccalAddr)
{
    if (icsp_pdev == NULL || osccalAddr == 0U)
        return 0U;
    if (ICSP_IS_BASELINE_FAST())
        return 1U;
    return (osccalAddr == (icsp_pdev->common.code_end_addr - 1U)) ? 1U : 0U;
}

static uint8_t icspBackupCriticalWords(void)
{
    uint8_t idx;
    uint8_t count;
    uint16_t value;
    uint16_t *slot;

    if (icsp_pdev == NULL)
        return ICSP_ERR;

    icspClearSavedParam();

    /* 备份所有配置字 */
    count = icspGetConfigWordCountEffective();
    for (idx = 0U; idx < count && idx < MAX_CONFIG_WORDS; idx++)
    {
        slot = icspGetSavedConfigSlot(idx);
        if (slot == NULL)
            return ICSP_ERR;
        value = icspReadCfg(idx);
        if (value == 0xFFFFU)
            return ICSP_ERR;
        *slot = value;
        #if UART1_TRACE
        uart1_WriteString("ICSP bak cfg idx=");
        uart1_WriteDec(idx);
        uart1_WriteString(" val=0x");
        uart1_WriteHex16(*slot);
        uart1_WriteString("\r\n");
        #endif
    }

    /* 备份 OSCCAL 校准字 */
    if (icspGetOsccalWordCountEffective() != 0U)
    {
        slot = icspGetSavedCalSlot(0U);
        value = icspReadOSCCAL(0U);
        if (slot == NULL || value == 0xFFFFU)
            return ICSP_ERR;
        {
            uint32_t osccalAddr = 0U;
            if (icspGetOsccalAddressByIndex(0U, &osccalAddr) == ICSP_OK &&
                icspIsOsccalInCodeArea(osccalAddr))
            {
                if (value == 0x0000U)
                {
                    #if UART1_TRACE
                    uart1_WriteString("ICSP CAL: OSCCAL all-zero (code-protected), device invalid\r\n");
                    #endif
                    return ICSP_ERR_CAL_LOST;
                }
                if (icspIsOsccalEntryWord(osccalAddr) &&
                    value == icspGetBitMask(g_picDataWidth))
                {
                    #if UART1_TRACE
                    uart1_WriteString("ICSP CAL: OSCCAL entry all-ones (missing factory content), device invalid\r\n");
                    #endif
                    return ICSP_ERR_CAL_LOST;
                }
            }
        }
        *slot = value;
        #if UART1_TRACE
        uart1_WriteString("ICSP bak osccal val=0x");
        uart1_WriteHex16(value);
        uart1_WriteString("\r\n");
        #endif
    }

    /* 备份额外校准数据 */
    for (idx = 0U; idx < icsp_pdev->common.cal_data_word_count && idx < 2U; idx++)
    {
        uint32_t targetAddr;
        slot = icspGetSavedCalSlot((uint8_t)(idx + 1U));
        if (slot == NULL)
            return ICSP_ERR;
        if (icspGetExtraCalAddressByIndex(idx, &targetAddr) != ICSP_OK)
            break;
        if (icspReadWordByAbsoluteAddress(targetAddr, &value) != ICSP_OK)
            return ICSP_ERR;
        *slot = value;
        #if UART1_TRACE
        uart1_WriteString("ICSP bak cal idx=");
        uart1_WriteDec(idx);
        uart1_WriteString(" addr=0x");
        uart1_WriteHex16((uint16_t)targetAddr);
        uart1_WriteString(" val=0x");
        uart1_WriteHex16(value);
        uart1_WriteString("\r\n");
        #endif
    }

    g_picSavedParamValid = 1U;
    return ICSP_OK;
}

/**
 * @brief  在整片擦除后, 恢复关键参数 (配置字/OSCCAL/校准数据)
 *         对每个备份项: 先读取当前值, 若已擦除则写回备份值。
 *         写回时使用 icspBuildConfigRestoreValue 处理未实现位填充。
 * @return ICSP_OK=成功, ICSP_ERR=恢复失败
 */
static uint8_t icspRestoreCriticalWords(void)
{
    uint8_t idx;
    uint8_t count;
    uint16_t value;
    uint16_t *slot;
    uint32_t targetAddr;

    if (icsp_pdev == NULL || g_picSavedParamValid == 0U)
        return ICSP_ERR;

    /* 恢复所有配置字 */
    /* Restore only the bits OUTSIDE impl_mask after erase (calibration/factory
     * content); impl bits stay all-ones (erased) so a code-protected
     * (CP/CPD = 0) config is NOT re-enabled and program reads stay open.
     * The host programs config words last anyway. */
    count = icspGetConfigWordCountEffective();
    for (idx = 0U; idx < count && idx < MAX_CONFIG_WORDS; idx++)
    {
        uint16_t implMask;
        uint16_t restore;

        slot = icspGetSavedConfigSlot(idx);
        if (slot == NULL)
            return ICSP_ERR;
        if (icspGetConfigAddressByIndex(idx, &targetAddr) != ICSP_OK)
            return ICSP_ERR;
        if (icspReadWordByAbsoluteAddress(targetAddr, &value) != ICSP_OK)
            return ICSP_ERR;

        /* Restore only the bits outside impl_mask (calibration/factory content);
         * impl bits stay all-ones (erased) so code protection is not re-enabled;
         * the host programs config words last anyway. */
        implMask = icsp_pdev->common.config_dcr[idx].impl_mask;
        restore = (uint16_t)((*slot & (uint16_t)~implMask) | implMask);
        if (implMask != 0U && restore != value)
        {
            if (ICSP_IS_BASELINE_FAST())
            {
                if (icspEnsureBaselineAtConfig() != ICSP_OK)
                    return ICSP_ERR;
                if (icspWriteProgramWordAt(targetAddr, restore,
                                          icsp_pdev->common.wait_cfg_us) != ICSP_OK)
                    return ICSP_ERR;
            }
            else
            {
                if (icspWriteConfigWordAt(targetAddr, restore,
                                         icsp_pdev->common.wait_cfg_us) != ICSP_OK)
                    return ICSP_ERR;
            }
        }
        #if UART1_TRACE
        uart1_WriteString("ICSP erase cfg back=0x");
        uart1_WriteHex16(*slot);
        uart1_WriteString(" rest=0x");
        uart1_WriteHex16(restore);
        uart1_WriteString(" post=0x");
        uart1_WriteHex16(value);
        uart1_WriteString("\r\n");
        #endif
    }

    /* 恢复 OSCCAL 校准字 */
    if (icspGetOsccalWordCountEffective() != 0U)
    {
        slot = icspGetSavedCalSlot(0U);
        if (slot == NULL)
            return ICSP_ERR;
        if (icspGetOsccalAddressByIndex(0U, &targetAddr) == ICSP_OK)
        {
            if (icspReadWordByAbsoluteAddress(targetAddr, &value) != ICSP_OK)
                return ICSP_ERR;
            if (icspIsCalWordErased(value))
            {
                #if UART1_TRACE
                uart1_WriteString("ICSP rst osccal post=0x");
                uart1_WriteHex16(value);
                uart1_WriteString(" w=0x");
                uart1_WriteHex16(*slot);
                uart1_WriteString("\r\n");
                #endif
                if (icspWriteWordByAbsoluteAddress(targetAddr, *slot, icsp_pdev->common.wait_cfg_us) != ICSP_OK)
                    return ICSP_ERR;
            }
        }
    }

    /* 恢复额外校准数据 */
    for (idx = 0U; idx < icsp_pdev->common.cal_data_word_count && idx < 2U; idx++)
    {
        slot = icspGetSavedCalSlot((uint8_t)(idx + 1U));
        if (slot == NULL)
            return ICSP_ERR;
        if (icspGetExtraCalAddressByIndex(idx, &targetAddr) != ICSP_OK)
            break;
        if (icspReadWordByAbsoluteAddress(targetAddr, &value) != ICSP_OK)
            return ICSP_ERR;
        if (icspIsCalWordErased(value))
        {
            #if UART1_TRACE
            uart1_WriteString("ICSP rst cal idx=");
            uart1_WriteDec(idx);
            uart1_WriteString(" addr=0x");
            uart1_WriteHex16((uint16_t)targetAddr);
            uart1_WriteString(" post=0x");
            uart1_WriteHex16(value);
            uart1_WriteString(" w=0x");
            uart1_WriteHex16(*slot);
            uart1_WriteString("\r\n");
            #endif
            if (icspWriteWordByAbsoluteAddress(targetAddr, *slot, icsp_pdev->common.wait_cfg_us) != ICSP_OK)
                return ICSP_ERR;
        }
    }

    return ICSP_OK;
}

/**
 * @brief  获取延时值，如果参数为0则使用默认值
 * @param  value       参数延时值
 * @param  defaultValue 默认延时值
 * @return 有效延时值
 */
static uint16_t icspGetDelayOrDefault(uint16_t value, uint16_t defaultValue)
{
    return (value != 0U) ? value : defaultValue;
}

/**
 * @brief  Enhanced Mid-Range 使用 Reset Address 将PC复位到0x0000
 *
 * 注意: 此操作后 PC 被复位到 0x0000 (程序区起始),
 *       因此需要同步更新跟踪变量。
 */
static void icspResetAddressRaw(void)
{
    if (icsp_pdev == NULL)
        return;

    if (icsp_pdev->common.core_family == PIC8_CORE_ENHANCED_14BIT)
    {
        icspLoadCmd(CMD_RESET_ADDR);
        ICSP_CMD_GAP_FAST();
        /*
         * CMD_RESET_ADDR 将 PC 复位到 0x0000 (程序区起始),
         * 同步更新跟踪变量。
         */
        g_picCurrentArea = ICSP_AREA_PROGRAM;
        g_picCurrentAddress = 0U;
    }
}

/**
 * @brief  在当前PC地址执行一整行擦除（仅 enhanced mid-range 支持）
 * @return ICSP_OK=成功, ICSP_ERR=当前器件不支持
 *
 * 注意: Row Erase 不改变 PC 位置, 因此不需要更新跟踪变量。
 */
static uint8_t icspRowEraseCurrentAddress(void)
{
    if (icsp_pdev == NULL)
        return ICSP_ERR;
    if (icsp_pdev->common.core_family != PIC8_CORE_ENHANCED_14BIT)
        return ICSP_ERR;
    if (icsp_pdev->common.has_row_erase_cmd == 0U)
        return ICSP_ERR;

    icspLoadCmd(CMD_ROW_ERASE_PROG);
    ICSP_DELAY_US(icspGetDelayOrDefault(icsp_pdev->common.wait_rowerase_us,
                                        icsp_pdev->common.wait_erase_us));
    return ICSP_OK;
}

/**
 * @brief  跳转到配置空间的指定地址 (智能寻址)
 *
 * 优化策略:
 *   - 若当前已在配置空间且目标地址 >= 当前地址:
 *     仅通过 Increment 移动到目标 (避免复位+全路径递增)
 *   - 否则: 执行完整路径 (Reset → Load Config → Increment)
 *   - 若目标地址恰好等于当前地址: 直接返回, 零操作
 *
 * @param  targetAddr  目标配置地址 (必须 >= config_space_base)
 * @return ICSP_OK=成功, ICSP_ERR=失败
 */
static uint8_t icspGotoConfigAddress(uint32_t targetAddr)
{
    uint32_t stepCount;

    if (icsp_pdev == NULL)
        return ICSP_ERR;
    if (!ICSP_SUPPORTS_CONFIG_SPACE_FAST())
        return ICSP_ERR;
    if (targetAddr < icsp_pdev->common.config_space_base)
        return ICSP_ERR;

    /* === 情况1: 已在目标地址, 零操作 === */
    if (g_picCurrentArea == ICSP_AREA_CONFIG && g_picCurrentAddress == targetAddr)
        return ICSP_OK;

    /* === 情况2: 当前在配置空间, 目标地址更大 → 仅递增 === */
    if (g_picCurrentArea == ICSP_AREA_CONFIG && targetAddr > g_picCurrentAddress)
    {
        stepCount = targetAddr - g_picCurrentAddress;
        while (stepCount-- != 0U)
        {
            ICSP_INCREMENT_ADDRESS_FAST();
        }
        g_picCurrentAddress = targetAddr;
        return ICSP_OK;
    }

    /*
     * === 情况3: 需要完整路径 ===
     * 条件: 当前不在配置空间, 或目标地址小于等于当前地址
     * (ICSP PC 不能递减, 只能复位后重新递增)
     */
    stepCount = targetAddr - icsp_pdev->common.config_space_base;

    icspResetAddressRaw();
    icspLoadCmd(CMD_LOAD_CFG);
    ICSP_CMD_GAP_FAST();
    icspLoadData(0xFFF, g_picDataWidth);  /* Load Config Data (dummy) */
    ICSP_CMD_GAP_FAST();
    while (stepCount-- != 0U)
    {
        ICSP_INCREMENT_ADDRESS_FAST();
    }

    g_picCurrentArea = ICSP_AREA_CONFIG;
    g_picCurrentAddress = targetAddr;
    return ICSP_OK;
}

/**
 * @brief  重新回到程序区起点，用于按绝对地址重新定位PC
 *
 * 注意: 退出再重新进入编程模式后, PC 回到初始状态
 *       (通常为 0x0000 或 baseline 器件的程序末端),
 *       因此跟踪变量被重置为程序区初始地址。
 *
 * @return ICSP_OK=成功, ICSP_ERR=失败
 */
static uint32_t icspGetBaselineConfigPhys(void)
{
    if (icsp_pdev == NULL)
        return 0U;
    if (icsp_pdev->baseLine.config_shadow_addr != 0U)
        return icsp_pdev->baseLine.config_shadow_addr;
    return (icsp_pdev->common.code_end_addr << 1) - 1U;
}

static uint32_t icspGetBaselinePcSpace(void)
{
    if (icsp_pdev == NULL)
        return 0U;
    if (icsp_pdev->baseLine.config_shadow_addr != 0U)
        return icsp_pdev->baseLine.config_shadow_addr + 1U;
    return icsp_pdev->common.code_end_addr << 1;
}

static uint8_t icspRestartAndSyncCodeBase(void)
{
    uint32_t initAddr = 0U;

    if (icsp_pdev == NULL)
        return ICSP_ERR;

    if (g_picProgmodeActive)
        icspExit();

    if (g_picEnterPreferLvp && ICSP_SUPPORTS_LVP_FAST())
    {
        if (icspEnterLV(icsp_pdev) != ICSP_OK)
            return ICSP_ERR;
    }
    else
    {
        if (icspEnterHV(icsp_pdev) != ICSP_OK)
            return ICSP_ERR;
    }

    g_picProgmodeActive = 1U;

    /*
     * 重新进入编程模式后, PC 回到器件初始值:
     * - 14-bit (mid-range/enhanced): PC = 0x0000
     * - baseline (12-bit): 可能为 code_end-1 或 config_shadow_addr
     * 下面根据 pc_init_mode 计算初始 PC 值。
     */
    if (ICSP_IS_BASELINE_FAST())
    {
        uint32_t codeSpan = icsp_pdev->common.code_end_addr;
        if (codeSpan != 0U)
        {
            switch ((pic8_pc_init_mode_t)icsp_pdev->common.pc_init_mode)
            {
            case PIC8_PC_INIT_AT_TOP:
                initAddr = codeSpan - 1U;
                break;
            case PIC8_PC_INIT_AT_CONFIG:
                initAddr = icspGetBaselineConfigPhys();
                break;
            case PIC8_PC_INIT_AT_ZERO:
            default:
                initAddr = 0U;
                break;
            }
        }
    }

    #if UART1_TRACE
    uart1_WriteString("ICSP restart init=0x");
    uart1_WriteHex16((uint16_t)initAddr);
    uart1_WriteString("\r\n");
    #endif
    g_picCurrentArea = ICSP_AREA_PROGRAM;
    g_picCurrentAddress = initAddr;
    return ICSP_OK;
}

/**
 * @brief  计算 baseline 器件从初始PC位置走到目标程序地址所需的递增次数
 * @param  targetAddr 目标程序地址
 * @return 递增次数
 */
static uint8_t icspEnsureBaselineAtConfig(void)
{
    uint32_t cfgPhys;

    if (icsp_pdev == NULL)
        return ICSP_ERR;
    cfgPhys = icspGetBaselineConfigPhys();
    if (g_picCurrentArea == ICSP_AREA_PROGRAM && g_picCurrentAddress == cfgPhys)
        return ICSP_OK;
    return icspRestartAndSyncCodeBase();
}

static uint32_t icspGetBaselineProgramSteps(uint32_t targetAddr)
{
    uint32_t codeSpan = icsp_pdev->common.code_end_addr;
    uint32_t startAddr = 0U;
    uint32_t pcSpace = icspGetBaselinePcSpace();

    if (pcSpace == 0U)
        return targetAddr;

    switch ((pic8_pc_init_mode_t)icsp_pdev->common.pc_init_mode)
    {
    case PIC8_PC_INIT_AT_TOP:
        startAddr = (codeSpan != 0U) ? (codeSpan - 1U) : 0U;
        break;
    case PIC8_PC_INIT_AT_CONFIG:
        startAddr = icspGetBaselineConfigPhys();
        break;
    case PIC8_PC_INIT_AT_ZERO:
    default:
        startAddr = 0U;
        break;
    }

    if (startAddr < pcSpace)
        return (targetAddr + pcSpace - startAddr) % pcSpace;

    return targetAddr;
}

/**
 * @brief  在配置空间指定地址写入一个配置字 (智能寻址)
 * @param  targetAddr  目标配置地址
 * @param  value       要写入的配置字数据
 * @param  waitUs      编程等待时间（微秒）
 * @return ICSP_OK=成功, ICSP_ERR=失败
 */
/* Reset the four write latches by loading all of them with '1's.
 * Required after programming user ID / configuration words on 14-bit
 * parts (spec: the latches are NOT auto-reset by config-space writes). */
static uint8_t icspResetWriteLatches(void)
{
    uint8_t i;
    uint16_t allOnes;

    if (icsp_pdev == NULL)
        return ICSP_ERR;
    allOnes = icspGetBitMask(g_picDataWidth);
    for (i = 0U; i < 4U; i++)
    {
        icspLoadCmd(ICSP_LOAD_PROG_CMD_FAST());
        ICSP_CMD_GAP_FAST();
        icspLoadData(allOnes, g_picDataWidth);
        if (i < 3U)
            ICSP_INCREMENT_ADDRESS_FAST();
    }
    return ICSP_OK;
}

static uint8_t icspWriteConfigWordAt(uint32_t targetAddr, uint16_t value, uint16_t waitUs)
{
    if (icspGotoConfigAddress(targetAddr) != ICSP_OK)
        return ICSP_ERR;

    #if UART1_TRACE
    uart1_WriteString("ICSP cfgW addr=0x");
    uart1_WriteHex16((uint16_t)targetAddr);
    uart1_WriteString(" val=0x");
    uart1_WriteHex16(value);
    uart1_WriteString(" w=");
    uart1_WriteDec(g_picDataWidth);
    uart1_WriteString("\r\n");
    #endif
    icspLoadCmd(CMD_LOAD_PROG);
    ICSP_CMD_GAP_FAST();
    icspLoadData(value, g_picDataWidth);
    ICSP_BEGIN_PROGRAM_FAST(waitUs);

    /* keep the write latches clean for the next program-memory write */
    return icspResetWriteLatches();
}


/**
 * @brief  从配置空间指定地址读取一个配置字 (智能寻址)
 * @param  targetAddr  目标配置地址
 * @param  value       输出缓冲区，存放读取到的配置字数据
 * @return ICSP_OK=成功, ICSP_ERR=失败
 */
static uint8_t icspReadConfigWordAt(uint32_t targetAddr, uint16_t *value)
{
    if (value == NULL)
        return ICSP_ERR;
    if (icspGotoConfigAddress(targetAddr) != ICSP_OK)
        return ICSP_ERR;

    icspLoadCmd(CMD_READ_PROG);
    ICSP_CMD_GAP_FAST();
    *value = icspReadData(g_picDataWidth);

    /*
     * 读操作不改变 PC 位置, 保持 g_picCurrentArea / g_picCurrentAddress 不变。
     */
    return ICSP_OK;
}




/* 时序敏感代码段: 局部提升优化等级, 使 BSRR 字常量与地址缓存在寄存器,
 * 消除逐 bit 的字面量加载; 段外恢复工程默认优化等级 */
#pragma push
#pragma O2
/*
 * ---- 性能优化: BSRR 合并发送 ----
 *
 * ICSPCLK(PB3) 与 ICSPDAT(PB4) 同在 GPIOB, 发送时序改为:
 *   高相位: 一次 BSRR 写同时输出 CLK=1 与 DAT=本位值
 *   低相位: 一次 BSRR 写仅拉低 CLK, DAT 保持到下降沿被锁存
 * 相比原来每 bit 3 次位带写 (ICSP_CLK_H/ICSP_DAT_W/ICSP_CLK_L):
 *   - 位带写在总线矩阵中被翻译为对 ODR 的读-改-写, 单次开销更大
 *   - BSRR 为普通 32-bit 存储, 且 CLK/DAT 合并少一次总线写
 *   - 配合循环全展开, 消除循环控制开销
 *
 * 注意:
 *   1. 本方案要求 CLK 与 DAT 引脚同属一个 GPIO 端口 (当前均为 GPIOB),
 *      若日后引脚分属不同端口, 需退回位带写实现
 *   2. BSRR 的 set 区 (bit0..15) 优先级高于 reset 区 (bit16..31),
 *      因此 "置位DAT|复位DAT" 组合字等效于 DAT=1, 可用算术直接生成
 *   3. ICSP_CLK_DELAY 为运行时可调相位填充 (见 icsp.h 的 icspSetIcspClock)
 */
#define ICSP_BSRR_SET(pin)      (1UL << (pin))
#define ICSP_BSRR_RESET(pin)    (1UL << ((pin) + 16UL))

/* 两级展开: 先把 "B,3" 拆出端口 B, 再拼出 GPIOB */
#define ICSP_TX_GPIO_(_port)    STM_IO_GPIO(_port)
#define ICSP_TX_GPIO            ICSP_TX_GPIO_(GET_PORT_FROM(HWPIN_ICSP_CLK))
#define ICSP_TX_CLK_PIN         GET_PIN_FROM(HWPIN_ICSP_CLK)
#define ICSP_TX_DAT_PIN         GET_PIN_FROM(HWPIN_ICSP_DAT)

/* 高相位字: CLK=1, DAT 按本位值置位/复位 */
#define ICSP_TX_BSRR_HI(_p)     (ICSP_BSRR_SET(ICSP_TX_CLK_PIN) | \
                                 ICSP_BSRR_RESET(ICSP_TX_DAT_PIN) | \
                                 ((uint32_t)((_p) & 1UL) << ICSP_TX_DAT_PIN))
/* 低相位字: 仅 CLK=0, DAT 保持 */
#define ICSP_TX_BSRR_LO()       (ICSP_BSRR_RESET(ICSP_TX_CLK_PIN))

/* 发送 1 bit: 高相位(CLK+DAT 同步) + TDLY + 低相位 + TDLY */
#define ICSP_TX_BIT(_p)         do { \
                                    ICSP_TX_GPIO->BSRR = ICSP_TX_BSRR_HI(_p); \
                                    ICSP_CLK_DELAY; \
                                    ICSP_TX_GPIO->BSRR = ICSP_TX_BSRR_LO(); \
                                    ICSP_CLK_DELAY; \
                                } while (0)

/**
 * @brief  发送6位命令 (LSB first)
 *         时序: 高相位(CLK=1与DAT同步输出)→TDLY→低相位(CLK=0,DAT保持)→TDLY
 *         命令间至少 1us (TDLY1)
 * @param  cmd  6-bit 命令码
 */
void icspLoadCmd(uint8_t cmd)
{
    uint8_t i;

    /*
     * 固定 6-bit 命令逐位全展开 (g_picCmdWidth 恒为 6),
     * 消除循环控制开销; 位宽异常时回退通用循环保证兼容
     */
    ICSP_DAT_OUT();
    if (g_picCmdWidth == 6U)
    {
        ICSP_TX_BIT(cmd); cmd >>= 1U;
        ICSP_TX_BIT(cmd); cmd >>= 1U;
        ICSP_TX_BIT(cmd); cmd >>= 1U;
        ICSP_TX_BIT(cmd); cmd >>= 1U;
        ICSP_TX_BIT(cmd); cmd >>= 1U;
        ICSP_TX_BIT(cmd);
    }
    else
    {
        for (i = 0U; i < g_picCmdWidth; i++)
        {
            ICSP_TX_BIT(cmd);
            cmd >>= 1U;
        }
    }
    ICSP_DAT_L();                   /* 发送完成后数据线拉低 */
    ICSP_DAT_IN();                  /* 数据线重新切换为输入 */
}

/**
 * @brief  发送数据字，固定16个时钟周期
 *         时序: 前导(0) + data(width bit, LSB first) + 补位(0)
 *         共16个时钟周期
 * @param  data   要发送的数据
 * @param  width  数据位宽
 */
void icspLoadData(uint16_t data, uint8_t width)
{
    uint16_t pattern;

    /*
     * 预先一次性构造出16位发送序列 (LSB first):
     *   bit0          = 0                  (前导位)
     *   bit1..bitN    = data 位             (N = width)
     *   bitN+1..bit15 = 0                  (剩余时钟周期补0)
     * 16 个时钟周期为固定帧长, 逐位全展开消除循环开销
     */
    pattern = (uint16_t)((uint16_t)(data << 1U) &
                         (uint16_t)((1U << (width + 1U)) - 1U));

    ICSP_DAT_OUT();
    ICSP_TX_BIT(pattern);  pattern >>= 1U;
    ICSP_TX_BIT(pattern);  pattern >>= 1U;
    ICSP_TX_BIT(pattern);  pattern >>= 1U;
    ICSP_TX_BIT(pattern);  pattern >>= 1U;
    ICSP_TX_BIT(pattern);  pattern >>= 1U;
    ICSP_TX_BIT(pattern);  pattern >>= 1U;
    ICSP_TX_BIT(pattern);  pattern >>= 1U;
    ICSP_TX_BIT(pattern);  pattern >>= 1U;
    ICSP_TX_BIT(pattern);  pattern >>= 1U;
    ICSP_TX_BIT(pattern);  pattern >>= 1U;
    ICSP_TX_BIT(pattern);  pattern >>= 1U;
    ICSP_TX_BIT(pattern);  pattern >>= 1U;
    ICSP_TX_BIT(pattern);  pattern >>= 1U;
    ICSP_TX_BIT(pattern);  pattern >>= 1U;
    ICSP_TX_BIT(pattern);  pattern >>= 1U;
    ICSP_TX_BIT(pattern);
    ICSP_DAT_L();                    /* 发送结束, 数据线拉低 */
    ICSP_DAT_IN();                   /* 发送完成后切换为输入 */
}
#pragma pop


/**
 * @brief  接收数据字，固定16个时钟周期
 *         时序: 前导时钟 + data(width bit, LSB first) + 补位时钟
 *         共16个时钟周期，从第2个时钟开始读取数据
 * @param  width  数据位宽
 * @return 低位对齐的 width 位数据
 */
uint16_t icspReadData(uint8_t width)
{
    uint16_t val = 0;
    uint8_t  i;
    uint8_t  nbits;                       /* 实际有效数据位数 */

    /*
     * 16 时钟帧: 前导(1个) + 数据位(最多15个), 故实际有效数据位数为:
     *   nbits = min(width, 15)
     * 循环内不做"当前时钟是否有效"的判断, 16 个时钟全部右移累积读取
     * (bit0 为前导位, 无效), 循环结束后一次性 移位+掩码 消除前导/补位
     */
    nbits = (width < 16U) ? width : 15U;

    ICSP_DAT_IN();
    for (i = 0; i < 16; i++)
    {
        ICSP_CLK_H();
        ICSP_CLK_DELAY;
        val >>= 1U;                       /* 已有位右移腾位 */
        if (ICSP_DAT_R())
            val |= 0x8000U;               /* 新位进最高位, LSB first */
        ICSP_CLK_L();
        ICSP_CLK_DELAY;
    }
    return (uint16_t)((val >> 1U) & ((1U << nbits) - 1U)); /* 消除前导/补位 */
}

/* ================================================================= */
/* C层: 编程模式进入/退出                                               */
/* ================================================================= */

/**
 * @brief  高压进入编程模式 (VPP-first 或 VDD-first)
 *         根据器件参数自动选择VPP-first或VDD-first顺序
 *         不同系列电压:
 *           baseline(12-bit): VPP=12.5~13.5V, VDD=5.0V
 *           mid-range(14-bit): VPP=10.0~13.5V, VDD=5.0V
 *           enhanced(增强14-bit): VPP=8.0~9.0V, VDD=5.0V
 * @param  dev  器件参数指针
 * @return ICSP_OK=成功, ICSP_ERR=失败
 */
uint8_t icspEnterHV(const pic_prog_params_t *dev)
{
    if (dev == NULL) return ICSP_ERR;

    /* CLK=0, DAT=0, LVP/PGM=0, 均为输出 */
    ICSP_CLK_OUT(); ICSP_DAT_OUT(); ICSP_LVP_OUT();
    ICSP_CLK_L();   ICSP_DAT_L();   ICSP_LVP_L();
    ICSP_DELAY_US(10);

    /* 关VDD/VPP */
    ICSP_VDD_OFF();
    ICSP_VPP_OFF();
    ICSP_DELAY_US(icspGetDelayOrDefault(dev->common.icsp_off_delay_us, 10U));

    if(dev->common.has_vpp_first)
    {
        /* 先升VPP再升VDD (VPP-first) */
        ICSP_VPP_ON();
        ICSP_DELAY_US(icspGetDelayOrDefault(dev->common.icsp_vpp_first_delay_us, 5000U));
        ICSP_VDD_ON();
        ICSP_DELAY_US(icspGetDelayOrDefault(dev->common.icsp_enter_vdd_delay_us, 2000U));
    }
    else
    {
        /* 先升VDD再升VPP (VDD-first) */
        ICSP_VDD_ON();
        ICSP_DELAY_US(icspGetDelayOrDefault(dev->common.icsp_enter_vdd_delay_us, 2000U));
        ICSP_VPP_ON();
        ICSP_DELAY_US(icspGetDelayOrDefault(dev->common.icsp_enter_vpp_delay_us, 5000U));
    }
    ICSP_DELAY_US(icspGetDelayOrDefault(dev->common.icsp_enter_hv_stable_time_us, 2000U));
    return ICSP_OK;
}

/**
 * @brief  低压(LVP)进入编程模式
 *         传统LVP(如PIC16F627A): VDD=5V, MCLR=VDD, PGM=1
 *         密钥LVP(如PIC16F1825): VDD=5V, MCLR=0, 发"MCHP"密钥
 * @param  dev  器件参数指针
 * @return ICSP_OK=成功, ICSP_ERR=失败
 */
uint8_t icspEnterLV(const pic_prog_params_t *dev)
{
    uint8_t i;
    uint16_t entryDelay;

    if (dev == NULL) return ICSP_ERR;
    if (dev->common.lvp_mode == PIC8_LVP_NONE) return ICSP_ERR;

    ICSP_CLK_OUT(); ICSP_DAT_OUT(); ICSP_LVP_OUT();
    ICSP_CLK_L();   ICSP_DAT_L();   ICSP_LVP_L();
    ICSP_DELAY_US(10);

    ICSP_VDD_ON();
    ICSP_DELAY_US(icspGetDelayOrDefault(dev->common.icsp_enter_vdd_delay_us, 10U));
    entryDelay = icspGetDelayOrDefault(dev->common.wait_lvpgm_us, 100U);

    if (dev->common.lvp_mode == PIC8_LVP_MCHP_KEY)
    {
        /* 密钥型 LVP: MCLR=低, 发32位 "MCHP" 类密钥 */
        uint32_t key = dev->common.lvp_key_value;
        ICSP_VPP_OFF();
        ICSP_DAT_OUT();
        for (i = 0; i <= dev->common.lvp_key_bits; i++)
        {
            uint8_t b = (i < dev->common.lvp_key_bits) ? (uint8_t)((key >> i) & 1U) : 0;
            if (b) ICSP_DAT_H(); else ICSP_DAT_L();
            ICSP_CLK_DELAY;
            ICSP_CLK_H(); ICSP_CLK_DELAY;
            ICSP_CLK_L();
        }
        ICSP_DELAY_US(entryDelay);
    }
    else
    {
        /* 传统 LVP: PGM 拉高后等待器件进入低压编程 */
        ICSP_VPP_ON();
        ICSP_LVP_H();
        ICSP_DELAY_US(entryDelay);
    }
    return ICSP_OK;
}

/**
 * @brief  退出编程模式
 *         将跟踪变量重置为 ICSP_AREA_NONE
 */
void icspExit(void)
{
    uint16_t offDelay = 2000U;

    if (icsp_pdev != NULL)
        offDelay = icspGetDelayOrDefault(icsp_pdev->common.icsp_off_delay_us, 10U);

    /* drive MCLR/VPP low so the chip really exits Program/Verify mode;
     * floating the rail may leave the part inside programming mode and the
     * PC is then NOT reset to 0x0000 on the next entry. */
    ICSP_VPP_GND();
    #if UART1_TRACE
    uart1_WriteString("ICSP exit: VPP->GND\r\n");
    #endif
    ICSP_DELAY_US(1000);
    ICSP_VDD_OFF();
    ICSP_DELAY_US(offDelay);                    /* TRESET */
    ICSP_DAT_IN();
    ICSP_CLK_IN();
    ICSP_LVP_IN();
    ICSP_VPP_OFF();
    ICSP_VDD_OFF();

    /*
     * 退出编程模式后, PC 状态不再有效,
     * 将跟踪变量重置, 确保下次进入后强制走完整路径。
     */
    g_picCurrentArea = ICSP_AREA_NONE;
    g_picCurrentAddress = 0U;
}

/* ================================================================= */
/* D层: 器件编程原语与地址访问                                         */
/* ================================================================= */

/**
 * @brief  读取器件ID（Device ID）
 *         通过配置空间访问Device ID地址，获取器件标识
 * @return 器件ID值，失败返回0
 */
uint32_t icspReadDevID(void)
{
    uint16_t value;

    if (icsp_pdev == NULL) return 0;
    if (icsp_pdev->common.deviceid_addr == 0U) return 0;
    if (icspReadConfigWordAt(icsp_pdev->common.deviceid_addr, &value) != ICSP_OK)
        return 0;
    return value;
}

/**
 * @brief  读取器件签名（Device Signature）
 *         实质是读取Device ID
 * @param  sig  输出缓冲区，存放签名值
 * @return ICSP_OK=成功, ICSP_ERR=失败
 */
uint8_t icspReadSignature(uint16_t *sig)
{
    if (sig == NULL)
        return ICSP_ERR;
    *sig = (uint16_t)icspReadDevID();
#if DEBUG_HARDWARE_CONFIG
    uart1_WriteString("icspReadSignature: ");
    uart1_WriteHex16(*sig);
    uart1_WriteString("\r\n");
#endif
    return (*sig != 0U) ? ICSP_OK : ICSP_ERR;
}

/**
 * @brief  整片擦除（Bulk Erase）
 *         Baseline系列发送CMD12_BULK_ERASE
 *         14-bit系列先Load Config再发Erase命令
 *
 * 注意: 擦除后 PC 状态与编程模式初始状态一致,
 *       因此重置跟踪变量。
 *
 * @return ICSP_OK=成功, ICSP_ERR=失败
 */
uint8_t icspBulkErase(void)
{
    if (icsp_pdev == NULL) return ICSP_ERR;
    {
        uint8_t st = icspBackupCriticalWords();
        if (st != ICSP_OK)
            return st;
    }
    #if UART1_TRACE
    uart1_WriteString("ICSP erase begin\r\n");
    #endif

    if (icsp_pdev->common.core_family == PIC8_CORE_BASELINE_12BIT)
    {
        icspLoadCmd(CMD12_BULK_ERASE);
        ICSP_DELAY_US(icsp_pdev->common.wait_erase_us);
        icspLoadCmd(CMD12_END_PROG);
        ICSP_CMD_GAP_FAST();
    }
    else
    {
        icspLoadCmd(CMD_LOAD_CFG);
        ICSP_CMD_GAP_FAST();
        icspLoadData(0xFFF, g_picDataWidth);  /* Load Config Data (dummy) */
        ICSP_CMD_GAP_FAST();
        icspLoadCmd(CMD_ERASE_PROG);
        ICSP_DELAY_US(icsp_pdev->common.wait_erase_us);
        /*
         * 14-bit Bulk Erase 后 PC 位于配置空间起始,
         * 更新跟踪变量。
         */
        g_picCurrentArea = ICSP_AREA_CONFIG;
        g_picCurrentAddress = icsp_pdev->common.config_space_base;
    }
    #if UART1_TRACE
    uart1_WriteString("ICSP erase done\r\n");
    #endif

    /*
     * Baseline 器件: Bulk Erase 后状态较为特殊,
     * 为保险起见重置跟踪变量, 下次操作走完整路径。
     */
    if (ICSP_IS_BASELINE_FAST())
    {
        g_picCurrentArea = ICSP_AREA_NONE;
        g_picCurrentAddress = 0U;
    }

    return icspRestoreCriticalWords();
}

/**
 * @brief  编程一个字到程序存储器
 *         先发送Load命令装入数据，再发送Begin命令启动编程
 *
 * 注意: 编程操作不改变 PC 地址, 但调用方通常在循环中
 *       显式调用 Increment Address 和 icspSetProgramAddress 来推进。
 *
 * @param  data  要编程的指令字数据
 * @return ICSP_OK=成功, ICSP_ERR=失败
 */
uint8_t icspProgWord(uint16_t data)
{
    if (icsp_pdev == NULL) return ICSP_ERR;

    icspLoadCmd(ICSP_LOAD_PROG_CMD_FAST());
    ICSP_CMD_GAP_FAST();
    icspLoadData(data, g_picDataWidth);

    ICSP_BEGIN_PROGRAM_FAST(icsp_pdev->common.wait_pgm_us);
    return ICSP_OK;
}

/**
 * @brief  从程序存储器读取一个字
 *         先发送Read命令，再接收数据
 *
 * 注意: 读操作不改变 PC 地址。
 *
 * @return 读取到的指令字数据，失败返回0xFFFF
 */
uint16_t icspReadWord(void)
{
    uint32_t data;
    if (icsp_pdev == NULL) return 0xFFFF;

    icspLoadCmd(ICSP_READ_PROG_CMD_FAST());
    ICSP_CMD_GAP_FAST();
    data = icspReadData(g_picDataWidth);
    return (uint16_t)(data & 0xFFFF);
}

/**
 * @brief  编程一行数据到程序存储器（连续地址）
 *         每编程一个数据后自动递增地址
 *
 * 注意: 每编程一个数据后自动递增地址,
 *       因此跟踪变量 g_picCurrentAddress 同步递增。
 *
 * @param  buf  数据缓冲区
 * @param  cnt  数据个数
 * @return ICSP_OK=成功, ICSP_ERR=失败
 */
uint8_t icspProgRow(const uint16_t *buf, uint32_t cnt)
{
    uint32_t i;
    uint16_t row;
    if (buf == NULL || icsp_pdev == NULL) return ICSP_ERR;

    if (icsp_pdev->common.has_row_erase_cmd != 0U &&
        icsp_pdev->common.row_erase_words != 0U &&
        cnt <= icsp_pdev->common.row_erase_words)
    {
        (void)icspRowEraseCurrentAddress();
    }

    row = icsp_pdev->common.row_pgm_words;
    i = 0U;

    /* Block programming: fill the whole row (Load Data + Increment)
     * first, then issue ONE Begin Programming command for the row. */
    if (row >= 2U && row <= 8U)
    {
        /* head: single words until the PC is row-aligned */
        while (i < cnt && (g_picCurrentAddress % row) != 0U)
        {
            icspLoadCmd(ICSP_LOAD_PROG_CMD_FAST());
            ICSP_CMD_GAP_FAST();
            icspLoadData(buf[i], g_picDataWidth);
            ICSP_BEGIN_PROGRAM_FAST(icsp_pdev->common.wait_pgm_us);
            i++;
            if (i < cnt)
                ICSP_INCREMENT_ADDRESS_FAST();
        }

        /* aligned rows: fill row words, one Begin, one trailing increment */
        while (i + row <= cnt)
        {
            uint16_t k;
            for (k = 0U; k < row; k++)
            {
                icspLoadCmd(ICSP_LOAD_PROG_CMD_FAST());
                ICSP_CMD_GAP_FAST();
                icspLoadData(buf[i + k], g_picDataWidth);
                if (k + 1U < row)
                    ICSP_INCREMENT_ADDRESS_FAST();
            }
            ICSP_BEGIN_PROGRAM_FAST(icsp_pdev->common.wait_pgm_us);
            ICSP_INCREMENT_ADDRESS_FAST();
            i += row;
        }
    }

    /* tail (and the whole block when row programming is not active) */
    for (; i < cnt; i++)
    {
        icspLoadCmd(ICSP_LOAD_PROG_CMD_FAST());
        ICSP_CMD_GAP_FAST();
        icspLoadData(buf[i], g_picDataWidth);
        ICSP_BEGIN_PROGRAM_FAST(icsp_pdev->common.wait_pgm_us);
        if (i + 1U < cnt)
            ICSP_INCREMENT_ADDRESS_FAST();
    }
    return ICSP_OK;
}


/**
 * @brief  将程序存储器PC定位到指定逻辑地址 (智能寻址)
 *
 * 优化策略:
 *   - 若当前已在程序区且目标地址 >= 当前地址:
 *     仅通过 Increment 移动到目标 (避免退出/重新进入编程模式)
 *   - 若目标地址等于当前地址: 零操作直接返回
 *   - 否则: 退出编程模式 → 重新进入 → 递增至目标
 *
 * @param  addr 目标程序字地址
 * @return ICSP_OK=成功, ICSP_ERR=失败
 */
uint8_t icspSetProgramAddress(uint32_t addr)
{
    uint32_t stepCount;

    if (icsp_pdev == NULL)
        return ICSP_ERR;
    #if UART1_TRACE
    uart1_WriteString("ICSP setPA req=0x");
    uart1_WriteHex16((uint16_t)addr);
    uart1_WriteString(" area=");
    uart1_WriteDec(g_picCurrentArea);
    uart1_WriteString(" cur=0x");
    uart1_WriteHex16((uint16_t)g_picCurrentAddress);
    uart1_WriteString("\r\n");
    #endif

    /* === 情况1: 已在目标地址, 零操作 === */
    if (g_picCurrentArea == ICSP_AREA_PROGRAM && g_picCurrentAddress == addr)
        return ICSP_OK;

    /* === 情况2: 当前在程序区, 目标地址更大 → 仅递增 === */
    if (g_picCurrentArea == ICSP_AREA_PROGRAM && addr > g_picCurrentAddress)
    {
        stepCount = addr - g_picCurrentAddress;
        while (stepCount-- != 0U)
        {
            ICSP_INCREMENT_ADDRESS_FAST();
        }
        g_picCurrentAddress = addr;
        return ICSP_OK;
    }

    /*
     * === 情况3: 需要完整路径 ===
     * 条件: 当前不在程序区, 或目标地址小于等于当前地址
     * (ICSP PC 不能递减, 必须退出后重新进入再递增)
     */
    if (icspRestartAndSyncCodeBase() != ICSP_OK)
        return ICSP_ERR;

    /*
     * icspRestartAndSyncCodeBase 已经将 g_picCurrentArea/Address
     * 重置为程序区初始值, 下面计算从初始地址到目标地址的步数。
     */
    if (ICSP_IS_BASELINE_FAST())
    {
        /* baseline 从初始位置到目标需经过的步数由封装函数计算 */
        stepCount = icspGetBaselineProgramSteps(addr);
        /* 但 g_picCurrentAddress 已被设为初始地址, 后续递增会自动更新它 */
    }
    else
    {
        stepCount = addr;
    }

    while (stepCount-- != 0U)
    {
        /*
         * ICSP_INCREMENT_ADDRESS_FAST 内部包含 g_picCurrentAddress++,
         * 所以不需要额外更新跟踪变量。
         * 循环结束后 g_picCurrentAddress 会自动变为 addr。
         */
        ICSP_INCREMENT_ADDRESS_FAST();
        /*
         * 注意: g_picCurrentAddress 在宏内部递增, 但我们需要在循环
         * 结束后确认其值等于 addr。由于 stepCount = addr (non-baseline),
         * 从 0 递增 addr 次后 g_picCurrentAddress = addr。
         * 对于 baseline, 步数计算结果可能包含模运算, 但最终也能到达 target。
         */
    }

    /*
     * 以下注释为了清晰说明: 循环结束时,
     * g_picCurrentArea 已在 icspRestartAndSyncCodeBase 中设为 ICSP_AREA_PROGRAM,
     * g_picCurrentAddress 由于每次 Increment 都自动 +1, 最终等于 addr。
     * 但由于 stepCount 可能为 0, 且 baseline 有取模运算,
     * 我们显式更新以确保一致性。
     */
    g_picCurrentArea = ICSP_AREA_PROGRAM;
    g_picCurrentAddress = addr;
    return ICSP_OK;
}

/**
 * @brief  将数据EEPROM地址定位到指定字节地址 (智能寻址)
 *
 * EEPROM 的地址导航与配置空间共享 CMD_LOAD_CFG + Increment 机制,
 * 因此使用 icspGotoConfigAddress 定位, 实现 PC 跟踪复用。
 *
 * 优化策略同 icspGotoConfigAddress。
 *
 * @param  addr EEPROM 字节偏移地址
 * @return ICSP_OK=成功, ICSP_ERR=失败
 */
uint8_t icspSetEeAddress(uint32_t addr)
{
    uint32_t targetAddr;

    if (icsp_pdev == NULL)
        return ICSP_ERR;
    if (icsp_pdev->common.has_eeprom == 0U)
        return ICSP_ERR;
    if (icsp_pdev->common.eedata_base == 0U)
        return ICSP_ERR;

    targetAddr = icsp_pdev->common.eedata_base + addr;

    /*
     * 利用 icspGotoConfigAddress 的智能寻址:
     * - 若当前已在配置空间且 targetAddr >= 当前地址 → 增量式移动
     * - 否则走完整路径
     * - 若已经在该地址 → 零操作
     */
    return icspGotoConfigAddress(targetAddr);
}

/**
 * @brief  编程一个字节到数据EEPROM
 * @param  val  要写入的字节数据
 * @return ICSP_OK=成功, ICSP_ERR=失败
 */
uint8_t icspProgEE(uint8_t val)
{
    if (icsp_pdev == NULL) return ICSP_ERR;
    icspLoadCmd(CMD_LOAD_DATA);
    ICSP_CMD_GAP_FAST();
    icspLoadData(val, 8);
    ICSP_BEGIN_PROGRAM_FAST(icsp_pdev->common.wait_eedata_us);
    return ICSP_OK;
}

/**
 * @brief  从数据EEPROM读取一个字节
 * @param  val  输出缓冲区，存放读取到的字节数据
 * @return ICSP_OK=成功, ICSP_ERR=失败
 */
uint8_t icspReadEE(uint8_t *val)
{
    uint32_t data;
    if (val == NULL || icsp_pdev == NULL) return ICSP_ERR;
    icspLoadCmd(CMD_READ_DATA);
    ICSP_CMD_GAP_FAST();
    data = icspReadData(8);
    *val = (uint8_t)(data & 0xFF);
    return ICSP_OK;
}

/**
 * @brief  编程配置字（Config Word）(智能寻址)
 *         Baseline系列直接使用Load+Begin方式
 *         14-bit系列通过配置空间地址写入
 * @param  idx  配置字索引（0开始）
 * @param  val  要写入的配置字数据
 * @return ICSP_OK=成功, ICSP_ERR=失败
 */
uint8_t icspProgCfg(uint8_t idx, uint16_t val)
{
    uint32_t targetAddr;

    if (icsp_pdev == NULL) return ICSP_ERR;
    /* Merge impl bits: the host usually sends only the implemented fuses.
     * Fill the other bits per DCRDef (unimpl_val=1 -> all-ones, which never
     * clears calibration/factory content because programming only clears bits). */
    if (idx < MAX_CONFIG_WORDS)
    {
        uint16_t implMask = icsp_pdev->common.config_dcr[idx].impl_mask;
        if (implMask != 0U)
        {
            uint16_t widthMask = icspGetBitMask(g_picDataWidth);
            uint16_t unimplFill;
            if (icsp_pdev->common.config_dcr[idx].unimpl_val != 0U)
                unimplFill = (uint16_t)((uint16_t)~implMask & widthMask);
            else
                unimplFill = (uint16_t)(icsp_pdev->common.config_dcr[idx].default_value &
                                        (uint16_t)~implMask & widthMask);
            val = (uint16_t)((val & implMask) | unimplFill);
        }
    }
    if (icspGetConfigAddressByIndex(idx, &targetAddr) != ICSP_OK)
        return ICSP_ERR;

    if (icsp_pdev->common.core_family == PIC8_CORE_BASELINE_12BIT)
    {
        if (icspEnsureBaselineAtConfig() != ICSP_OK)
            return ICSP_ERR;
        return icspWriteProgramWordAt(targetAddr, val, icsp_pdev->common.wait_cfg_us);
    }

    /*
     * icspWriteConfigWordAt 内部调用 icspGotoConfigAddress,
     * 已包含智能寻址逻辑。
     */
    return icspWriteConfigWordAt(targetAddr, val, icsp_pdev->common.wait_cfg_us);
}

/**
 * @brief  读取配置字（Config Word）(智能寻址)
 *         Baseline系列直接使用Read命令
 *         14-bit系列通过配置空间地址读取
 * @param  idx  配置字索引（0开始）
 * @return 读取到的配置字数据，失败返回0xFFFF
 */
uint16_t icspReadCfg(uint8_t idx)
{
    uint16_t value;
    uint32_t targetAddr;

    if (icsp_pdev == NULL) return 0xFFFF;
    if (icspGetConfigAddressByIndex(idx, &targetAddr) != ICSP_OK)
        return 0xFFFF;

    if (icsp_pdev->common.core_family == PIC8_CORE_BASELINE_12BIT)
    {
        if (icspEnsureBaselineAtConfig() != ICSP_OK)
            return 0xFFFF;
        if (icspReadProgramWordAt(targetAddr, &value) != ICSP_OK)
            return 0xFFFF;
        return value;
    }

    /*
     * icspReadConfigWordAt 内部调用 icspGotoConfigAddress,
     * 已包含智能寻址逻辑。
     */
    if (icspReadConfigWordAt(targetAddr, &value) != ICSP_OK)
        return 0xFFFF;
    #if UART1_TRACE
    uart1_WriteString("ICSP cfgR idx=");
    uart1_WriteDec(idx);
    uart1_WriteString(" val=0x");
    uart1_WriteHex16(value);
    uart1_WriteString(" w=");
    uart1_WriteDec(g_picDataWidth);
    uart1_WriteString("\r\n");
    #endif
    return value;
}

/**
 * @brief  编程用户ID字（User ID）(智能寻址)
 *         Baseline系列直接使用Load+Begin方式
 *         14-bit系列通过配置空间地址写入
 * @param  idx  用户ID索引（0~3）
 * @param  val  要写入的用户ID数据
 * @return ICSP_OK=成功, ICSP_ERR=失败
 */
uint8_t icspProgUID(uint8_t idx, uint16_t val)
{
    uint32_t targetAddr;

    if (icsp_pdev == NULL || idx > 3) return ICSP_ERR;

    if (icsp_pdev->common.core_family == PIC8_CORE_BASELINE_12BIT)
    {
        targetAddr = icsp_pdev->common.userid_base + idx;
        return icspWriteWordByAbsoluteAddress(targetAddr, val, icsp_pdev->common.wait_userid_us);
    }

    if (idx >= icsp_pdev->common.userid_word_count && icsp_pdev->common.userid_word_count != 0U)
        return ICSP_ERR;
    targetAddr = icsp_pdev->common.userid_base + idx;

    /*
     * icspWriteConfigWordAt 内部调用 icspGotoConfigAddress,
     * 已包含智能寻址逻辑。
     */
    return icspWriteConfigWordAt(targetAddr, val, icsp_pdev->common.wait_userid_us);
}

/**
 * @brief  读取用户ID字（User ID）(智能寻址)
 *         Baseline系列直接使用Read命令
 *         14-bit系列通过配置空间地址读取
 * @param  idx  用户ID索引（0~3）
 * @return 读取到的用户ID数据，失败返回0xFFFF
 */
uint16_t icspReadUID(uint8_t idx)
{
    uint16_t value;

    if (icsp_pdev == NULL || idx > 3) return 0xFFFF;
    if (icsp_pdev->common.core_family == PIC8_CORE_BASELINE_12BIT)
    {
        if (icspReadWordByAbsoluteAddress(icsp_pdev->common.userid_base + idx, &value) != ICSP_OK)
            return 0xFFFF;
        return value;
    }

    if (idx >= icsp_pdev->common.userid_word_count && icsp_pdev->common.userid_word_count != 0U)
        return 0xFFFF;

    /*
     * icspReadConfigWordAt 内部调用 icspGotoConfigAddress,
     * 已包含智能寻址逻辑。
     */
    if (icspReadConfigWordAt(icsp_pdev->common.userid_base + idx, &value) != ICSP_OK)
        return 0xFFFF;
    return value;
}

/**
 * @brief  读取振荡器校准字（OSCCAL）(智能寻址)
 * @param  idx  校准字索引（0开始）
 * @return 读取到的校准字数据，失败返回0xFFFF
 */
uint16_t icspReadOSCCAL(uint8_t idx)
{
    uint16_t value;
    uint32_t targetAddr;

    if (icsp_pdev == NULL)
        return 0xFFFF;
    if (icspGetOsccalWordCountEffective() == 0U)
        return 0xFFFF;
    if (icspGetOsccalAddressByIndex(idx, &targetAddr) != ICSP_OK)
        return 0xFFFF;

    if (icspReadWordByAbsoluteAddress(targetAddr, &value) != ICSP_OK)
        return 0xFFFF;
    return value;
}

/**
 * @brief  编程振荡器校准字（OSCCAL）(智能寻址)
 * @param  idx  校准字索引（0开始）
 * @param  val  要写入的校准字数据
 * @return ICSP_OK=成功, ICSP_ERR=失败
 */
uint8_t icspWriteOSCCAL(uint8_t idx, uint16_t val)
{
    uint32_t targetAddr;

    if (icsp_pdev == NULL)
        return ICSP_ERR;
    if (icspGetOsccalWordCountEffective() == 0U)
        return ICSP_ERR;
    if (icspGetOsccalAddressByIndex(idx, &targetAddr) != ICSP_OK)
        return ICSP_ERR;

    return icspWriteWordByAbsoluteAddress(targetAddr, val, icsp_pdev->common.wait_cfg_us);
}

/* ================================================================= */
/* E层: 校验与安全                                                     */
/* ================================================================= */
/* F层: Family 驱动入口                                                */
/* ================================================================= */

/**
 * @brief  初始化PIC8编程器驱动
 *         根据器件参数设置命令位宽和数据位宽, 重置跟踪变量
 * @param  dev  器件参数指针
 */
void pic8Init(const pic_prog_params_t *dev)
{
    if (dev == NULL) return;
    icsp_pdev = dev;
    g_picProgmodeActive = 0U;
    g_picEnterPreferLvp = 0U;
    icspClearSavedParam();
    g_picCmdWidth = 6;                /* 所有PIC 8-bit核均为6-bit命令 */
    g_picDataWidth = dev->common.inst_bits;
    if (g_picDataWidth != 12U && g_picDataWidth != 14U)
        g_picDataWidth = (dev->common.core_family == PIC8_CORE_BASELINE_12BIT) ? 12U : 14U;

    /*
     * 初始化时重置 PC 跟踪变量,
     * 确保首次操作走完整路径。
     */
    g_picCurrentArea = ICSP_AREA_NONE;
    g_picCurrentAddress = 0U;

#if !ICSP_CLK_FAST
    icspSetIcspClock(ICSP_CLK_DEFAULT_HZ);  /* 默认位时钟, 可按器件调用调整 */
#endif
}

/**
 * @brief  进入编程模式（自动选择LVP或HVP）
 *         成功后重置 PC 跟踪变量为初始状态
 * @param  preferLvp  1=优先使用LVP, 0=使用HVP
 * @return ICSP_OK=成功, ICSP_ERR=失败
 */
uint8_t pic8EnterProgmode(uint8_t preferLvp)
{
    if (icsp_pdev == NULL)
        return ICSP_ERR;

    g_picEnterPreferLvp = (preferLvp != 0U) ? 1U : 0U;
    if (preferLvp && ICSP_SUPPORTS_LVP_FAST())
    {
        if (icspEnterLV(icsp_pdev) == ICSP_OK)
        {
            g_picProgmodeActive = 1U;

            /*
             * 进入编程模式后, PC 处于器件初始值:
             * - 14-bit: PC = 0x0000 (程序区起始)
             * - baseline: 取决于 pc_init_mode
             * 重置跟踪变量确保首操作走完整路径。
             */
            g_picCurrentArea = ICSP_AREA_NONE;
            g_picCurrentAddress = 0U;
            return ICSP_OK;
        }
        return ICSP_ERR;
    }
    if (icspEnterHV(icsp_pdev) == ICSP_OK)
    {
        g_picProgmodeActive = 1U;

        /* 同上, 重置跟踪变量 */
        g_picCurrentArea = ICSP_AREA_NONE;
        g_picCurrentAddress = 0U;
        return ICSP_OK;
    }
    return ICSP_ERR;
}

/**
 * @brief  退出编程模式
 *         icspExit 内部已重置跟踪变量为 ICSP_AREA_NONE
 */
void pic8LeaveProgmode(void)
{
    icspExit();
    g_picProgmodeActive = 0U;
    /*
     * icspExit 已设置:
     *   g_picCurrentArea = ICSP_AREA_NONE
     *   g_picCurrentAddress = 0U
     */
}

/* ================================================================= */
/* G2: User ID block helpers (absolute address, LE16 wire format)     */
/* ================================================================= */

uint8_t icspProgUserIdWords(uint32_t baseAddr, const uint8_t *data, uint16_t count)
{
    uint16_t i;
    if (data == NULL || icsp_pdev == NULL)
        return ICSP_ERR;
    for (i = 0U; i < count; i++)
    {
        uint16_t word = (uint16_t)data[i * 2U] | ((uint16_t)data[i * 2U + 1U] << 8);
        if (icspWriteWordByAbsoluteAddress(baseAddr + i, word,
                                           icsp_pdev->common.wait_userid_us) != ICSP_OK)
            return ICSP_ERR;
    }
    return ICSP_OK;
}

uint8_t icspReadUserIdWords(uint32_t baseAddr, uint8_t *out, uint16_t count)
{
    uint16_t i;
    if (out == NULL || icsp_pdev == NULL)
        return ICSP_ERR;
    for (i = 0U; i < count; i++)
    {
        uint16_t word;
        if (icspReadWordByAbsoluteAddress(baseAddr + i, &word) != ICSP_OK)
            return ICSP_ERR;
        out[i * 2U] = (uint8_t)(word & 0xFFU);
        out[i * 2U + 1U] = (uint8_t)(word >> 8);
    }
    return ICSP_OK;
}

/* ================================================================= */
/* G: STK500v2 protocol adapter layer                                 */
/*                                                                   */
/* Moved from USER/Stk500Protocol.c (former stkIcspProgramFlash,     */
/* stkIcspReadFlash, stkIcspProgramEeprom, stkIcspReadEeprom).       */
/* Organized like isp.c / hvproc.c: the protocol command structs      */
/* (stkProgramFlashIcsp_t / stkReadFlashIcsp_t) are taken directly   */
/* and the target address comes from the protocol-layer global        */
/* stkAddress (LOAD_ADDRESS register). The address is advanced after */
/* every written/read unit, keeping the same behavior as before.     */
/* ================================================================= */

static uint16_t icspGetLe16(const uint8_t *bytes)
{
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
}

static void icspPutLe16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)(value & 0xFFU);
    bytes[1] = (uint8_t)(value >> 8);
}

/* Program flash words (isEeprom=0) or data EEPROM bytes (isEeprom=1)
 * at the current protocol address, then advance stkAddress. */
uint8_t icspProgramMemory(stkProgramFlashIcsp_t *param, uint8_t isEeprom)
{
    uint16_t count;
    uint16_t i;
    uint16_t row;

    if (param == NULL)
        return STK_STATUS_CMD_FAILED;

    count = icspGetLe16(param->numWords);   /* EEPROM struct uses numBytes, same layout */
    if (isEeprom)
    {
        if (icspSetEeAddress(stkAddress.dword) != ICSP_OK)
            return STK_STATUS_CMD_FAILED;
    }
    else
    {
        if (icspSetProgramAddress(stkAddress.dword) != ICSP_OK)
            return STK_STATUS_CMD_FAILED;
    }

    row = (icsp_pdev != NULL) ? icsp_pdev->common.row_pgm_words : 0U;
    i = 0U;

    /* Block (multi-word) programming for flash when the device table defines
     * a row size of 2..8 words (e.g. 12F6XX 4-word mode). One Begin command
     * programs the whole row; the write latches auto-reset for program memory. */
    if (!isEeprom && row >= 2U && row <= 8U)
    {
        /* head: single words until the address is row-aligned */
        while (i < count && ((stkAddress.dword + i) % row) != 0U)
        {
            uint16_t word = icspGetLe16(&param->data[i * 2U]);
            if (icspProgWord(word) != ICSP_OK)
                return STK_STATUS_CMD_FAILED;
            ICSP_INCREMENT_ADDRESS_FAST();
            stkAddress.dword++;
            i++;
        }

        /* aligned rows: load row words, one Begin, one trailing increment */
        while (i + row <= count)
        {
            uint16_t k;
            for (k = 0U; k < row; k++)
            {
                uint16_t word = icspGetLe16(&param->data[(i + k) * 2U]);
                icspLoadCmd(ICSP_LOAD_PROG_CMD_FAST());
                ICSP_CMD_GAP_FAST();
                icspLoadData(word, g_picDataWidth);
                if (k + 1U < row)
                    ICSP_INCREMENT_ADDRESS_FAST();
            }
            ICSP_BEGIN_PROGRAM_FAST(icsp_pdev->common.wait_pgm_us);
            ICSP_INCREMENT_ADDRESS_FAST();
            stkAddress.dword += row;
            i += row;
        }
    }

    /* tail (and the whole block when row programming is not active) */
    for (; i < count; i++)
    {
        if (isEeprom)
        {
            if (icspProgEE(param->data[i]) != ICSP_OK)
                return STK_STATUS_CMD_FAILED;
        }
        else
        {
            uint16_t word = icspGetLe16(&param->data[i * 2U]);
            if (icspProgWord(word) != ICSP_OK)
                return STK_STATUS_CMD_FAILED;
        }
        if (i + 1U < count)
        {
            ICSP_INCREMENT_ADDRESS_FAST();
            stkAddress.dword++;
        }
    }
    if (count != 0U)
        stkAddress.dword++;
    return STK_STATUS_CMD_OK;
}

/* Read flash words (isEeprom=0) or data EEPROM bytes (isEeprom=1)
 * from the current protocol address, then advance stkAddress.
 * Returns status + data byte count (caller prepends the command byte). */
uint16_t icspReadMemory(stkReadFlashIcsp_t *param,
                        stkReadFlashIcspResult_t *result,
                        uint8_t isEeprom)
{
    uint16_t count;
    uint16_t i;

    if (param == NULL || result == NULL)
        return 0U;

    count = icspGetLe16(param->numWords);   /* EEPROM struct uses numBytes, same layout */
    if (isEeprom)
    {
        if (icspSetEeAddress(stkAddress.dword) != ICSP_OK)
        {
            result->status1 = STK_STATUS_CMD_FAILED;
            return 1U;
        }
    }
    else
    {
        if (icspSetProgramAddress(stkAddress.dword) != ICSP_OK)
        {
            result->status1 = STK_STATUS_CMD_FAILED;
            return 1U;
        }
    }

    result->status1 = STK_STATUS_CMD_OK;
    for (i = 0; i < count; i++)
    {
        if (isEeprom)
        {
            uint8_t value;
            if (icspReadEE(&value) != ICSP_OK)
            {
                result->status1 = STK_STATUS_CMD_FAILED;
                return 1U;
            }
            result->data[i] = value;
        }
        else
        {
            uint16_t word = icspReadWord();
            icspPutLe16(&result->data[i * 2U], word);
        }
        if (i + 1U < count)
        {
            ICSP_INCREMENT_ADDRESS_FAST();
            stkAddress.dword++;
        }
    }
    if (count != 0U)
        stkAddress.dword++;
    return (uint16_t)(1U + (isEeprom ? count : count * 2U));
}
