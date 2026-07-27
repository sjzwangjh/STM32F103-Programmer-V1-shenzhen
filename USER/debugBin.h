/*====================================================================
 * 本文件采用GB2312编码，禁止转换为UTF-8。
 * debugBin.h - UART1 binary debug protocol interface
 *
 * Frame byte order: little endian.
 * Request and response use the same frame layout:
 *   SOF1 SOF2 VER TYPE SEQ CMD STATUS LENGTH PAYLOAD CRC16
 *   A5   5A   u8  u8   u16 u16 u16    u16    N       u16
 * CRC16 uses CRC-16/CCITT-FALSE over VER through PAYLOAD.
 *====================================================================*/

#ifndef __DEBUG_BIN_H__
#define __DEBUG_BIN_H__

#include "sys.h"

#define DEBUG_BIN_SOF1                 0xA5U
#define DEBUG_BIN_SOF2                 0x5AU
#define DEBUG_BIN_PROTOCOL_VERSION     0x01U
#define DEBUG_BIN_MAX_PAYLOAD          128U

#define DEBUG_BIN_MSG_REQUEST          0x01U
#define DEBUG_BIN_MSG_RESPONSE         0x02U
#define DEBUG_BIN_MSG_EVENT            0x03U

/* Status codes returned in the STATUS field. */
#define DEBUG_BIN_STATUS_OK             0x0000U
#define DEBUG_BIN_STATUS_UNKNOWN_CMD    0x0001U
#define DEBUG_BIN_STATUS_BAD_LENGTH     0x0002U
#define DEBUG_BIN_STATUS_BAD_PARAM      0x0003U
#define DEBUG_BIN_STATUS_BAD_VERSION    0x0004U
#define DEBUG_BIN_STATUS_BAD_TYPE       0x0005U
#define DEBUG_BIN_STATUS_CRC_ERROR      0x0006U
#define DEBUG_BIN_STATUS_BUSY           0x0007U
#define DEBUG_BIN_STATUS_IO_ERROR       0x0008U
#define DEBUG_BIN_STATUS_NOT_SUPPORTED  0x0009U

/* System commands: 0x0000 - 0x00FF. */
#define DEBUG_BIN_CMD_PING              0x0001U
#define DEBUG_BIN_CMD_GET_INFO          0x0002U
#define DEBUG_BIN_CMD_GET_CAPABILITIES  0x0003U
#define DEBUG_BIN_CMD_USB_REENUMERATE   0x0010U

/* Board control commands: 0x0100 - 0x01FF. */
#define DEBUG_BIN_CMD_LED_SET           0x0101U
#define DEBUG_BIN_CMD_BEEP_SET          0x0102U
#define DEBUG_BIN_CMD_VBUS_SET          0x0103U

/* DUT power commands: 0x0200 - 0x02FF. */
#define DEBUG_BIN_CMD_VPP_SET           0x0201U
#define DEBUG_BIN_CMD_VDD_SET           0x0202U
#define DEBUG_BIN_CMD_DUT_VPP_ROUTE     0x0203U
#define DEBUG_BIN_CMD_DUT_VDD_ROUTE     0x0204U

/* DUT bus commands: 0x0300 - 0x03FF. */
#define DEBUG_BIN_CMD_DUT_PIN_CONFIG    0x0301U
#define DEBUG_BIN_CMD_DUT_PIN_READ      0x0302U
#define DEBUG_BIN_CMD_DUT_BUS_TEST      0x0310U
#define DEBUG_BIN_CMD_DELAY_TEST        0x0311U
#define DEBUG_BIN_CMD_PE_DIRECT_TEST    0x0312U

/* Measurement commands: 0x0400 - 0x04FF. */
#define DEBUG_BIN_CMD_ADC_READ          0x0401U
#define DEBUG_BIN_CMD_ADC_READ_ALL      0x0402U

/* 调试EEPROM数据 */
#define DEBUG_BIN_CMD_EEPROM_READ       0x0501U
#define DEBUG_BIN_CMD_EEPROM_WRITE      0x0502U
#define DEBUG_BIN_CMD_EEPROM_DEMO       0x0503U

/* 调试Flash数据 — 块读写，用于与USB HID双向验证 */
#define DEBUG_BIN_CMD_FLASH_READ        0x0601U
#define DEBUG_BIN_CMD_FLASH_WRITE       0x0602U
#define DEBUG_BIN_CMD_FLASH_DEMO        0x0603U
#define DEBUG_BIN_CMD_FLASH_DEMO_DMA    0x0604U

/* Capability bits returned by GET_CAPABILITIES. */
#define DEBUG_BIN_CAP_BOARD_IO          (1UL << 0)
#define DEBUG_BIN_CAP_DUT_POWER         (1UL << 1)
#define DEBUG_BIN_CAP_DUT_BUS           (1UL << 2)
#define DEBUG_BIN_CAP_ADC               (1UL << 3)
#define DEBUG_BIN_CAP_USB_REENUM        (1UL << 4)

void debugBin_Init(void);
void debugBin_Task(void);
void debugBin_ResetParser(void);

#endif
