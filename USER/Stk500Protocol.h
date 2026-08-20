/*
 * STK500?????????????????????- ?????????????????/???????????????????????????????
 */

#ifndef __STK500PROTOCOL_H_INCLUDED__
#define __STK500PROTOCOL_H_INCLUDED__

#include <stdint.h>
#include <stddef.h>

#include "sys.h"

/*
 * STK500v2 protocol parser - STM32 port
 * Ported from AVR-Doper firmware/stk500protocol.c (C. Starkjohann, obdev.at)
 *
 * Data flow:
 *   HID_Rx_Store() -> stkSetRxChar(byte) -> assemble STK frame
 *   -> stkEvaluateRxMessage() -> generate response -> stkSetTxMessage()
 *   -> HID_GetReport_Buffer() provides response bytes to USB host
 *
 * External called from main loop: stkPoll()
 */

/* Boot/App 公共契约：EEPROM 启动模式标志等，与 Boot 工程共用同一来源 */
#include "bootAppCommon.h"

#define BUFFER_SIZE     281 /* results in 275 bytes max body size */
#define RX_TIMEOUT      200 /* timeout in milliseconds */

/* STK_CMD_FIRMWARE_UPGRADE payload magic; must match STM32F103VET6_BootLoader_PC. */
#define STK_FW_UPGRADE_MAGIC0       0xA5U
#define STK_FW_UPGRADE_MAGIC1       0x5AU

typedef union{      // ?????????/????????????????????????????????????
    uint16_t    word;
    uint8_t     bytes[2];
}utilWord_t;

typedef union{      // ??????/???????????????????????????
    uint32_t    dword;
    uint8_t     bytes[4];
}utilDword_t;

typedef struct{     // STK?????????????????????????????????????????????????????????
    uint8_t     bytes[32];
    struct{
        int     buildVersionLow;
        uint8_t reserved1[14];
        uint8_t hardwareVersion;
        uint8_t softwareVersionMajor;
        uint8_t softwareVersionMinor;
        uint8_t reserved2;
        uint8_t vTarget;
        uint8_t vRef;
        uint8_t oscPrescale;
        uint8_t oscCmatch;
        uint8_t sckDuration;
        uint8_t reserved3;
        uint8_t topcardDetect;
        uint8_t reserved4;
        uint8_t status;
        uint8_t data;
        uint8_t resetPolarity;
        uint8_t controllerInit;
    }       s;
}stkParam_t;

/* Global variables */
extern utilDword_t  stkAddress;
extern stkParam_t   stkParam;
/* STK500 ??????????????????USB HID???USB CDC ??? Flash ??????????????????????????? STK500 ???????????? */
#define STK_DATA_SOURCE_USB_HID         0U
#define STK_DATA_SOURCE_USB_CDC         1U
#define STK_DATA_SOURCE_FLASH_RECORD    2U
#define STK_DATA_SOURCE_USB_WINUSB     3U

typedef struct stkDataFrame{
    const uint8_t *frame;       /* RX: ?????? STK500 ???, ??????????????? USB HID ??? Flash ?????? */
    uint16_t frameLen;          /* RX: ???????????????, ?????? payloadLen + 6 */
    uint8_t *txFrame;           /* TX: ???????????????????????????USB ??????????????? USB TX, ????????????????????????????????? */
    uint16_t txFrameSize;       /* TX: ???????????????????????? */
    uint16_t txFrameLen;        /* TX: ???????????????????????????????????? */
    uint8_t  source;            /* STK_DATA_SOURCE_xxx, ???????????? USB ??? Flash ?????? */
} stkDataFrame_t;

/* =================== [ ISP parameter structs ] =================== */
/* Ported from AVR-Doper firmware/stk500protocol.h */

typedef struct stkEnterProgIsp{ // ?????????ISP
    uint8_t   timeout;
    uint8_t   stabDelay;
    uint8_t   cmdExeDelay;
    uint8_t   synchLoops;
    uint8_t   byteDelay;
    uint8_t   pollValue;
    uint8_t   pollIndex;
    uint8_t   cmd[4];
}stkEnterProgIsp_t;

