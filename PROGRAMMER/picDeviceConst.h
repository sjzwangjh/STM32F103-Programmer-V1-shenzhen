/*
 * picDeviceConst.h - PIC10/12/16 器件参数常量表 (共享参数表法)
 *
 * 设计原则:
 *   将 457 个器件的初始化参数压缩存储, 通过共享表 + 器件索引两级结构实现。
 *   查询结果直接填充本文件中定义的 pic_prog_params_t,
 *   可直接赋给 icsp.c 中的 icsp_pdev 指针使用。
 *
 * 数据组织:
 *   共享参数表 (const, 存 Flash):
 *     g_powerTable[]   - 电源 + LVP 参数
 *     g_seqTable[]     - 编程时序 + 锁存器组合
 *     g_spaceTable[]   - 地址空间布局
 *     g_dcrTable[]     - DCRDef 掩码组
 *     g_subTable[]     - 子结构体字段 (config_shadow/cal_word/boundary)
 *
 *   器件索引表 (const, 存 Flash):
 *     g_deviceTable[]  - 每个器件 28 字节: 名称 + 4 个共享表索引 + 关键覆盖值
 *
 * API:
 *   pic8GetDeviceList()      获取支持的器件列表
 *   pic8FindDevice()         根据器件型号查找, 填充 pic_prog_params_t
 *   pic8FindDeviceByIndex()  根据索引查找
 *   pic8GetDeviceCount()     获取器件总数
 *
 * 与 icsp.c 的对接:
 *   pic_prog_params_t params;
 *   pic8FindDevice("PIC16F1825", &params);  // 填充 union
 *   pic8Init(&params);                      // 设置到 icsp.c 驱动
 */

#ifndef __PIC_DEVICE_CONST_H__
#define __PIC_DEVICE_CONST_H__

#include "sys.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef     DEVICE_NAME_CHAR_LENGTH
#define     DEVICE_NAME_CHAR_LENGTH     16
#elif (DEVICE_NAME_CHAR_LENGTH != 16)
#error "宏定义DEVICE_NAME_CHAR_LENGTH必须定义为16, 否则将影响整个架构"
#endif

/* ── 常数: 各共享表条目数 ────────────────────────────────────── */
#define PIC8_POWER_TABLE_SIZE       50
#define PIC8_SEQ_TABLE_SIZE         48
#define PIC8_SPACE_TABLE_SIZE       83
#define PIC8_DCR_TABLE_SIZE         64
#define PIC8_SUB_TABLE_SIZE         26
#define PIC8_DEVICE_TABLE_SIZE               407      // 实际器件数量 (457 个)
#define MAX_CONFIG_WORDS            4

typedef enum
{
    PIC8_CORE_BASELINE_12BIT = 0,
    PIC8_CORE_MIDRANGE_14BIT,
    PIC8_CORE_ENHANCED_14BIT
} pic8_core_family_t;

typedef enum
{
    PIC8_PC_INIT_AT_ZERO = 0,
    PIC8_PC_INIT_AT_TOP,
    PIC8_PC_INIT_AT_CONFIG
} pic8_pc_init_mode_t;

typedef enum
{
    PIC8_LVP_NONE = 0,
    PIC8_LVP_PGM_PIN,
    PIC8_LVP_MCHP_KEY
} pic8_lvp_mode_t;

typedef struct {
    char      dcr_name[12];
    uint32_t  dcr_addr;
    uint16_t  impl_mask;
    uint16_t  chksum_mask;
    uint16_t  default_value;
    uint16_t  factory_default;
    uint16_t  unused_mask;
    uint8_t   unimpl_val;
    uint8_t   nzwidth;
} pic8_dcr_entry_t;

