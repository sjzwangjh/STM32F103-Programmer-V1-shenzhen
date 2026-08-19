#ifndef __BOOT_APP_COMMON_H_INCLUDED__
#define __BOOT_APP_COMMON_H_INCLUDED__

/*
 * Boot / App 公共契约定义（唯一来源，双端各存一份）
 * ------------------------------------------------------------
 * 本文件同时在两个工程中各保留一份：
 *   - STM32F103VET6_Bootloader_V1\USER\bootAppCommon.h
 *   - STM32F103VET6_Programmer_V1\USER\bootAppCommon.h
 *
 * 两个工程相互独立，不依赖共享目录；修改本文件时，
 * 必须同步修改另一份，保证 App 起始/结束地址、EEPROM 标志、
 * RAM 跳转痕迹在 Boot 与 App 两端完全一致。
 */

/* ===== App 程序分区（STM32F103VE，512KB 片内 Flash） ===== */
#define APP_START_ADDER         0x0800C000UL  /* App 起始地址：Boot 占用 0x08000000~0x0800BFFF（48KB） */
#define FLASH_APP_END           0x08080000UL  /* App 区域结束地址（不包含，片内 Flash 末尾） */
#define APP_FLASH_SIZE          (FLASH_APP_END - APP_START_ADDER)  /* App 可用空间 464KB */

/* ===== RAM 跳转痕迹标记 ===== */
#define APP_TRACE_MARK_ADDR     0x2000FFF0UL  /* Boot 跳转前 / App 启动早期共用的 RAM 标记地址 */
#define APP_TRACE_MARK_BOOT     0x424F4F54UL  /* "BOOT"：Boot 即将跳转时写入 */
#define APP_TRACE_MARK_APPS     0x41505053UL  /* "APPS"：App 上电早期写入 */

/* ===== SPI EEPROM 标志 ===== */
#define EEPROM_BOOT_MODE_ADDR   0x0200UL      /* 启动模式标志：UPDATE=0xFF 进入升级，APP=0x00 直接跳 App */
#define EEPROM_APP_VALID_ADDR   0x0201UL      /* App 有效性标志 */
#define EEPROM_BOOT_MODE_UPDATE 0xFFUL        /* 固件升级模式 */
#define EEPROM_BOOT_MODE_APP    0x00UL        /* 正常启动（跳 App） */
#define EEPROM_APP_VALID_VALUE  0xA5UL        /* App 有效标记值 */

#endif /* __BOOT_APP_COMMON_H_INCLUDED__ */