typedef struct stkLeaveProgIsp{ // ??????????SP
    uint8_t   preDelay;
    uint8_t   postDelay;
}stkLeaveProgIsp_t;

typedef struct stkChipEraseIsp{ // ??????????????????
    uint8_t   eraseDelay;
    uint8_t   pollMethod;
    uint8_t   cmd[4];
}stkChipEraseIsp_t;

typedef struct stkProgramFlashIsp{  // ?????????Flash
    uint8_t   numBytes[2];
    uint8_t   mode;
    uint8_t   delay;
    uint8_t   cmd[3];
    uint8_t   poll[2];
    uint8_t   data[1];    /* actually more data than 1 byte */
}stkProgramFlashIsp_t;

typedef struct stkReadFlashIsp{     // ?????????Flash
    uint8_t   numBytes[2];
    uint8_t   cmd;
}stkReadFlashIsp_t;

typedef struct stkReadFlashIspResult{   // Flash??????????????????
    uint8_t   status1;
    uint8_t   data[1];    /* actually more than 1 byte */
    /* uint8_t status2 */
}stkReadFlashIspResult_t;

typedef struct stkProgramFuseIsp{       // ISP ???????????????
    uint8_t   cmd[4];
}stkProgramFuseIsp_t;

typedef struct stkReadFuseIsp{          // ISP ???????????????
    uint8_t   retAddr;
    uint8_t   cmd[4];
}stkReadFuseIsp_t;

typedef struct stkMultiIsp{             // ISP
    uint8_t   numTx;
    uint8_t   numRx;
    uint8_t   rxStartAddr;
    uint8_t   txData[1];  /* actually more than 1 byte */
}stkMultiIsp_t;

typedef struct stkMultiIspResult{
    uint8_t   status1;
    uint8_t   rxData[1];  /* potentially more than 1 byte */
    /* uint8_t status2 */
}stkMultiIspResult_t;

/* =================== [ HVSP parameter structs ] =================== */
/* Ported from AVR-Doper firmware/stk500protocol.h */

typedef struct stkEnterProgHvsp{
    uint8_t   stabDelay;
    uint8_t   cmdExeDelay;
    uint8_t   synchCycles;
    uint8_t   latchCycles;
    uint8_t   toggleVtg;
    uint8_t   powerOffDelay;
    uint8_t   resetDelay1;
    uint8_t   resetDelay2;
}stkEnterProgHvsp_t;

typedef struct stkLeaveProgHvsp{
    uint8_t   stabDelay;
    uint8_t   resetDelay;
}stkLeaveProgHvsp_t;

typedef struct stkChipEraseHvsp{
    uint8_t   pollTimeout;
    uint8_t   eraseTime;
}stkChipEraseHvsp_t;

typedef struct stkProgramFlashHvsp{
    uint8_t   numBytes[2];
    uint8_t   mode;
    uint8_t   pollTimeout;
    uint8_t   data[1];    /* actually more data than 1 byte */
}stkProgramFlashHvsp_t;

typedef struct stkReadFlashHvsp{
    uint8_t   numBytes[2];
}stkReadFlashHvsp_t;

#define stkReadFlashHvspResult_t    stkReadFlashIspResult_t

typedef struct stkProgramFuseHvsp{
    uint8_t   fuseAddress;
    uint8_t   fuseByte;
    uint8_t   pollTimeout;
}stkProgramFuseHvsp_t;

typedef struct stkReadFuseHvsp{
    uint8_t   fuseAddress;
}stkReadFuseHvsp_t;

/* =================== [ PP parameter structs ] =================== */
/* Ported from AVR-Doper firmware/stk500protocol.h */

typedef struct stkEnterProgPp{
    uint8_t   stabDelay;
    uint8_t   progModeDelay;
    uint8_t   latchCycles;
    uint8_t   toggleVtg;
    uint8_t   powerOffDelay;
    uint8_t   resetDelayMs;
    uint8_t   resetDelayUs;
}stkEnterProgPp_t;

#define stkLeaveProgPp_t        stkLeaveProgHvsp_t

typedef struct stkChipErasePp{
    uint8_t   pulseWidth;
    uint8_t   pollTimeout;
}stkChipErasePp_t;