typedef struct
{
    uint8_t  device_name[DEVICE_NAME_CHAR_LENGTH];
    uint8_t  core_family;
    uint8_t  pc_init_mode;
    uint8_t  inst_bits;
    uint8_t  data_bits;
    uint8_t  code_word_bytes;
    uint8_t  has_load_config_cmd;
    uint8_t  has_eeprom;
    uint8_t  has_checksum;

    uint16_t vpp_min_mv;
    uint16_t vpp_max_mv;
    uint16_t vdd_min_mv;
    uint16_t vdd_max_mv;
    uint16_t vdd_nominal_mv;
    uint16_t lvp_threshold_mv;

    uint16_t icsp_off_delay_us;
    uint16_t icsp_enter_vdd_delay_us;
    uint16_t icsp_enter_vpp_delay_us;
    uint16_t icsp_vpp_first_delay_us;
    uint16_t icsp_enter_hv_stable_time_us;

    uint8_t  erase_algo;
    uint8_t  tries;
    uint8_t  has_vpp_first;
    uint8_t  has_row_erase_cmd;

    uint8_t  lvp_mode;
    uint8_t  lvp_pin;
    uint8_t  lvp_key_required;
    uint8_t  lvp_key_bits;
    uint32_t lvp_key_value;

    uint16_t wait_pgm_us;
    uint16_t wait_erase_us;
    uint16_t wait_cfg_us;
    uint16_t wait_userid_us;
    uint16_t wait_eedata_us;
    uint16_t wait_rowerase_us;
    uint16_t wait_lvpgm_us;
    uint16_t wait_lverase_us;

    uint8_t  row_pgm_words;
    uint8_t  row_cfg_words;
    uint8_t  row_userid_words;
    uint8_t  row_eedata_words;
    uint8_t  row_erase_words;

    uint8_t  latch_pgm_words;
    uint8_t  latch_cfg_words;
    uint8_t  latch_userid_words;
    uint8_t  latch_eedata_words;
    uint8_t  latch_rowerase_words;

    uint32_t code_base_addr;
    uint32_t code_end_addr;
    uint32_t config_space_base;
    uint32_t config_addr;
    uint8_t  config_word_count;
    uint32_t userid_base;
    uint8_t  userid_word_count;
    uint32_t deviceid_addr;
    uint16_t deviceid_mask;
    uint16_t deviceid_expected;
    uint32_t eedata_base;
    uint32_t eedata_end_addr;
    uint32_t osccal_base;
    uint8_t  osccal_word_count;
    uint32_t cal_data_base;
    uint8_t  cal_data_word_count;

    pic8_dcr_entry_t config_dcr[MAX_CONFIG_WORDS];
} pic8_icsp_common_t;

/* Variant-only fields are merged into a single extension block.
 * Legacy member access is preserved through compatibility macros below.
 */
typedef struct
{
    uint32_t config_shadow_addr;
    uint32_t osccal_addr;
    uint32_t config2_addr;
    uint32_t config3_addr;
    uint32_t config4_addr;
    uint32_t cal_word1_addr;
    uint32_t cal_word2_addr;
    uint32_t debug_reserved_base;
    uint32_t debug_reserved_end;
    uint16_t boundary_words;
    uint16_t reserved;
} pic8_variant_fields_t;

typedef struct
{
    pic8_icsp_common_t    common;
    pic8_variant_fields_t variant;
} pic_prog_params_t;

#define baseLine   variant
#define midRange   variant
#define enhanced   variant

typedef struct
{
    uint16_t config_word;
    uint16_t config2_word;
    uint16_t config3_word;
    uint16_t config4_word;
    uint16_t osccal_word;
    uint16_t cal_word1;
    uint16_t cal_word2;
} pic_saved_param_t;

typedef enum
{
    PIC_SECTION_PROGRAM_MEMORY = 0,
    PIC_SECTION_DATA_EEPROM,
    PIC_SECTION_CONFIG_WORDS,
    PIC_SECTION_USER_ID,
    PIC_SECTION_DEVICE_ID_EXPECTED,
    PIC_SECTION_OSCCAL_BACKUP,
    PIC_SECTION_CALIBRATION_DATA
} pic_section_type_t;

typedef struct
{
    uint16_t section_type;
    uint16_t section_index;
    uint32_t target_addr;
    uint32_t item_count;
    uint16_t item_width_bits;
    uint16_t flags;
    uint32_t payload_offset;
    uint32_t payload_size;
    uint32_t expected_value;
    uint32_t expected_mask;
    uint32_t crc32;
} pic8_section_desc_t;

/* ── 共享参数表条目类型 ──────────────────────────────────────── */

/** 电源 + LVP 参数条目 */
typedef struct {
    uint16_t vpp_min_mv;
    uint16_t vpp_max_mv;
    uint16_t vdd_min_mv;
    uint16_t vdd_max_mv;
    uint16_t vdd_nominal_mv;
    uint16_t lvp_threshold_mv;
    uint8_t  has_vpp_first;
    uint8_t  lvp_mode;          /* pic8_lvp_mode_t */
} pic8_power_entry_t;

/** 编程时序 + 锁存器/行大小组合条目 */
typedef struct {
    /* 时序 */
    uint16_t wait_pgm_us;
    uint16_t wait_erase_us;
    uint16_t wait_cfg_us;
    uint16_t wait_userid_us;
    uint16_t wait_eedata_us;
    uint16_t wait_rowerase_us;
    uint16_t wait_lvpgm_us;
    uint16_t wait_lverase_us;
    /* 算法 */
    uint8_t  erase_algo;
    uint8_t  tries;
    uint8_t  has_row_erase_cmd;
    /* 行大小 */
    uint8_t  row_pgm_words;
    uint8_t  row_cfg_words;
    uint8_t  row_userid_words;
    uint8_t  row_eedata_words;
    uint8_t  row_erase_words;
    /* 锁存器 */
    uint8_t  latch_pgm_words;
    uint8_t  latch_cfg_words;
    uint8_t  latch_userid_words;
    uint8_t  latch_eedata_words;
    uint8_t  latch_rowerase_words;
} pic8_seq_entry_t;

