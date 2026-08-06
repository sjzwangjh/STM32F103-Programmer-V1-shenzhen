/*
 * 名称: Hardware_Config.h
 * 说明: 统一定义本项目使用的硬件引脚和功能开关。
 *      其他模块只依赖这里的宏，不直接写死具体引脚。
 */

#ifndef __HARDWARE_CONFIG_H__
#define __HARDWARE_CONFIG_H__

#ifdef USE_BOOTLOADER
  #define APP_START_ADDER  0x0800C000   // bootloader 占 48KB
#else
  #define APP_START_ADDER  0x08000000   // 独立运行
#endif

#define  DEBUG_HARDWARE_CONFIG      1       // 硬件调试标志

#define  HW_USB_HID_SPEED_FULL      1       // USB HID 全速模式（1=全速，0=USB1.1兼容模式）

/* -------- LED -------- */
#define HW_LED_ACTIVE               C,0
#define HW_LED_RESET                C,1
#define HW_LED_HALT                 C,2
#define HWPIN_LED                   HW_LED_ACTIVE

/* -------- BEEP -------- */
#define HW_BEEP                     B,9

/* -------- KEY -------- */
#define HW_BTN_UP                   D,13
#define HW_BTN_DOWN                 D,12
#define HW_BTN_ENTER                D,15
#define HW_BTN_BACK                 D,14

/* -------- LCD12864 / GB2312 字库 -------- */
#define HW_LCD12864_GB2312_MOSI     C,5
#define HW_GB2312_MISO              B,0
#define HW_LCD12864_GB2312_SCK      B,1
#define HW_GB2312_CS                E,7
#define HW_LCD12864_RS              E,8
#define HW_LCD12864_RST             B,10
#define HW_LCD12864_CS              E,10

/* -------- 数控电位器 IIC -------- */
#define HW_DVR_VPP_IIC_SCL          D,5
#define HW_DVR_VPP_IIC_SDA          D,6
#define HW_DVR_VDD_IIC_SCL          B,7
#define HW_DVR_VDD_IIC_SDA          B,6

/* -------- 电源总控 -------- */
#define HW_USB_ON                   C,3

/* -------- DUT 总线 --------
 * PIN1: VPP / RESET
 * PIN2: VDD
 * PIN3: GND
 * PIN4: ICSP_DAT / TDO / MISO / SDO
 * PIN5: ICSP_CLK / TCK / SCK / DW
 * PIN6: ICSP_LVP / TDI / MOSI / SDI
 * PIN7: TMS / SII
 * PIN8: 预留
 */
#define HW_DUT_VPP_VH_ON            E,12
#define HW_DUT_VPP_VL_ON            E,13
#define HW_DUT_VDD_VH_ON            E,14
#define HW_DUT_VDD_VL_ON            E,15

#define HW_DUT_PIN4_CTRL            D,3
#define HW_DUT_PIN4_DAT             B,4
#define HW_DUT_PIN5_CTRL            D,4
#define HW_DUT_PIN5_DAT             B,3
#define HW_DUT_PIN6_CTRL            E,0
#define HW_DUT_PIN6_DAT             E,2
#define HW_DUT_PIN7_CTRL            E,4
#define HW_DUT_PIN7_DAT             B,5
#define HW_DUT_PIN8_CTRL            E,6
#define HW_DUT_PIN8_DAT             E,5

/* -------- HANDLER -------- */
#define HW_HANDLER_OK               C,6
#define HW_HANDLER_NG               C,7
#define HW_HANDLER_BUSY             A,8
#define HW_HANDLER_START            D,1
#define HW_HANDLER_UD               D,0

/* -------- 功能开关 -------- */
#define ENABLE_DEBUG_INTERFACE      1
#define ENABLE_HID_INTERFACE        1
#define ENABLE_HVPROG               1
#define HW_DEBUG_BAUDRATE           256000

/* -------- SPI2: EEPROM / FLASH 共用 -------- */
#define HW_SPI2_SCK                 B,13
#define HW_SPI2_SDI                 B,14
#define HW_SPI2_SDO                 B,15

#define HW_SPI_EEPROM_CS            D,11
#define HW_SPI_EEPROM_WP            D,10
#define HW_FLASH_CS                 D,9
#define HW_FLASH_WP                 D,8

/* -------- SDIO -------- */
#define HW_SDIO_DAT0                C,8
#define HW_SDIO_DAT1                C,9
#define HW_SDIO_DAT2                C,10
#define HW_SDIO_DAT3                C,11
#define HW_SDIO_CLK                 C,12
#define HW_SDIO_CMD                 D,2

/* -------- ADC -------- */
#define HW_VREF_PLUS                2.5
#define HW_VREF_NEG                 0

/* 原本分为ADC1和ADC2，但是ADC2没有DMA传输，所以都改为ADC1采集 */
#define HW_ADC1_IN1_VDD_FBACK       A,1     // DUT VDD电压采集
#define HW_ADC1_IN2_VPP_MAIN_FBACK  A,2     // DUT 主电源VPP电压采集
#define HW_ADC1_IN3_USB_GOOD        A,3     // USB输入电压采集
#define HW_ADC1_IN4_DUT_IVDD        A,4     // DUT VDD电流采集
#define HW_ADC1_IN5_DUT_IVPP        A,5     // DUT VPP电流采集
#define HW_ADC1_IN6_3V3_POWER_GOOD  A,6     // MCU 3.3V工作电压采集
#define HW_ADC1_IN7_DUT_UVPP        A,7     // DUT VPP电压采集
#define HW_ADC1_IN14_VDD_MAIN_FBACK C,4     // DUT 主电源VDD电压采集


#endif