#define stkProgramFlashPp_t     stkProgramFlashHvsp_t

#define stkReadFlashPp_t        stkReadFlashHvsp_t

#define stkReadFlashPpResult_t  stkReadFlashHvspResult_t

typedef struct stkProgramFusePp{
    uint8_t   address;
    uint8_t   data;
    uint8_t   pulseWidth;
    uint8_t   pollTimeout;
}stkProgramFusePp_t;

#define stkReadFusePp_t         stkReadFuseHvsp_t

/* =================== [ ICSP parameter structs ] =================== */

typedef struct stkEnterProgIcsp{
    uint8_t   deviceProfile;    /* ????????????????????? 0=??????????????????, 1=baseline, 2=mid-range, 3=enhanced */
    uint8_t   enterMode;        /* bit0=prefer LVP, bit7=????????? deviceIndex */
    uint8_t   deviceIndex[2];   /* ????????????? little-endian ?????????????????? */
}stkEnterProgIcsp_t;

typedef struct stkLeaveProgIcsp{
    uint8_t   reserved1;
    uint8_t   reserved2;
}stkLeaveProgIcsp_t;

typedef struct stkChipEraseIcsp{
    uint8_t   flags;
    uint8_t   reserved;
}stkChipEraseIcsp_t;

typedef struct stkProgramFlashIcsp{
    uint8_t   numWords[2];
    uint8_t   flags;
    uint8_t   delay;
    uint8_t   data[1];          /* ???????????????????????????????????????????? */
}stkProgramFlashIcsp_t;

typedef struct stkReadFlashIcsp{
    uint8_t   numWords[2];
    uint8_t   flags;
}stkReadFlashIcsp_t;

#define stkReadFlashIcspResult_t    stkReadFlashIspResult_t

typedef struct stkProgramEepromIcsp{
    uint8_t   numBytes[2];
    uint8_t   flags;
    uint8_t   delay;
    uint8_t   data[1];          /* ???????????????????????????????????????????? */
}stkProgramEepromIcsp_t;

typedef struct stkReadEepromIcsp{
    uint8_t   numBytes[2];
    uint8_t   flags;
}stkReadEepromIcsp_t;

#define stkReadEepromIcspResult_t   stkReadFlashIspResult_t

typedef struct stkProgramConfigIcsp{
    uint8_t   index;
    uint8_t   reserved;
    uint8_t   value[2];
}stkProgramConfigIcsp_t;

typedef struct stkReadConfigIcsp{
    uint8_t   index;
}stkReadConfigIcsp_t;

typedef struct stkProgramUserIdIcsp{
    uint8_t   index;
    uint8_t   reserved;
    uint8_t   value[2];
}stkProgramUserIdIcsp_t;

typedef struct stkReadUserIdIcsp{
    uint8_t   index;
}stkReadUserIdIcsp_t;

typedef struct stkReadSignatureIcsp{
    uint8_t   index;
}stkReadSignatureIcsp_t;

typedef struct stkReadOsccalIcsp{
    uint8_t   index;
}stkReadOsccalIcsp_t;

typedef struct stkWriteOsccalIcsp{
    uint8_t   index;
    uint8_t   reserved;
    uint8_t   value[2];
}stkWriteOsccalIcsp_t;

/* Public functions */
void    stkSetRxChar(uint8_t c);
void    stkSetRxCharEx(uint8_t src, uint8_t c);
void    stkEvaluateRxMessage(stkDataFrame_t *pDataFrame);
int     stkGetTxByte(void);
int     stkGetTxCount(void);
void    stkPoll(void);              /* must be called from main loop */
uint8_t stkGetTxSource(void);
uint8_t stkFwUpgradeRequested(void);
void    stkIncrementAddress(void);

