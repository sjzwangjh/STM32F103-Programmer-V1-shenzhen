#ifndef __MCP4017_CALIBRATION_H__
#define __MCP4017_CALIBRATION_H__

#include "sys.h"

/* A single 32-byte EEPROM page reserved for per-programmer rail calibration. */
#define MCP4017_CALIBRATION_EEPROM_ADDR       0x0020UL
#define MCP4017_CALIBRATION_EEPROM_SIZE       32U
#define MCP4017_CALIBRATION_MAGIC             0x314C4143UL
#define MCP4017_CALIBRATION_VERSION           1U
#define MCP4017_CALIBRATION_CRC_OFFSET         22U

/* Three-programmer source-output fit; use these values when provisioning. */
#define MCP4017_CALIBRATION_VDD_BASE_DEFAULT  782U
#define MCP4017_CALIBRATION_VDD_GAIN_DEFAULT  232819UL
#define MCP4017_CALIBRATION_VDD_OFFSET_DEFAULT 3948U
#define MCP4017_CALIBRATION_VPP_BASE_DEFAULT  551U
#define MCP4017_CALIBRATION_VPP_GAIN_DEFAULT  329780UL
#define MCP4017_CALIBRATION_VPP_OFFSET_DEFAULT 4182U

/*
 * Voltage model in mV: V = base_mv + gain / (tap + offset_milli / 1000).
 * The stored record is little-endian because it is written by this STM32.
 * A fully erased page is intentionally invalid and selects the legacy model.
 */
typedef struct
{
    uint32_t magic;
    uint8_t version;
    uint8_t record_size;
    uint16_t vdd_base_mv;
    uint32_t vdd_gain;
    uint16_t vdd_offset_milli;
    uint16_t vpp_base_mv;
    uint32_t vpp_gain;
    uint16_t vpp_offset_milli;
    uint16_t crc16;
    uint8_t reserved[8];
} mcp4017_calibration_t;

typedef char mcp4017_calibration_record_size_must_be_32[
    (sizeof(mcp4017_calibration_t) == MCP4017_CALIBRATION_EEPROM_SIZE) ? 1 : -1];

#endif
