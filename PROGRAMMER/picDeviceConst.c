/* Auto-generated complete PIC device table (285 devices) */
/* Generated from pic10-12-16-init.xml */
#include "picDeviceConst.h"
#include "Hardware_Config.h"
#include <string.h>
#include <stdlib.h>

/* Resolve operations are frequent; enable only when investigating the device DB. */
#define PIC8_DB_TRACE 0

/*
 * PICkit2 stores PIC memory addresses in Intel HEX byte units. This overlay
 * contains only fields that map losslessly to the STM32 ICSP model; timing and
 * PK2 firmware script IDs intentionally remain outside the programming path.
 */
typedef struct {
    char     name[16];
    uint32_t program_words;
    uint32_t config_word_addr;
    uint32_t userid_word_addr;
    uint8_t  config_words;
    uint8_t  userid_words;
    uint8_t  osccal_save;
} pic8_pk2_supplement_t;

/* ICSP 内置器件轮廓:
 * 1 = baseline 12-bit (以 PIC12F508 类器件为参考)
 * 2 = mid-range 14-bit (以 PIC16F627A/628A/648A 类器件为参考)
 * 3 = enhanced mid-range (以 PIC16F1825/1829 类器件为参考)
 */

/* Power shared table (27 entries) */



/* Power shared table (26 entries) */
static const pic8_power_entry_t g_powerTable[] = {
  {12750,13250,3000,5500,5000,4500,1,1},
  {10000,12000,3000,5500,5000,4500,1,1},
  {12500,13500,3000,5500,5000,0,1,0},
  {10000,12000,3000,5500,5000,0,1,0},
  {10000,12000,4500,5500,5000,4500,1,1},
  {10000,13000,4500,4750,4750,4500,1,1},
  {8000,9000,1800,5500,5000,0,1,0},
  {8000,9000,1800,3600,3300,0,1,0},
  {10000,13000,4500,5500,5000,4500,1,1},
  {12500,13500,2700,3700,3300,0,1,0},
  {12750,13250,3000,5500,5000,4500,0,1},
  {12750,13250,3000,5500,5000,0,1,0},
  {12500,13500,4500,5500,5000,0,1,0},
  {12750,13250,4500,5500,5000,0,1,0},
  {12750,13250,3000,6000,5000,0,1,0},
  {12750,13250,4000,5500,5000,0,1,0},
  {12750,13250,4375,5250,5000,0,1,0},
  {2000,5500,4000,5500,5000,0,1,0},
  {12000,14000,4500,5500,5000,0,1,0},
  {8000,9000,1800,5500,5000,2700,1,2},
  {8000,9000,1800,3600,3300,2700,1,2},
  {8000,9000,2300,5500,5000,2700,1,2},
  {8000,9000,1800,3600,3300,2600,1,2},
  {8000,9000,1800,3600,3000,2700,1,2},
  {7900,9000,1800,3600,3300,2700,1,2},
  {8000,9000,2300,3600,3300,2700,1,2},
};

/* Seq+latch shared table (23 entries) */
static const pic8_seq_entry_t g_seqTable[] = {
  {2000,8000,2500,2500,6000,0,2500,0,4,1,0,1,1,1,1,0,1,1,1,1,0},
  {2500,6000,6000,6000,6000,0,2500,2500,1,1,1,4,1,1,1,16,4,1,1,1,16},
  {3000,10000,2000,2000,0,0,2000,0,1,1,0,1,1,1,0,0,1,1,1,0,0},
  {2500,6000,6000,6000,0,0,2500,2500,1,1,1,1,1,1,0,16,1,1,1,0,16},
  {2500,6000,6000,6000,0,0,2500,2500,1,1,1,4,1,1,0,16,4,1,1,0,16},
  {2500,6000,6000,6000,0,0,2500,2500,1,1,0,1,1,1,0,0,1,1,1,0,0},
  {3000,10000,2000,2000,0,0,2000,0,3,1,0,1,1,1,0,0,1,1,1,0,0},
  {2500,6000,5000,2500,0,0,2500,0,1,1,1,16,1,1,0,16,16,1,1,0,16},
  {2500,6000,6000,6000,0,0,2500,2500,1,1,1,4,1,4,0,16,4,1,4,0,16},
  {2000,10000,2000,2000,0,0,2000,0,4,1,1,1,1,1,0,8,1,1,1,0,8},
  {2000,10000,2000,2000,0,0,2000,0,4,1,0,1,1,1,0,0,1,1,1,0,0},
  {100,0,100,100,0,0,0,0,0,8,0,1,1,1,0,0,1,1,1,0,0},
  {100,0,100,100,0,0,0,0,0,25,0,1,1,1,0,0,1,1,1,0,0},
  {0,0,0,0,0,0,0,0,0,1,0,1,1,1,0,0,1,1,1,0,0},
  {2500,6000,5000,2500,8000,0,2500,0,1,1,1,8,1,1,1,32,8,1,1,1,32},
  {2500,6000,5000,2500,8000,0,2500,0,1,1,1,32,1,1,1,32,32,1,1,1,32},
  {2500,6000,5000,2500,8000,0,2500,0,1,1,1,16,1,1,1,16,16,1,1,1,16},
  {2500,6000,5000,2500,0,0,2500,0,1,1,1,32,1,1,0,32,32,1,1,0,32},
  {2500,5000,5000,2500,5000,0,2500,0,1,1,1,32,1,1,1,32,32,1,1,1,32},
  {2500,6000,5000,2500,0,2500,2500,0,1,1,1,16,1,1,0,16,16,1,1,0,16},
  {2500,6000,5000,2500,0,2500,2500,0,1,1,1,32,1,1,0,32,32,1,1,0,32},
  {2500,6000,5000,2500,0,0,2500,6000,1,1,1,32,1,1,0,32,32,1,1,0,32},
  {2500,6000,5000,2500,0,2500,2500,6000,1,1,1,32,1,1,0,32,32,1,1,0,32},
};