/* =================== [ STK general command constants ] =================== */
#define STK_CMD_SIGN_ON                         0x01
#define STK_CMD_SET_PARAMETER                   0x02
#define STK_CMD_GET_PARAMETER                   0x03
#define STK_CMD_SET_DEVICE_PARAMETERS           0x04
#define STK_CMD_OSCCAL                          0x05
#define STK_CMD_LOAD_ADDRESS                    0x06
#define STK_CMD_FIRMWARE_UPGRADE                0x07
#define STK_CMD_SET_WORK_STATE                  0x08
#define STK_CMD_SET_PROG_STATE                  0x09
#define STK_CMD_GET_OFFLINE_INFO                0x0A
#define STK_CMD_GET_OFFLINE_PACKAGE             0x0B
#define STK_CMD_SET_OFFLINE_ACTIVE              0x0C
#define CMD_CHECK_TARGET_CONNECTION             0x0D
#define CMD_LOAD_RC_ID_TABLE                    0x0E
#define CMD_LOAD_EC_ID_TABLE                    0x0F

/* =================== [ STK ISP command constants ] =================== */
#define STK_CMD_ENTER_PROGMODE_ISP              0x10
#define STK_CMD_LEAVE_PROGMODE_ISP              0x11
#define STK_CMD_CHIP_ERASE_ISP                  0x12
#define STK_CMD_PROGRAM_FLASH_ISP               0x13
#define STK_CMD_READ_FLASH_ISP                  0x14
#define STK_CMD_PROGRAM_EEPROM_ISP              0x15
#define STK_CMD_READ_EEPROM_ISP                 0x16
#define STK_CMD_PROGRAM_FUSE_ISP                0x17
#define STK_CMD_READ_FUSE_ISP                   0x18
#define STK_CMD_PROGRAM_LOCK_ISP                0x19
#define STK_CMD_READ_LOCK_ISP                   0x1A
#define STK_CMD_READ_SIGNATURE_ISP              0x1B
#define STK_CMD_READ_OSCCAL_ISP                 0x1C
#define STK_CMD_SPI_MULTI                       0x1D

/* =================== [ STK PP command constants ] =================== */
#define STK_CMD_ENTER_PROGMODE_PP               0x20
#define STK_CMD_LEAVE_PROGMODE_PP               0x21
#define STK_CMD_CHIP_ERASE_PP                   0x22
#define STK_CMD_PROGRAM_FLASH_PP                0x23
#define STK_CMD_READ_FLASH_PP                   0x24
#define STK_CMD_PROGRAM_EEPROM_PP               0x25
#define STK_CMD_READ_EEPROM_PP                  0x26
#define STK_CMD_PROGRAM_FUSE_PP                 0x27
#define STK_CMD_READ_FUSE_PP                    0x28
#define STK_CMD_PROGRAM_LOCK_PP                 0x29
#define STK_CMD_READ_LOCK_PP                    0x2A
#define STK_CMD_READ_SIGNATURE_PP               0x2B
#define STK_CMD_READ_OSCCAL_PP                  0x2C
#define STK_CMD_SET_CONTROL_STACK               0x2D

/* =================== [ STK HVSP command constants ] =================== */
#define STK_CMD_ENTER_PROGMODE_HVSP             0x30
#define STK_CMD_LEAVE_PROGMODE_HVSP             0x31
#define STK_CMD_CHIP_ERASE_HVSP                 0x32
#define STK_CMD_PROGRAM_FLASH_HVSP              0x33
#define STK_CMD_READ_FLASH_HVSP                 0x34
#define STK_CMD_PROGRAM_EEPROM_HVSP             0x35
#define STK_CMD_READ_EEPROM_HVSP                0x36
#define STK_CMD_PROGRAM_FUSE_HVSP               0x37
#define STK_CMD_READ_FUSE_HVSP                  0x38
#define STK_CMD_PROGRAM_LOCK_HVSP               0x39
#define STK_CMD_READ_LOCK_HVSP                  0x3A
#define STK_CMD_READ_SIGNATURE_HVSP             0x3B
#define STK_CMD_READ_OSCCAL_HVSP                0x3C