/** 地址空间布局条目 */
typedef struct {
    uint32_t code_base_addr;
    uint32_t code_end_addr;
    uint32_t config_space_base;
    uint32_t config_addr;
    uint8_t  config_word_count;
    uint32_t userid_base;
    uint8_t  userid_word_count;
    uint32_t deviceid_addr;
    uint32_t eedata_base;
    uint32_t eedata_end_addr;
    uint32_t osccal_base;
    uint8_t  osccal_word_count;
    uint32_t cal_data_base;
    uint8_t  cal_data_word_count;
    /* 子结构体附加字段索引 */
    uint8_t  sub_index;     /* 索引到 g_subTable */
} pic8_space_entry_t;

/** DCRDef 组条目 - 一组配置字 (最多 4 个) */
typedef struct {
    uint8_t  dcr_count;
    uint8_t  pad[3];
    pic8_dcr_entry_t dcr[MAX_CONFIG_WORDS];
} pic8_dcr_group_t;
typedef struct {
    uint32_t config_shadow_addr;
    uint32_t osccal_addr;
    uint32_t config2_addr;
    uint32_t config3_addr;
    uint32_t config4_addr;
    uint32_t cal_word1_addr;
    uint32_t cal_word2_addr;
    uint32_t debug_reserved_base;
    uint32_t debug_reserved_end;
    uint16_t boundary_words;
    uint16_t reserved;
} pic8_sub_entry_t;

#define PIC8_DEVICE_INDEX_INVALID 0xFFFFU

/* ── 器件索引条目 ──────────────────────────────────────────── */
typedef struct {
    char     name[16];          /* 器件型号名称 (空结尾) */
    uint8_t  power_idx;         /* 索引到 g_powerTable */
    uint8_t  seq_idx;           /* 索引到 g_seqTable */
    uint8_t  space_idx;         /* 索引到 g_spaceTable */
    uint8_t  dcr_idx;           /* 索引到 g_dcrTable */
    uint8_t  core_family;       /* pic8_core_family_t */
    uint8_t  pc_init_mode;      /* pic8_pc_init_mode_t */
    uint8_t  has_eeprom;        /* 0/1 */
    uint8_t  inst_bits;         /* 12 或 14 */
    uint16_t deviceid_mask;
    uint16_t deviceid_expected;
} pic8_device_index_t;

/* ── API 函数原型 ────────────────────────────────────────────── */

/**
 * @brief  获取支持的器件列表
 * @param  startIndex     [输入] 起始索引
 * @param  rdCount  [输入] 读取数量
 * @return 实际器件总数 (即使超过 maxCount 也返回总数)
 */
uint16_t pic8GetDeviceList(uint16_t startIndex, uint16_t rdCount);

/**
 * @brief  根据器件型号名称查找, 填充 pic_prog_params_t
 *
 * @param  deviceName [输入] 器件型号, 如 "PIC16F1825"
 * @param  out        [输出] 指向 pic_prog_params_t 的指针 (union)
 * @return 0=找到, -1=未找到
 *
 * 查找策略:
 *   1. 精确匹配 (大小写不敏感)
 *   2. 自动剥离 ACxxx_AS_ 前缀后再匹配
 *
 * 用法:
 *   pic_prog_params_t dev;
 *   if (pic8FindDevice("PIC16F1825", &dev) == 0) {
 *       pic8Init(&dev);  // 设置 icsp.c 驱动
 *   }
 */
int8_t pic8FindDeviceByName(const char *deviceName, pic_prog_params_t *out);

/**
 * @brief  根据索引获取器件完整参数 (O(1))
 * @param  index [输入] 器件索引号
 * @param  out   [输出] 指向 pic_prog_params_t 的指针
 * @return 0=找到, -1=索引越界
 */
int8_t pic8FindDeviceByIndex(uint16_t index, pic_prog_params_t *out);

/**
 * @brief  获取支持的器件总数
 * @return 器件数量
 */
uint16_t pic8GetDeviceCount(void);

/**
 * @brief  获取器件索引条目 (只读)
 * @param  index [输入] 器件索引号
 * @param  entry [输出] 指向索引条目的指针
 * @return 0=找到, -1=索引越界
 */
int8_t pic8GetDeviceEntry(uint16_t index, const pic8_device_index_t **entry);

#ifdef __cplusplus
}
#endif

#endif /* __PIC_DEVICE_CONST_H__ */