/* Space shared table (34 entries) */
static const pic8_space_entry_t g_spaceTable[] = {
  {0x0000,0x0400,0x2000,0x2007,1,0x2000,4,0x2006,0x2100,0x2180,0x03FF,1,0x03FF,1,0},
  {0x0000,0x0400,0x2000,0x2007,1,0x2000,4,0x2006,0x2100,0x2180,0x2008,2,0x2008,2,1},
  {0x0000,0x0800,0x2000,0x2007,1,0x2000,4,0x2006,0x2100,0x2200,0x2008,1,0x2008,1,2},
  {0x0000,0x0100,0x0000,0x0FFF,1,0x0100,4,0x0000,0x0000,0x0000,0x00FF,1,0x00FF,1,3},
  {0x0000,0x0200,0x0000,0x0FFF,1,0x0200,4,0x0000,0x0000,0x0000,0x01FF,1,0x01FF,1,4},
  {0x0000,0x0200,0x0000,0x0FFF,1,0x0400,4,0x0000,0x0000,0x0000,0x03FF,1,0x03FF,1,5},
  {0x0000,0x0400,0x2000,0x2007,1,0x2000,4,0x2006,0x0000,0x0000,0x2008,1,0x2008,1,6},
  {0x0000,0x0800,0x2000,0x2007,1,0x2000,4,0x2006,0x0000,0x0000,0x2008,1,0x2008,1,6},
  {0x0000,0x0200,0x0000,0x0FFF,1,0x0440,4,0x0000,0x0000,0x0000,0x03FF,1,0x03FF,1,5},
  {0x0000,0x0100,0x2000,0x2007,1,0x2000,4,0x2006,0x0000,0x0000,0x2008,1,0x2008,1,6},
  {0x0000,0x0200,0x2000,0x2007,1,0x2000,4,0x2006,0x0000,0x0000,0x2008,1,0x2008,1,6},
  {0x0000,0x0400,0x2000,0x2007,1,0x2000,4,0x2006,0x0000,0x0000,0x2008,2,0x2008,2,7},
  {0x0000,0x0200,0x0000,0x0FFF,1,0x0640,4,0x0000,0x0000,0x0000,0x05FF,1,0x05FF,1,8},
  {0x0000,0x0400,0x0000,0x0FFF,1,0x0400,4,0x0000,0x0000,0x0000,0x03FF,1,0x03FF,1,5},
  {0x0000,0x0400,0x2000,0x2007,1,0x2000,4,0x2006,0x0000,0x0000,0x03FF,1,0x03FF,1,9},
  {0x0000,0x0800,0x2000,0x2007,1,0x2000,4,0x2006,0x0000,0x0000,0x07FF,1,0x07FF,1,10},
  {0x0000,0x0800,0x2000,0x2007,1,0x2000,4,0x2006,0x0000,0x0000,0x2008,1,0x2008,1,11},
  {0x0000,0x0200,0x0000,0x0FFF,1,0x0200,4,0x0000,0x0000,0x0000,0x2008,1,0x2008,1,12},
  {0x0000,0x0200,0x2000,0x2007,1,0x2000,4,0x2006,0x0000,0x0000,0x2008,1,0x2008,1,11},
  {0x0000,0x0200,0x0000,0x0FFF,1,0x0400,4,0x0000,0x0000,0x0000,0x2008,1,0x2008,1,12},
  {0x0000,0x0200,0x0000,0x0FFF,1,0x0800,4,0x0000,0x0000,0x0000,0x2008,1,0x2008,1,12},
  {0x0000,0x0800,0x0000,0x0FFF,1,0x0800,4,0x0000,0x0000,0x0000,0x2008,1,0x2008,1,12},
  {0x0000,0x0400,0x2000,0x2007,1,0x2000,4,0x2006,0x0000,0x0000,0x2008,1,0x2008,1,11},
  {0x0000,0x0200,0x2000,0x2007,1,0x2000,4,0x2006,0x2100,0x2140,0x2008,1,0x2008,1,11},
  {0x0000,0x0400,0x2000,0x2007,1,0x2000,4,0x2006,0x2100,0x2140,0x2008,1,0x2008,1,11},
  {0x0000,0x0800,0x8000,0x8007,2,0x8000,4,0x8006,0xF000,0xF100,0x2008,1,0x2008,1,13},
  {0x0000,0x0800,0x8000,0x8007,2,0x8000,4,0x8006,0xF000,0xF100,0x2008,1,0x2008,1,14},
  {0x0000,0x0800,0x8000,0x8007,2,0x8000,4,0x8006,0x0000,0x0000,0x2008,1,0x2008,1,13},
  {0x0000,0x0400,0x8000,0x8007,2,0x8000,4,0x8006,0x0000,0x0000,0x2008,1,0x2008,1,13},
  {0x0000,0x0800,0x8000,0x8007,3,0x8000,4,0x8006,0x0000,0x0000,0x2008,1,0x2008,1,15},
  {0x0000,0x0400,0x8000,0x8007,2,0x8000,4,0x8006,0x0000,0x0000,0x2008,1,0x2008,1,14},
  {0x0000,0x0800,0x8000,0x8007,2,0x8000,4,0x8006,0x0000,0x0000,0x2008,1,0x2008,1,14},
  {0x0000,0x0800,0x8000,0x8007,2,0x8000,4,0x8006,0x0000,0x0000,0x2008,1,0x2008,1,16},
  {0x0000,0x0800,0x8000,0x8007,2,0x8000,4,0x8006,0x0000,0x0000,0x2008,1,0x2008,1,17},
};