/* ==================[STK ICSP command constants ]======================*/
#define STK_CMD_ENTER_PROGMODE_ICSP             0x40
#define STK_CMD_LEAVE_PROGMODE_ICSP             0x41
#define STK_CMD_CHIP_ERASE_ICSP                 0x42
#define STK_CMD_PROGRAM_FLASH_ICSP              0x43
#define STK_CMD_READ_FLASH_ICSP                 0x44
#define STK_CMD_PROGRAM_EEPROM_ICSP             0x45
#define STK_CMD_READ_EEPROM_ICSP                0x46
#define STK_CMD_PROGRAM_CONFIG_ICSP             0x47
#define STK_CMD_READ_CONFIG_ICSP                0x48
#define STK_CMD_PROGRAM_USER_ID_ICSP            0x49
#define STK_CMD_READ_USER_ID_ICSP               0x4A
#define STK_CMD_READ_SIGNATURE_ICSP             0x4B
#define STK_CMD_READ_OSCCAL_ICSP                0x4C
#define STK_CMD_WRITE_OSCCAL_ICSP               0x4D

/* =================== [ STK status constants ] =================== */
#define STK_STATUS_CMD_OK                       0x00
#define STK_STATUS_CMD_TOUT                     0x80
#define STK_STATUS_RDY_BSY_TOUT                 0x81
#define STK_STATUS_SET_PARAM_MISSING            0x82
#define STK_STATUS_CMD_FAILED                   0xC0
#define STK_STATUS_CAL_LOST                     0xE1   /* device calibration lost, cannot erase/reprogram */
#define STK_STATUS_CKSUM_ERROR                  0xC1
#define STK_STATUS_CMD_UNKNOWN                  0xC9

/* =================== [ STK parameter constants ] =================== */
#define STK_PARAM_BUILD_NUMBER_LOW              0x80
#define STK_PARAM_BUILD_NUMBER_HIGH             0x81
#define STK_PARAM_HW_VER                        0x90
#define STK_PARAM_SW_MAJOR                      0x91
#define STK_PARAM_SW_MINOR                      0x92
#define STK_PARAM_VTARGET                       0x94
#define STK_PARAM_VADJUST                       0x95
#define STK_PARAM_OSC_PSCALE                    0x96
#define STK_PARAM_OSC_CMATCH                    0x97
#define STK_PARAM_SCK_DURATION                  0x98
#define STK_PARAM_TOPCARD_DETECT                0x9A
#define STK_PARAM_STATUS                        0x9C
#define STK_PARAM_DATA                          0x9D
#define STK_PARAM_RESET_POLARITY                0x9E
#define STK_PARAM_CONTROLLER_INIT               0x9F
#define STK_PARAM_DEVICE_IDENTITY               0xB6

#define STK_PARAM_ITEM_ID_LEN                  12
#define STK_PARAM_ITEM_DESC_LEN                64

/* =================== [ STK answer constants ] =================== */
#define STK_ANSWER_CKSUM_ERROR                  0xB0

/* =================== [ DFM programmer work modes ] =================== */
#define STK_WORK_MODE_SIMULATE                  0
#define STK_WORK_MODE_ONLINE                    1
#define STK_WORK_MODE_RECORD                    2
#define STK_WORK_MODE_ONLINE_RECORD             3

/* =================== [ Frame constants ] =================== */
#define STK_STX     27
#define STK_TOKEN   14
#define STK_TXMSG_START 5

/* ====================[ Device Family constants ]==============*/
#define STK_MCU_ARCH_AVR        0
#define STK_MCU_ARCH_PIC        1

/* ====================[ Device program mode ] =================*/
#define STK_MCU_PROGRAM_WITH_ISP        0
#define STK_MCU_PROGRAM_WITH_HVSP       1
#define STK_MCU_PROGRAM_WITH_HVPP       2
#define STK_MCU_PROGRAM_WITH_ICSP       3
#define STK_MCU_PROGRAM_WITH_JTAG       4

/* 上位机下发指令 STK_CMD_SET_PARAMETER -> STK_PARAM_DEVICE_IDENTITY 对应的结构体 */
typedef struct
{
    uint8_t arch;     // 器件类型：0= AVR, 1 = PIC
    uint16_t index;     // 器件索引 (0~65535)
    uint8_t itemId[STK_PARAM_ITEM_ID_LEN];     // 项目ID, 用于唯一标识一个项目（一个烧录任务）
    char itemDesc[STK_PARAM_ITEM_DESC_LEN + 1];// 项目描述, 用于显示项目的信息
} stkDeviceIdentity_t;


#endif /* __STK500PROTOCOL_H_INCLUDED__ */