/* DCR shared table (42 entries) */
static const pic8_dcr_group_t g_dcrTable[] = {
  { 1, {0,0,0}, { { "CONFIG", 0x2007, 0x31FF, 0x01FF, 0x31FF, 0x31FF, 0x3E00, 1, 14 }, {0}, {0}, {0} } },
  { 1, {0,0,0}, { { "CONFIG", 0x2007, 0x1FFF, 0x1FFF, 0x3FFF, 0x3FFF, 0xE000, 1, 14 }, {0}, {0}, {0} } },
  { 1, {0,0,0}, { { "CONFIG", 0x2007, 0x0FFF, 0x0FFF, 0x3FFF, 0x3FFF, 0x3000, 1, 14 }, {0}, {0}, {0} } },
  { 1, {0,0,0}, { { "CONFIG", 0x0FFF, 0x001C, 0x001C, 0x0FFF, 0x0FFF, 0x0FE3, 1, 12 }, {0}, {0}, {0} } },
  { 1, {0,0,0}, { { "CONFIG", 0x0FFF, 0x001F, 0x001F, 0x0FFF, 0x0FFF, 0x0000, 1, 12 }, {0}, {0}, {0} } },
  { 1, {0,0,0}, { { "CONFIG", 0x0FFF, 0x001F, 0x001F, 0x0FFF, 0x0FFF, 0x0FE3, 1, 12 }, {0}, {0}, {0} } },
  { 1, {0,0,0}, { { "CONFIG", 0x0FFF, 0x003F, 0x003F, 0x0FFF, 0x0FFF, 0x0000, 1, 12 }, {0}, {0}, {0} } },
  { 1, {0,0,0}, { { "CONFIG", 0x2007, 0x03FF, 0x03FF, 0x3FFF, 0x3FFF, 0x3000, 1, 14 }, {0}, {0}, {0} } },
  { 1, {0,0,0}, { { "CONFIG", 0x0FFF, 0x007F, 0x007F, 0x0FFF, 0x0FFF, 0x0000, 1, 12 }, {0}, {0}, {0} } },
  { 1, {0,0,0}, { { "CONFIG", 0x2007, 0x1FFF, 0x1FFF, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, {0}, {0}, {0} } },
  { 1, {0,0,0}, { { "CONFIG", 0x2007, 0x3F79, 0x3F79, 0x3FFF, 0x3FFF, 0x3000, 1, 14 }, {0}, {0}, {0} } },
  { 1, {0,0,0}, { { "CONFIG", 0x0FFF, 0x07FF, 0x007F, 0x0DFF, 0x0DFF, 0x0000, 1, 12 }, {0}, {0}, {0} } },
  { 1, {0,0,0}, { { "CONFIG", 0x2007, 0x31FF, 0x01FF, 0x3FFF, 0x31FF, 0x3E00, 1, 14 }, {0}, {0}, {0} } },
  { 1, {0,0,0}, { { "CONFIG", 0x0FFF, 0x001F, 0x001F, 0x0FFF, 0x0FFF, 0x0FE0, 1, 12 }, {0}, {0}, {0} } },
  { 1, {0,0,0}, { { "CONFIG", 0x2007, 0x3FFF, 0x3FFF, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, {0}, {0}, {0} } },
  { 1, {0,0,0}, { { "CONFIG1", 0x2007, 0x3F7F, 0x3F7F, 0x3FFF, 0x3FFF, 0x0080, 1, 14 }, {0}, {0}, {0} } },
  { 1, {0,0,0}, { { "CONFIG1", 0x2007, 0x3FFF, 0x3FFF, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, {0}, {0}, {0} } },
  { 1, {0,0,0}, { { "CONFIG", 0x0FFF, 0x0FFF, 0x0FFF, 0x0FFF, 0x0FFF, 0x0000, 1, 12 }, {0}, {0}, {0} } },
  { 1, {0,0,0}, { { "CONFIG", 0x0FFF, 0x000F, 0x000F, 0x0FFF, 0x0FFF, 0x0FF0, 1, 12 }, {0}, {0}, {0} } },
  { 1, {0,0,0}, { { "CONFIG1", 0x2007, 0x3F3F, 0x3F3F, 0x3FFF, 0x3FFF, 0x0080, 1, 14 }, {0}, {0}, {0} } },
  { 1, {0,0,0}, { { "CONFIG", 0x2007, 0x3F7F, 0x3F7F, 0x3FFF, 0x3FFF, 0x0080, 1, 14 }, {0}, {0}, {0} } },
  { 1, {0,0,0}, { { "CONFIG1", 0x2007, 0x001F, 0x001F, 0x3FFF, 0x3FFF, 0x3FE0, 1, 14 }, {0}, {0}, {0} } },
  { 1, {0,0,0}, { { "CONFIG1", 0x2007, 0x3F3F, 0x3F3F, 0x3FFF, 0x3FFF, 0x00C0, 1, 14 }, {0}, {0}, {0} } },
  { 1, {0,0,0}, { { "CONFIG1", 0x2007, 0x007F, 0x007F, 0x3FFF, 0x3FFF, 0x3F80, 1, 14 }, {0}, {0}, {0} } },
  { 1, {0,0,0}, { { "CONFIG", 0x2007, 0x005F, 0x005F, 0x3FFF, 0x3FFF, 0x3FA0, 1, 14 }, {0}, {0}, {0} } },
  { 2, {0,0,0}, { { "CONFIG1", 0x8007, 0x3FFF, 0x3FFF, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, { "CONFIG2", 0x8008, 0x3733, 0x3733, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, {0}, {0} } },
  { 2, {0,0,0}, { { "CONFIG1", 0x8007, 0x3FFF, 0x3FFF, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, { "CONFIG2", 0x8008, 0x3703, 0x3703, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, {0}, {0} } },
  { 2, {0,0,0}, { { "CONFIG1", 0x8007, 0x3FFF, 0x3FFF, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, { "CONFIG2", 0x8008, 0x3713, 0x3713, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, {0}, {0} } },
  { 2, {0,0,0}, { { "CONFIG1", 0x8007, 0x0EFB, 0x0EFB, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, { "CONFIG2", 0x8008, 0x3E03, 0x3E03, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, {0}, {0} } },
  { 2, {0,0,0}, { { "CONFIG1", 0x8007, 0x0EFB, 0x0EFB, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, { "CONFIG2", 0x8008, 0x2E03, 0x2E03, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, {0}, {0} } },
  { 2, {0,0,0}, { { "CONFIG1", 0x8007, 0x3EFF, 0x3EFF, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, { "CONFIG2", 0x8008, 0x3E03, 0x3E03, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, {0}, {0} } },
  { 2, {0,0,0}, { { "CONFIG1", 0x8007, 0x3EFF, 0x3EFF, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, { "CONFIG2", 0x8008, 0x3FF3, 0x3FF3, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, {0}, {0} } },
  { 2, {0,0,0}, { { "CONFIG1", 0x8007, 0x3FFF, 0x3FFF, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, { "CONFIG2", 0x8008, 0x3F23, 0x3F23, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, {0}, {0} } },
  { 2, {0,0,0}, { { "CONFIG1", 0x8007, 0x3FFF, 0x3FFF, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, { "CONFIG2", 0x8008, 0x3F03, 0x3F03, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, {0}, {0} } },
  { 2, {0,0,0}, { { "CONFIG1", 0x8007, 0x0EFB, 0x0EFB, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, { "CONFIG2", 0x8008, 0x3F87, 0x3F87, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, {0}, {0} } },
  { 2, {0,0,0}, { { "CONFIG1", 0x8007, 0x3EFF, 0x3EFF, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, { "CONFIG2", 0x8008, 0x3F87, 0x3F87, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, {0}, {0} } },
  { 3, {0,0,0}, { { "CONFIG1", 0x8007, 0x0EE3, 0x0EE3, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, { "CONFIG2", 0x8008, 0x3F83, 0x3F83, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, { "CONFIG3", 0x8009, 0x3F7F, 0x3F7F, 0x3FFF, 0x3FFF, 0x0000, 0, 14 }, {0} } },
  { 3, {0,0,0}, { { "CONFIG1", 0x8007, 0x0EE3, 0x0EE3, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, { "CONFIG2", 0x8008, 0x3F87, 0x3F87, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, { "CONFIG3", 0x8009, 0x3F7F, 0x3F7F, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, {0} } },
  { 3, {0,0,0}, { { "CONFIG1", 0x8007, 0x3EE7, 0x3EE7, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, { "CONFIG2", 0x8008, 0x3F87, 0x3F87, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, { "CONFIG3", 0x8009, 0x3F7F, 0x3F7F, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, {0} } },
  { 2, {0,0,0}, { { "CONFIG1", 0x8007, 0x0EFB, 0x0EFB, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, { "CONFIG2", 0x8008, 0x3F03, 0x3F03, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, {0}, {0} } },
  { 2, {0,0,0}, { { "CONFIG1", 0x8007, 0x3EFF, 0x3EFF, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, { "CONFIG2", 0x8008, 0x3E13, 0x3E13, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, {0}, {0} } },
  { 2, {0,0,0}, { { "CONFIG1", 0x8007, 0x0EFB, 0x0EFB, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, { "CONFIG2", 0x8008, 0x3F07, 0x3F07, 0x3FFF, 0x3FFF, 0x0000, 1, 14 }, {0}, {0} } },
};

/* Variant sub table (18 entries) */
static const pic8_sub_entry_t g_subTable[] = {
  {0x0000, 0x03FF, 0x0000, 0x0000, 0x0000, 0x03FF, 0x0000, 0x2004, 0x2005, 0, 0},
  {0x0000, 0x2008, 0x0000, 0x0000, 0x0000, 0x2008, 0x2009, 0x2004, 0x2005, 0, 0},
  {0x0000, 0x2008, 0x0000, 0x0000, 0x0000, 0x2008, 0x0000, 0x2004, 0x2005, 0, 0},
  {0x0FFF, 0x00FF, 0x0000, 0x0000, 0x0000, 0x00FF, 0x0000, 0x0000, 0x0000, 0, 0},
  {0x0FFF, 0x01FF, 0x0000, 0x0000, 0x0000, 0x01FF, 0x0000, 0x0000, 0x0000, 0, 0},
  {0x0FFF, 0x03FF, 0x0000, 0x0000, 0x0000, 0x03FF, 0x0000, 0x0000, 0x0000, 0, 0},
  {0x0000, 0x2008, 0x0000, 0x0000, 0x0000, 0x2008, 0x0000, 0x2004, 0x2005, 4, 0},
  {0x0000, 0x2008, 0x0000, 0x0000, 0x0000, 0x2008, 0x2009, 0x2004, 0x2005, 4, 0},
  {0x0FFF, 0x05FF, 0x0000, 0x0000, 0x0000, 0x05FF, 0x0000, 0x0000, 0x0000, 0, 0},
  {0x0000, 0x03FF, 0x0000, 0x0000, 0x0000, 0x03FF, 0x0000, 0x0000, 0x0000, 0, 0},
  {0x0000, 0x07FF, 0x0000, 0x0000, 0x0000, 0x07FF, 0x0000, 0x0000, 0x0000, 0, 0},
  {0x0000, 0x2008, 0x0000, 0x0000, 0x0000, 0x2008, 0x0000, 0x0000, 0x0000, 0, 0},
  {0x0FFF, 0x2008, 0x0000, 0x0000, 0x0000, 0x2008, 0x0000, 0x0000, 0x0000, 0, 0},
  {0x0000, 0x2008, 0x8008, 0x0000, 0x0000, 0x2008, 0x0000, 0x8004, 0x8005, 8, 0},
  {0x0000, 0x2008, 0x8008, 0x0000, 0x0000, 0x2008, 0x0000, 0x8004, 0x8005, 4, 0},
  {0x0000, 0x2008, 0x8008, 0x8009, 0x0000, 0x2008, 0x0000, 0x8004, 0x8005, 8, 0},
  {0x0000, 0x2008, 0x8008, 0x0000, 0x0000, 0x2008, 0x0000, 0x8004, 0x8006, 8, 0},
  {0x0000, 0x2008, 0x8008, 0x0000, 0x0000, 0x2008, 0x0000, 0x800D, 0x800F, 8, 0},
};

static const pic8_pk2_supplement_t g_pk2SupplementTable[] = {
#include "picDeviceConst_pk2_supplement.inc"
};

/* Device table (285 entries) */
static const pic8_device_index_t g_deviceTable[] = {
  { "PIC12F629", 0, 0, 0, 0, 1, 0, 1, 14, 0x3FE0, 0x0F80 },
  { "PIC12F675", 0, 0, 0, 0, 1, 0, 1, 14, 0x3FE0, 0x0FC0 },
  { "PIC12F635", 1, 1, 1, 1, 1, 0, 1, 14, 0x3FE0, 0x0FA0 },
  { "PIC12F683", 1, 1, 2, 2, 1, 0, 1, 14, 0x3FE0, 0x0460 },
  { "PIC10F200", 2, 2, 3, 3, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC10F202", 2, 2, 4, 3, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC10F204", 2, 2, 3, 3, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC10F206", 2, 2, 4, 3, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC12F508", 3, 2, 4, 4, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC12F509", 3, 2, 5, 4, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC10F220", 3, 2, 3, 5, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC10F222", 3, 2, 4, 5, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC12F510", 3, 2, 5, 6, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC12F609", 4, 3, 6, 7, 1, 0, 1, 14, 0x3FE0, 0x2240 },
  { "PIC12F615", 4, 3, 6, 7, 1, 0, 1, 14, 0x3FE0, 0x2180 },
  { "PIC12F617", 4, 4, 7, 2, 1, 0, 1, 14, 0x3FE0, 0x1360 },
  { "PIC12HV609", 5, 5, 6, 7, 1, 0, 1, 14, 0x3FE0, 0x2280 },
  { "PIC12HV615", 5, 5, 6, 7, 1, 0, 1, 14, 0x3FE0, 0x21A0 },
  { "PIC12F519", 3, 6, 8, 8, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC10F320", 6, 7, 9, 9, 1, 0, 1, 14, 0x3FE0, 0x29A0 },
  { "PIC10F322", 6, 7, 10, 9, 1, 0, 1, 14, 0x3FE0, 0x2980 },
  { "PIC10LF320", 7, 7, 9, 9, 1, 0, 1, 14, 0x3FE0, 0x29E0 },
  { "PIC10LF322", 7, 7, 10, 9, 1, 0, 1, 14, 0x3FE0, 0x29C0 },
  { "PIC12F752", 8, 8, 11, 10, 1, 0, 1, 14, 0x3FE0, 0x1500 },
  { "PIC12HV752", 8, 8, 11, 10, 1, 0, 1, 14, 0x3FE0, 0x1520 },
  { "PIC12F529T39A", 9, 9, 12, 11, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC12F529T48A", 9, 10, 12, 11, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PICRF675F", 10, 0, 0, 12, 1, 0, 1, 14, 0x3FE0, 0x0FC0 },
  { "PICRF675H", 10, 0, 0, 12, 1, 0, 1, 14, 0x3FE0, 0x0FC0 },
  { "PICRF675K", 10, 0, 0, 12, 1, 0, 1, 14, 0x3FE0, 0x0FC0 },
  { "PIC12C508", 11, 11, 4, 13, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC12C508A", 11, 11, 4, 13, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC12C509", 11, 11, 13, 13, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC12C509A", 11, 11, 13, 13, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC12C671", 11, 12, 14, 14, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC12C672", 11, 12, 15, 14, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC12CE518", 11, 11, 4, 13, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC12CE519", 11, 11, 13, 13, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC12CE673", 11, 11, 14, 14, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC12CE674", 11, 11, 15, 14, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC12CR509A", 11, 13, 13, 13, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC16C432", 11, 12, 16, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C433", 11, 12, 15, 16, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C505", 11, 11, 13, 17, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC16C54", 12, 11, 17, 18, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC16C54C", 12, 11, 17, 17, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC16C55", 12, 11, 17, 18, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC16C554", 13, 12, 18, 19, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C557", 13, 12, 16, 19, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C558", 13, 12, 16, 19, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C55A", 12, 11, 17, 17, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC16C56", 12, 11, 19, 18, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC16C56A", 12, 11, 19, 17, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC16C57", 12, 11, 20, 18, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC16C57C", 12, 11, 20, 17, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC16C58A", 12, 11, 21, 18, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC16C58B", 12, 11, 20, 17, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC16C620", 14, 12, 18, 20, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C620A", 11, 12, 18, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C621", 14, 12, 22, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C621A", 11, 12, 22, 20, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C622", 14, 12, 16, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C622A", 11, 12, 16, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C62A", 13, 12, 16, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C62B", 13, 12, 16, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C63", 13, 12, 16, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C63A", 13, 12, 16, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C642", 13, 12, 16, 14, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C64A", 13, 12, 16, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C65A", 13, 12, 16, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C65B", 13, 12, 16, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C66", 13, 12, 16, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C662", 13, 12, 16, 16, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C67", 13, 12, 16, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C71", 13, 12, 22, 21, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C710", 13, 12, 18, 16, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C711", 13, 12, 22, 16, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C712", 15, 12, 22, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C715", 13, 12, 16, 16, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C716", 15, 12, 16, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C717", 15, 12, 16, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C72", 13, 12, 16, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C72A", 13, 12, 16, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C73A", 13, 12, 16, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C73B", 13, 12, 16, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C745", 16, 12, 16, 19, 1, 0, 1, 14, 0x3FE0, 0x0B60 },
  { "PIC16C74A", 13, 12, 16, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C74B", 13, 12, 16, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C76", 13, 12, 16, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C765", 16, 12, 16, 19, 1, 0, 1, 14, 0x3FE0, 0x0B80 },
  { "PIC16C77", 13, 12, 16, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C770", 15, 12, 16, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C771", 15, 12, 16, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C773", 13, 12, 16, 20, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C774", 13, 12, 16, 20, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C781", 15, 12, 22, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C782", 15, 12, 16, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C923", 13, 12, 16, 22, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C924", 13, 12, 16, 22, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16C925", 13, 12, 16, 23, 1, 0, 1, 14, 0x3FE0, 0x0140 },
  { "PIC16C926", 13, 12, 16, 23, 1, 0, 1, 14, 0x3FE0, 0x0100 },
  { "PIC16CE623", 11, 12, 18, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16CE624", 11, 12, 22, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16CE625", 11, 12, 16, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16CR54", 12, 13, 17, 18, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC16CR54A", 12, 13, 17, 17, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC16CR54C", 12, 13, 17, 17, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC16CR56A", 12, 13, 19, 17, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC16CR57C", 12, 13, 20, 17, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC16CR58B", 12, 13, 20, 17, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC16CR62", 12, 13, 16, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16CR620A", 11, 13, 18, 16, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16CR63", 12, 13, 16, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16CR64", 13, 13, 16, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16CR65", 13, 13, 16, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16CR72", 12, 13, 16, 15, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16CR73", 17, 13, 16, 24, 1, 0, 1, 14, 0x3FE0, 0x0C80 },
  { "PIC16CR74", 17, 13, 16, 24, 1, 0, 1, 14, 0x3FE0, 0x0CC0 },
  { "PIC16CR76", 17, 13, 16, 24, 1, 0, 1, 14, 0x3FE0, 0x0C00 },
  { "PIC16CR77", 17, 13, 16, 24, 1, 0, 1, 14, 0x3FE0, 0x0C40 },
  { "PIC16CR83", 18, 13, 23, 16, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16CR84", 18, 13, 24, 16, 1, 0, 1, 14, 0x3FFF, 0x0000 },
  { "PIC16CR926", 17, 13, 16, 23, 1, 0, 1, 14, 0x3FE0, 0x2100 },
  { "PICRF509AF", 11, 11, 13, 13, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PICRF509AG", 11, 11, 13, 13, 0, 1, 1, 12, 0x3FFF, 0x0000 },
  { "PIC16F1933", 19, 14, 25, 25, 2, 0, 1, 14, 0x3FE0, 0x2300 },
  { "PIC16F1934", 19, 14, 25, 25, 2, 0, 1, 14, 0x3FE0, 0x2340 },
  { "PIC16F1936", 19, 14, 25, 25, 2, 0, 1, 14, 0x3FE0, 0x2360 },
  { "PIC16F1937", 19, 14, 25, 25, 2, 0, 1, 14, 0x3FE0, 0x2380 },
  { "PIC16F1938", 19, 15, 25, 25, 2, 0, 1, 14, 0x3FE0, 0x23A0 },
  { "PIC16F1939", 19, 15, 25, 25, 2, 0, 1, 14, 0x3FE0, 0x23C0 },
  { "PIC16LF1933", 20, 14, 25, 26, 2, 0, 1, 14, 0x3FE0, 0x2400 },
  { "PIC16LF1934", 20, 14, 25, 26, 2, 0, 1, 14, 0x3FE0, 0x2440 },
  { "PIC16LF1936", 20, 14, 25, 26, 2, 0, 1, 14, 0x3FE0, 0x2460 },
  { "PIC16LF1937", 20, 14, 25, 26, 2, 0, 1, 14, 0x3FE0, 0x2480 },
  { "PIC16LF1938", 20, 15, 25, 26, 2, 0, 1, 14, 0x3FE0, 0x24A0 },
  { "PIC16LF1939", 20, 15, 25, 26, 2, 0, 1, 14, 0x3FE0, 0x24C0 },
  { "PIC12F1822", 19, 16, 26, 27, 2, 0, 1, 14, 0x3FE0, 0x2700 },
  { "PIC12F1840", 21, 15, 26, 27, 2, 0, 1, 14, 0x3FE0, 0x1B80 },
  { "PIC16F1823", 19, 16, 26, 27, 2, 0, 1, 14, 0x3FE0, 0x2720 },
  { "PIC16F1824", 19, 15, 26, 27, 2, 0, 1, 14, 0x3FE0, 0x2740 },
  { "PIC16F1825", 19, 15, 26, 27, 2, 0, 1, 14, 0x3FE0, 0x2760 },
  { "PIC16F1828", 19, 15, 26, 27, 2, 0, 1, 14, 0x3FE0, 0x27C0 },
  { "PIC16F1829", 19, 15, 26, 27, 2, 0, 1, 14, 0x3FE0, 0x27E0 },
  { "PIC12LF1822", 20, 16, 26, 27, 2, 0, 1, 14, 0x3FE0, 0x2800 },
  { "PIC12LF1840", 20, 15, 26, 27, 2, 0, 1, 14, 0x3FE0, 0x1BC0 },
  { "PIC16LF1823", 20, 16, 26, 27, 2, 0, 1, 14, 0x3FE0, 0x2820 },
  { "PIC16LF1824", 20, 15, 26, 27, 2, 0, 1, 14, 0x3FE0, 0x2840 },
  { "PIC16LF1825", 20, 15, 26, 27, 2, 0, 1, 14, 0x3FE0, 0x2860 },
  { "PIC16LF1828", 20, 15, 26, 27, 2, 0, 1, 14, 0x3FE0, 0x28C0 },
  { "PIC16LF1829", 20, 15, 26, 27, 2, 0, 1, 14, 0x3FE0, 0x28E0 },
  { "PIC16F1826", 19, 14, 25, 27, 2, 0, 1, 14, 0x3FE0, 0x2780 },
  { "PIC16F1827", 19, 14, 25, 27, 2, 0, 1, 14, 0x3FE0, 0x27A0 },
  { "PIC16F1847", 19, 15, 25, 27, 2, 0, 1, 14, 0x3FE0, 0x1480 },
  { "PIC16LF1826", 20, 14, 25, 26, 2, 0, 1, 14, 0x3FE0, 0x2880 },
  { "PIC16LF1827", 20, 14, 25, 26, 2, 0, 1, 14, 0x3FE0, 0x28A0 },
  { "PIC16LF1847", 20, 15, 25, 27, 2, 0, 1, 14, 0x3FE0, 0x14A0 },
  { "PIC16LF1902", 22, 17, 27, 28, 2, 0, 1, 14, 0x3FE0, 0x2C20 },
  { "PIC16LF1903", 22, 17, 27, 28, 2, 0, 1, 14, 0x3FE0, 0x2C00 },
  { "PIC16LF1904", 22, 17, 27, 28, 2, 0, 1, 14, 0x3FE0, 0x2C80 },
  { "PIC16LF1906", 22, 17, 27, 28, 2, 0, 1, 14, 0x3FE0, 0x2C60 },
  { "PIC16LF1907", 22, 17, 27, 28, 2, 0, 1, 14, 0x3FE0, 0x2C40 },
  { "PIC12F1501", 21, 7, 28, 29, 2, 0, 1, 14, 0x3FE0, 0x2CC0 },
  { "PIC16F1503", 21, 7, 27, 29, 2, 0, 1, 14, 0x3FE0, 0x2CE0 },
  { "PIC16F1507", 21, 7, 27, 29, 2, 0, 1, 14, 0x3FE0, 0x2D00 },
  { "PIC16F1508", 21, 17, 27, 30, 2, 0, 1, 14, 0x3FE0, 0x2D20 },
  { "PIC16F1509", 21, 17, 27, 30, 2, 0, 1, 14, 0x3FE0, 0x2D40 },
  { "PIC12LF1501", 20, 7, 28, 29, 2, 0, 1, 14, 0x3FE0, 0x2D80 },
  { "PIC16LF1503", 20, 7, 27, 29, 2, 0, 1, 14, 0x3FE0, 0x2DA0 },
  { "PIC16LF1507", 20, 7, 27, 29, 2, 0, 1, 14, 0x3FE0, 0x2DC0 },
  { "PIC16LF1508", 20, 17, 27, 30, 2, 0, 1, 14, 0x3FE0, 0x2DE0 },
  { "PIC16LF1509", 20, 17, 27, 30, 2, 0, 1, 14, 0x3FE0, 0x2E00 },
  { "PIC16F1454", 21, 17, 27, 31, 2, 0, 1, 14, 0x3FFF, 0x3020 },
  { "PIC16F1455", 21, 17, 27, 31, 2, 0, 1, 14, 0x3FFF, 0x3021 },
  { "PIC16F1459", 21, 17, 27, 31, 2, 0, 1, 14, 0x3FFF, 0x3023 },
  { "PIC16LF1454", 20, 17, 27, 31, 2, 0, 1, 14, 0x3FFF, 0x3024 },
  { "PIC16LF1455", 20, 17, 27, 31, 2, 0, 1, 14, 0x3FFF, 0x3025 },
  { "PIC16LF1459", 20, 17, 27, 31, 2, 0, 1, 14, 0x3FFF, 0x3027 },
  { "PIC16F1782", 21, 18, 26, 32, 2, 0, 1, 14, 0x3FE0, 0x2A00 },
  { "PIC16F1783", 21, 18, 26, 32, 2, 0, 1, 14, 0x3FE0, 0x2A20 },
  { "PIC16F1784", 21, 18, 26, 32, 2, 0, 1, 14, 0x3FE0, 0x2A40 },
  { "PIC16F1786", 21, 18, 26, 32, 2, 0, 1, 14, 0x3FE0, 0x2A60 },
  { "PIC16F1787", 21, 18, 26, 32, 2, 0, 1, 14, 0x3FE0, 0x2A80 },
  { "PIC16LF1782", 20, 18, 26, 33, 2, 0, 1, 14, 0x3FE0, 0x2AA0 },
  { "PIC16LF1783", 20, 18, 26, 33, 2, 0, 1, 14, 0x3FE0, 0x2AC0 },
  { "PIC16LF1784", 20, 18, 26, 33, 2, 0, 1, 14, 0x3FE0, 0x2AE0 },
  { "PIC16LF1786", 20, 18, 26, 33, 2, 0, 1, 14, 0x3FE0, 0x2B00 },
  { "PIC16LF1787", 20, 18, 26, 33, 2, 0, 1, 14, 0x3FE0, 0x2B20 },
  { "PIC16F1788", 21, 18, 26, 32, 2, 0, 1, 14, 0x3FFF, 0x302B },
  { "PIC16F1789", 21, 18, 26, 32, 2, 0, 1, 14, 0x3FFF, 0x302A },
  { "PIC16LF1788", 20, 18, 26, 33, 2, 0, 1, 14, 0x3FFF, 0x302D },
  { "PIC16LF1789", 20, 18, 26, 33, 2, 0, 1, 14, 0x3FFF, 0x302C },
  { "PIC16F1703", 21, 19, 27, 34, 2, 0, 1, 14, 0x3FFF, 0x3061 },
  { "PIC16F1704", 21, 20, 27, 35, 2, 0, 1, 14, 0x3FFF, 0x3043 },
  { "PIC16F1705", 21, 20, 27, 35, 2, 0, 1, 14, 0x3FFF, 0x3055 },
  { "PIC16F1707", 21, 19, 27, 34, 2, 0, 1, 14, 0x3FFF, 0x3060 },
  { "PIC16F1708", 21, 20, 27, 35, 2, 0, 1, 14, 0x3FFF, 0x3042 },
  { "PIC16F1709", 21, 20, 27, 35, 2, 0, 1, 14, 0x3FFF, 0x3054 },
  { "PIC16F1713", 21, 17, 27, 35, 2, 0, 1, 14, 0x3FFF, 0x3049 },
  { "PIC16F1716", 21, 17, 27, 35, 2, 0, 1, 14, 0x3FFF, 0x3048 },
  { "PIC16F1717", 21, 17, 27, 35, 2, 0, 1, 14, 0x3FFF, 0x305C },
  { "PIC16F1718", 21, 17, 27, 35, 2, 0, 1, 14, 0x3FFF, 0x305B },
  { "PIC16F1719", 21, 17, 27, 35, 2, 0, 1, 14, 0x3FFF, 0x305A },
  { "PIC16LF1703", 20, 19, 27, 34, 2, 0, 1, 14, 0x3FFF, 0x3063 },
  { "PIC16LF1704", 20, 20, 27, 35, 2, 0, 1, 14, 0x3FFF, 0x3045 },
  { "PIC16LF1705", 20, 20, 27, 35, 2, 0, 1, 14, 0x3FFF, 0x3057 },
  { "PIC16LF1707", 20, 19, 27, 34, 2, 0, 1, 14, 0x3FFF, 0x3062 },
  { "PIC16LF1708", 20, 20, 27, 35, 2, 0, 1, 14, 0x3FFF, 0x3044 },
  { "PIC16LF1709", 20, 20, 27, 35, 2, 0, 1, 14, 0x3FFF, 0x3056 },
  { "PIC16LF1713", 20, 17, 27, 35, 2, 0, 1, 14, 0x3FFF, 0x304B },
  { "PIC16LF1716", 20, 17, 27, 35, 2, 0, 1, 14, 0x3FFF, 0x304A },
  { "PIC16LF1717", 20, 17, 27, 35, 2, 0, 1, 14, 0x3FFF, 0x305F },
  { "PIC16LF1718", 20, 17, 27, 35, 2, 0, 1, 14, 0x3FFF, 0x305E },
  { "PIC16LF1719", 20, 17, 27, 35, 2, 0, 1, 14, 0x3FFF, 0x305D },
  { "PIC12F1612", 21, 7, 29, 36, 2, 0, 1, 14, 0x3FFF, 0x3058 },
  { "PIC12LF1612", 20, 7, 29, 36, 2, 0, 1, 14, 0x3FFF, 0x3059 },
  { "PIC16F1613", 21, 7, 29, 36, 2, 0, 1, 14, 0x3FFF, 0x304C },
  { "PIC16F1614", 21, 17, 29, 37, 2, 0, 1, 14, 0x3FFF, 0x3078 },
  { "PIC16F1615", 21, 17, 29, 38, 2, 0, 1, 14, 0x3FFF, 0x307C },
  { "PIC16F1618", 21, 17, 29, 37, 2, 0, 1, 14, 0x3FFF, 0x3079 },
  { "PIC16F1619", 21, 17, 29, 38, 2, 0, 1, 14, 0x3FFF, 0x307D },
  { "PIC16LF1613", 20, 7, 29, 36, 2, 0, 1, 14, 0x3FFF, 0x304D },
  { "PIC16LF1614", 20, 17, 29, 37, 2, 0, 1, 14, 0x3FFF, 0x307A },
  { "PIC16LF1615", 23, 17, 29, 38, 2, 0, 1, 14, 0x3FFF, 0x307E },
  { "PIC16LF1618", 20, 17, 29, 37, 2, 0, 1, 14, 0x3FFF, 0x307B },
  { "PIC16LF1619", 20, 17, 29, 38, 2, 0, 1, 14, 0x3FFF, 0x307F },
  { "PIC12F1571", 21, 7, 30, 39, 2, 0, 1, 14, 0x3FFF, 0x3051 },
  { "PIC12F1572", 21, 7, 31, 39, 2, 0, 1, 14, 0x3FFF, 0x3050 },
  { "PIC12LF1552", 20, 7, 31, 29, 2, 0, 1, 14, 0x3FE0, 0x2BC0 },
  { "PIC12LF1571", 24, 7, 30, 39, 2, 0, 1, 14, 0x3FFF, 0x3053 },
  { "PIC12LF1572", 24, 7, 31, 39, 2, 0, 1, 14, 0x3FFF, 0x3052 },
  { "PIC12LF1840T39A", 20, 15, 26, 27, 2, 0, 1, 14, 0x3FE0, 0x1BC0 },
  { "PIC12LF1840T48A", 20, 15, 26, 27, 2, 0, 1, 14, 0x3FE0, 0x1BC0 },
  { "PIC16F1512", 21, 17, 32, 40, 2, 0, 1, 14, 0x3FE0, 0x1700 },
  { "PIC16F1513", 21, 17, 32, 40, 2, 0, 1, 14, 0x3FE0, 0x1640 },
  { "PIC16F1516", 21, 17, 27, 40, 2, 0, 1, 14, 0x3FE0, 0x1680 },
  { "PIC16F1517", 21, 17, 27, 40, 2, 0, 1, 14, 0x3FE0, 0x16A0 },
  { "PIC16F1518", 21, 17, 27, 40, 2, 0, 1, 14, 0x3FE0, 0x16C0 },
  { "PIC16F1519", 21, 17, 27, 40, 2, 0, 1, 14, 0x3FE0, 0x16E0 },
  { "PIC16F1526", 21, 17, 27, 40, 2, 0, 1, 14, 0x3FE0, 0x1580 },
  { "PIC16F1527", 21, 17, 27, 40, 2, 0, 1, 14, 0x3FE0, 0x15A0 },
  { "PIC16F1574", 21, 21, 31, 41, 2, 0, 1, 14, 0x3FFF, 0x3000 },
  { "PIC16F1575", 21, 21, 31, 41, 2, 0, 1, 14, 0x3FFF, 0x3001 },
  { "PIC16F1578", 21, 21, 31, 41, 2, 0, 1, 14, 0x3FFF, 0x3002 },
  { "PIC16F1579", 21, 21, 31, 41, 2, 0, 1, 14, 0x3FFF, 0x3003 },
  { "PIC16F1764", 21, 22, 33, 35, 2, 0, 1, 14, 0x3FFF, 0x3080 },
  { "PIC16F1765", 21, 22, 33, 35, 2, 0, 1, 14, 0x3FFF, 0x3081 },
  { "PIC16F1768", 21, 22, 33, 35, 2, 0, 1, 14, 0x3FFF, 0x3084 },
  { "PIC16F1769", 21, 22, 33, 35, 2, 0, 1, 14, 0x3FFF, 0x3085 },
  { "PIC16F1773", 21, 22, 33, 35, 2, 0, 1, 14, 0x3FFF, 0x308A },
  { "PIC16F1776", 21, 22, 33, 35, 2, 0, 1, 14, 0x3FFF, 0x308B },
  { "PIC16F1777", 21, 22, 33, 35, 2, 0, 1, 14, 0x3FFF, 0x308E },
  { "PIC16F1778", 21, 22, 33, 35, 2, 0, 1, 14, 0x3FFF, 0x308F },
  { "PIC16F1779", 21, 22, 33, 35, 2, 0, 1, 14, 0x3FFF, 0x3090 },
  { "PIC16F1829LIN", 19, 15, 26, 27, 2, 0, 1, 14, 0x3FE0, 0x27E0 },
  { "PIC16F1946", 19, 15, 25, 27, 2, 0, 1, 14, 0x3FE0, 0x2500 },
  { "PIC16F1947", 19, 15, 25, 27, 2, 0, 1, 14, 0x3FE0, 0x2520 },
  { "PIC16LF1512", 20, 17, 32, 30, 2, 0, 1, 14, 0x3FE0, 0x1720 },
  { "PIC16LF1513", 20, 17, 32, 30, 2, 0, 1, 14, 0x3FE0, 0x1740 },
  { "PIC16LF1516", 20, 17, 27, 30, 2, 0, 1, 14, 0x3FE0, 0x1780 },
  { "PIC16LF1517", 20, 17, 27, 30, 2, 0, 1, 14, 0x3FE0, 0x17A0 },
  { "PIC16LF1518", 20, 17, 27, 30, 2, 0, 1, 14, 0x3FE0, 0x17C0 },
  { "PIC16LF1519", 20, 17, 27, 30, 2, 0, 1, 14, 0x3FE0, 0x17E0 },
  { "PIC16LF1526", 20, 17, 27, 30, 2, 0, 1, 14, 0x3FE0, 0x15C0 },
  { "PIC16LF1527", 20, 17, 27, 30, 2, 0, 1, 14, 0x3FE0, 0x15E0 },
  { "PIC16LF1554", 20, 17, 27, 28, 2, 0, 1, 14, 0x3FE0, 0x2F00 },
  { "PIC16LF1559", 20, 17, 27, 28, 2, 0, 1, 14, 0x3FE0, 0x2F20 },
  { "PIC16LF1566", 20, 17, 27, 28, 2, 0, 1, 14, 0x3FFF, 0x3046 },
  { "PIC16LF1567", 20, 17, 27, 28, 2, 0, 1, 14, 0x3FFF, 0x3047 },
  { "PIC16LF1574", 20, 21, 31, 41, 2, 0, 1, 14, 0x3FFF, 0x3004 },
  { "PIC16LF1575", 20, 21, 31, 41, 2, 0, 1, 14, 0x3FFF, 0x3005 },
  { "PIC16LF1578", 20, 21, 31, 41, 2, 0, 1, 14, 0x3FFF, 0x3006 },
  { "PIC16LF1579", 20, 21, 31, 41, 2, 0, 1, 14, 0x3FFF, 0x3007 },
  { "PIC16LF1764", 20, 22, 33, 35, 2, 0, 1, 14, 0x3FFF, 0x3082 },
  { "PIC16LF1765", 20, 22, 33, 35, 2, 0, 1, 14, 0x3FFF, 0x3083 },
  { "PIC16LF1768", 20, 22, 33, 35, 2, 0, 1, 14, 0x3FFF, 0x3086 },
  { "PIC16LF1769", 20, 22, 33, 35, 2, 0, 1, 14, 0x3FFF, 0x3087 },
  { "PIC16LF1773", 25, 22, 33, 35, 2, 0, 1, 14, 0x3FFF, 0x308C },
  { "PIC16LF1776", 25, 22, 33, 35, 2, 0, 1, 14, 0x3FFF, 0x308D },
  { "PIC16LF1777", 20, 22, 33, 35, 2, 0, 1, 14, 0x3FFF, 0x3091 },
  { "PIC16LF1778", 20, 22, 33, 35, 2, 0, 1, 14, 0x3FFF, 0x3092 },
  { "PIC16LF1779", 20, 22, 33, 35, 2, 0, 1, 14, 0x3FFF, 0x3093 },
  { "PIC16LF1824T39A", 20, 15, 26, 27, 2, 0, 1, 14, 0x3FE0, 0x2840 },
  { "PIC16LF1946", 20, 15, 25, 26, 2, 0, 1, 14, 0x3FE0, 0x2580 },
  { "PIC16LF1947", 20, 15, 25, 26, 2, 0, 1, 14, 0x3FE0, 0x25A0 },
};



static void pic8ApplyPk2Supplement(const pic8_device_index_t *entry,
                                   pic_prog_params_t *out)
{
    uint16_t i;
    const pic8_pk2_supplement_t *pk2;
    pic8_icsp_common_t *cm;

    if (entry == NULL || out == NULL)
        return;

    cm = &out->common;
    for (i = 0U; i < (uint16_t)(sizeof(g_pk2SupplementTable) / sizeof(g_pk2SupplementTable[0])); i++)
    {
        pk2 = &g_pk2SupplementTable[i];
        if (strcmp(entry->name, pk2->name) != 0)
            continue;

        cm->code_base_addr = 0U;
        cm->code_end_addr = pk2->program_words;
        cm->config_addr = pk2->config_word_addr;
        cm->config_word_count = pk2->config_words;
        cm->userid_base = pk2->userid_word_addr;
        cm->userid_word_count = pk2->userid_words;

        if (cm->core_family == PIC8_CORE_BASELINE_12BIT)
        {
            /* Baseline devices enter Program/Verify with PC at configuration. */
            cm->pc_init_mode = PIC8_PC_INIT_AT_CONFIG;
            /* PK2 config_word_addr is the HEX logical address, not the PC. */
            out->baseLine.config_shadow_addr = (pk2->program_words != 0U) ?
                                                ((pk2->program_words << 1) - 1U) : 0U;
            if (pk2->osccal_save != 0U && pk2->program_words != 0U)
            {
                cm->osccal_base = pk2->program_words - 1U;
                cm->osccal_word_count = 1U;
                /* PIC12F508/509 factory backup is immediately after User IDs. */
                cm->cal_data_base = pk2->userid_word_addr + pk2->userid_words;
                cm->cal_data_word_count = 1U;
            }
        }
        return;
    }
}

static void pic8AggregateParams(const pic8_device_index_t *entry,
                                pic_prog_params_t *out)
{
    const pic8_power_entry_t  *pw;
    const pic8_seq_entry_t    *sq;
    const pic8_space_entry_t  *sp;
    const pic8_dcr_group_t    *dc;
    const pic8_sub_entry_t    *sb;
    uint8_t i;
    pic8_icsp_common_t *cm = &out->common;
    if (entry == NULL || out == NULL) return;
    memset(out, 0, sizeof(*out));
    cm->core_family = entry->core_family;
    cm->pc_init_mode = entry->pc_init_mode;
    cm->inst_bits = entry->inst_bits;
    cm->data_bits = 8;
    cm->code_word_bytes = 2;
    cm->has_load_config_cmd = (entry->core_family != PIC8_CORE_BASELINE_12BIT) ? 1 : 0;
    cm->has_eeprom = entry->has_eeprom;
    cm->has_checksum = (entry->core_family == PIC8_CORE_ENHANCED_14BIT) ? 1 : 0;
    cm->tries = 1;
    i = entry->power_idx;
    if (i < PIC8_POWER_TABLE_SIZE) {
        pw = &g_powerTable[i];
        cm->vpp_min_mv = pw->vpp_min_mv;
        cm->vpp_max_mv = pw->vpp_max_mv;
        cm->vdd_min_mv = pw->vdd_min_mv;
        cm->vdd_max_mv = pw->vdd_max_mv;
        cm->vdd_nominal_mv = pw->vdd_nominal_mv;
        cm->lvp_threshold_mv = pw->lvp_threshold_mv;
        cm->has_vpp_first = pw->has_vpp_first;
        cm->lvp_mode = pw->lvp_mode;
    }
    i = entry->seq_idx;
    if (i < PIC8_SEQ_TABLE_SIZE) {
        sq = &g_seqTable[i];
        cm->wait_pgm_us = sq->wait_pgm_us;
        cm->wait_erase_us = sq->wait_erase_us;
        cm->wait_cfg_us = sq->wait_cfg_us;
        cm->wait_userid_us = sq->wait_userid_us;
        cm->wait_eedata_us = sq->wait_eedata_us;
        cm->wait_rowerase_us = sq->wait_rowerase_us;
        cm->wait_lvpgm_us = sq->wait_lvpgm_us;
        cm->wait_lverase_us = sq->wait_lverase_us;
        cm->erase_algo = sq->erase_algo;
        cm->tries = sq->tries;
        cm->has_row_erase_cmd = sq->has_row_erase_cmd;
        cm->row_pgm_words = sq->row_pgm_words;
        cm->row_cfg_words = sq->row_cfg_words;
        cm->row_userid_words = sq->row_userid_words;
        cm->row_eedata_words = sq->row_eedata_words;
        cm->row_erase_words = sq->row_erase_words;
        cm->latch_pgm_words = sq->latch_pgm_words;
        cm->latch_cfg_words = sq->latch_cfg_words;
        cm->latch_userid_words = sq->latch_userid_words;
        cm->latch_eedata_words = sq->latch_eedata_words;
        cm->latch_rowerase_words = sq->latch_rowerase_words;
    }
    i = entry->space_idx;
    if (i < PIC8_SPACE_TABLE_SIZE) {
        sp = &g_spaceTable[i];
        cm->code_base_addr = sp->code_base_addr;
        cm->code_end_addr = sp->code_end_addr;
        cm->config_space_base = sp->config_space_base;
        cm->config_addr = sp->config_addr;
        cm->config_word_count = sp->config_word_count;
        cm->userid_base = sp->userid_base;
        cm->userid_word_count = sp->userid_word_count;
        cm->deviceid_addr = sp->deviceid_addr;
        cm->eedata_base = sp->eedata_base;
        cm->eedata_end_addr = sp->eedata_end_addr;
        cm->osccal_base = sp->osccal_base;
        cm->osccal_word_count = sp->osccal_word_count;
        cm->cal_data_base = sp->cal_data_base;
        cm->cal_data_word_count = sp->cal_data_word_count;
        i = sp->sub_index;
        if (i < PIC8_SUB_TABLE_SIZE) {
            sb = &g_subTable[i];
            switch (entry->core_family) {
            case PIC8_CORE_BASELINE_12BIT:
                out->baseLine.config_shadow_addr = sb->config_shadow_addr;
                out->baseLine.osccal_addr = sb->osccal_addr;
                break;
            case PIC8_CORE_MIDRANGE_14BIT:
                out->midRange.config2_addr = sb->config2_addr;
                out->midRange.config3_addr = sb->config3_addr;
                out->midRange.cal_word1_addr = sb->cal_word1_addr;
                out->midRange.cal_word2_addr = sb->cal_word2_addr;
                break;
            case PIC8_CORE_ENHANCED_14BIT:
                out->enhanced.config2_addr = sb->config2_addr;
                out->enhanced.config3_addr = sb->config3_addr;
                out->enhanced.config4_addr = sb->config4_addr;
                out->enhanced.debug_reserved_base = sb->debug_reserved_base;
                out->enhanced.debug_reserved_end = sb->debug_reserved_end;
                out->enhanced.boundary_words = sb->boundary_words;
                break;
            }
        }
    }
    cm->deviceid_mask = entry->deviceid_mask;
    cm->deviceid_expected = entry->deviceid_expected;

    if (cm->lvp_mode == PIC8_LVP_MCHP_KEY) {
        cm->lvp_key_required = 1;
        cm->lvp_key_bits = 32;
        cm->lvp_key_value = 0x5048434DUL;
    }
    i = entry->dcr_idx;
    if (i < PIC8_DCR_TABLE_SIZE) {
        dc = &g_dcrTable[i];
        for (i = 0; i < dc->dcr_count && i < MAX_CONFIG_WORDS; i++)
            memcpy(&cm->config_dcr[i], &dc->dcr[i], sizeof(pic8_dcr_entry_t));
    }

    pic8ApplyPk2Supplement(entry, out);
}

/* ─---------------─ 供模块外部调用的API函数 ─------------------─ */
/// @brief 获取器件列表
/// @param startIndex 起始索引
/// @param rdCount 读取数量
/// @return 返回器件总数
uint16_t pic8GetDeviceList(uint16_t startIndex, uint16_t rdCount) {
    uint16_t i, count = PIC8_DEVICE_TABLE_SIZE;
    uint8_t buff[100];

    if (rdCount == 0) return count;
    if ( startIndex + rdCount > PIC8_DEVICE_TABLE_SIZE){ 
        count = PIC8_DEVICE_TABLE_SIZE-startIndex;
    }else{
        count = rdCount;
    }
    for (i = 0; i < count; i++) {
        sprintf(buff,"Device %d: %s\r\n", startIndex + i, g_deviceTable[startIndex + i].name);
        uart1_WriteString(buff);
    }
    return PIC8_DEVICE_TABLE_SIZE;
}

/// @brief 根据器件名称，获取器件参数，并填充到给定的结构体指针中
/// @param deviceName ：器件名称
/// @param out ：输出参数结构体指针
/// @return 成功返回0，失败返回-1
int8_t pic8FindDeviceByName(const char *deviceName, pic_prog_params_t *out) {
    uint16_t i;
    if (deviceName == NULL || out == NULL) return -1;
    for (i = 0; i < PIC8_DEVICE_TABLE_SIZE; i++) {
        const char *t = g_deviceTable[i].name;
        const char *d = deviceName;
        uint8_t ok = 1, j;
        for (j = 0; j < 16; j++) {
            char c1 = t[j]; if (c1>='A'&&c1<='Z') c1 += 32;
            char c2 = d[j]; if (c2>='A'&&c2<='Z') c2 += 32;
            if (c1 != c2) { ok = 0; break; }
            if (c1 == 0 && c2 == 0) break;
        }
        if (ok) { pic8AggregateParams(&g_deviceTable[i], out); return 0; }
    }
    return -1;
}

/// @brief 根据器件索引，获取器件参数，并填充到给定的结构体指针中
/// @param index ：器件索引
/// @param out ：输出参数结构体指针
/// @return 成功返回0，失败返回-1
int8_t pic8FindDeviceByIndex(uint16_t index, pic_prog_params_t *out) {
    if (out == NULL || index >= PIC8_DEVICE_TABLE_SIZE) return -1;
    #if DEBUG_HARDWARE_CONFIG && PIC8_DB_TRACE
        printf("pic8FindDeviceByIndex: idx=%u, name=%s\r\n", index, g_deviceTable[index].name);
    #endif 
    pic8AggregateParams(&g_deviceTable[index], out);
    #if DEBUG_HARDWARE_CONFIG && PIC8_DB_TRACE
    printf("PIC DB: pwr=%u seq=%u space=%u dcr=%u core=%u init=%u\r\n",
           g_deviceTable[index].power_idx, g_deviceTable[index].seq_idx,
           g_deviceTable[index].space_idx, g_deviceTable[index].dcr_idx,
           out->common.core_family, out->common.pc_init_mode);
    printf("PIC DB: codeEnd=%lX cfgMeta=%lX cfgShadow=%lX user=%lX osccal=%lX cal=%lX\r\n",
           (unsigned long)out->common.code_end_addr,
           (unsigned long)out->common.config_addr,
           (unsigned long)out->baseLine.config_shadow_addr,
           (unsigned long)out->common.userid_base,
           (unsigned long)out->common.osccal_base,
           (unsigned long)out->common.cal_data_base);
    #endif
    return 0;
}

/// @brief 获取器件总数
/// @param  
/// @return 器件总数
uint16_t pic8GetDeviceCount(void) { return PIC8_DEVICE_TABLE_SIZE; }

/// @brief 根据索引获取器件条目
/// @param index 器件索引
/// @param entry 输出指向器件条目的指针
/// @return 成功返回0，失败返回-1
int8_t pic8GetDeviceEntry(uint16_t index, const pic8_device_index_t **entry) {
    if (entry == NULL || index >= PIC8_DEVICE_TABLE_SIZE) return -1;
    *entry = &g_deviceTable[index];
    return 0;
}
