/*====================================================================
 * 本文件采用GB2312编码，禁止转换为UTF-8。
 * debugBin.c - independent UART1 binary debug module
 *
 * This module consumes the UART1 receive ring buffer directly. It does
 * not use text commands, strtok, printf, HID, or the STK500 protocol.
 * All multi-byte protocol fields are little endian.
 *====================================================================*/

#include "sys.h"
#include "Hardware_Config.h"
#include "usart.h"
#include "delay.h"
#include "led.h"
#include "beep.h"
#include "adc.h"
#include "timer.h"
#include "power.h"
#include "MCP4017_VPP.h"
#include "MCP4017_VDD.h"
#include "MCP4017_Calibration.h"
#include "dutBus.h"
#include "debugBin.h"
#include "eeprom.h"
#include "flash.h"
#include "offLineRecorder.h"
#include <stdio.h>

#define DEBUG_BIN_ADC_TRACE  1

#if DEBUG_BIN_ADC_TRACE
#define DEBUG_BIN_ADC_LOG(...)  printf(__VA_ARGS__)
#else
#define DEBUG_BIN_ADC_LOG(...)
#endif

extern void usb_port_set(u8 enable);

typedef enum
{
    DBIN_RX_SOF1 = 0,
    DBIN_RX_SOF2,
    DBIN_RX_VERSION,
    DBIN_RX_TYPE,
    DBIN_RX_SEQ0,
    DBIN_RX_SEQ1,
    DBIN_RX_CMD0,
    DBIN_RX_CMD1,
    DBIN_RX_STATUS0,
    DBIN_RX_STATUS1,
    DBIN_RX_LEN0,
    DBIN_RX_LEN1,
    DBIN_RX_PAYLOAD,
    DBIN_RX_CRC0,
    DBIN_RX_CRC1
} debugBinRxState_t;

typedef struct
{
    debugBinRxState_t state;
    u8 version;
    u8 type;
    u16 sequence;
    u16 command;
    u16 requestStatus;
    u16 payloadLength;
    u16 payloadIndex;
    u16 receivedCrc;
    u16 calculatedCrc;
    u8 payload[DEBUG_BIN_MAX_PAYLOAD];
} debugBinParser_t;

static debugBinParser_t g_debugBinParser;

static u16 debugBin_ReadU16Le(const u8 *data)
{
    return (u16)((u16)data[0] | ((u16)data[1] << 8));
}
static u32 debugBin_ReadU32Le(const u8 *data)
{
    return (u32)data[0] | ((u32)data[1] << 8) |
           ((u32)data[2] << 16) | ((u32)data[3] << 24);
}

static void debugBin_WriteU16Le(u8 *data, u16 value)
{
    data[0] = (u8)(value & 0xFFU);
    data[1] = (u8)((value >> 8) & 0xFFU);
}

static void debugBin_WriteU32Le(u8 *data, u32 value)
{
    data[0] = (u8)(value & 0xFFUL);
    data[1] = (u8)((value >> 8) & 0xFFUL);
    data[2] = (u8)((value >> 16) & 0xFFUL);
    data[3] = (u8)((value >> 24) & 0xFFUL);
}

static u16 debugBin_Crc16Update(u16 crc, u8 data)
{
    u8 bit;
    crc ^= (u16)data << 8;
    for (bit = 0U; bit < 8U; bit++)
    {
        if ((crc & 0x8000U) != 0U)
        {
            crc = (u16)((crc << 1) ^ 0x1021U);
        }
        else
        {
            crc <<= 1;
        }
    }
    return crc;
}

static u16 debugBin_CalibrationRecordCrc(const mcp4017_calibration_t *record)
{
    const u8 *bytes = (const u8 *)record;
    u16 crc = 0xFFFFU;
    u16 i;

    for (i = 0U; i < sizeof(*record); i++)
    {
        if (i == MCP4017_CALIBRATION_CRC_OFFSET ||
            i == (MCP4017_CALIBRATION_CRC_OFFSET + 1U))
            continue;
        crc = debugBin_Crc16Update(crc, bytes[i]);
    }
    return crc;
}

static u8 debugBin_CalibrationParametersValid(u16 baseMv, u32 gain, u16 offsetMilli)
{
    return (baseMv >= 400U && baseMv <= 1200U &&
            gain >= 100000UL && gain <= 500000UL &&
            offsetMilli <= 10000U) ? 1U : 0U;
}

static u8 debugBin_CalibrationRecordIsValid(const mcp4017_calibration_t *record)
{
    if (record->magic != MCP4017_CALIBRATION_MAGIC ||
        record->version != MCP4017_CALIBRATION_VERSION ||
        record->record_size != sizeof(*record))
        return 0U;

    return (record->crc16 == debugBin_CalibrationRecordCrc(record)) ? 1U : 0U;
}

static void debugBin_CalibrationRecordInit(mcp4017_calibration_t *record)
{
    u8 *bytes = (u8 *)record;
    u16 i;

    for (i = 0U; i < sizeof(*record); i++)
        bytes[i] = 0xFFU;
    record->magic = MCP4017_CALIBRATION_MAGIC;
    record->version = MCP4017_CALIBRATION_VERSION;
    record->record_size = sizeof(*record);
}

static void debugBin_CalibrationEncodeResponse(u8 *response, u8 valid,
                                                u16 baseMv, u32 gain, u16 offsetMilli)
{
    response[0] = valid;
    debugBin_WriteU16Le(&response[1], baseMv);
    debugBin_WriteU32Le(&response[3], gain);
    debugBin_WriteU16Le(&response[7], offsetMilli);
}

static u8 debugBin_CalibrationStoreRecord(mcp4017_calibration_t *record)
{
    mcp4017_calibration_t verify;
    const u8 *bytes = (const u8 *)record;
    u16 i;

    record->crc16 = debugBin_CalibrationRecordCrc(record);
    for (i = 0U; i < sizeof(record->magic); i++)
        SPI_EEPROM_WriteByte(MCP4017_CALIBRATION_EEPROM_ADDR + i, 0xFFU);
    SPI_EEPROM_Write(MCP4017_CALIBRATION_EEPROM_ADDR + sizeof(record->magic),
                     &bytes[sizeof(record->magic)],
                     sizeof(*record) - sizeof(record->magic));
    SPI_EEPROM_Write(MCP4017_CALIBRATION_EEPROM_ADDR, bytes, sizeof(record->magic));

    SPI_EEPROM_Read(MCP4017_CALIBRATION_EEPROM_ADDR, (u8 *)&verify, sizeof(verify));
    if (debugBin_CalibrationRecordIsValid(&verify) == 0U)
        return 0U;
    for (i = 0U; i < sizeof(verify); i++)
    {
        if (((const u8 *)&verify)[i] != bytes[i])
            return 0U;
    }
    return 1U;
}
static void debugBin_SendByteCrc(u8 value, u16 *crc)
{
    uart1_WriteByte(value);
    *crc = debugBin_Crc16Update(*crc, value);
}

static void debugBin_SendResponse(u16 sequence, u16 command, u16 status,
                                  const u8 *payload, u16 payloadLength)
{
    u16 crc;
    u16 i;

    if (payloadLength > DEBUG_BIN_MAX_PAYLOAD)
    {
        status = DEBUG_BIN_STATUS_BAD_LENGTH;
        payloadLength = 0U;
        payload = 0;
    }

    crc = 0xFFFFU;
    uart1_WriteByte(DEBUG_BIN_SOF1);
    uart1_WriteByte(DEBUG_BIN_SOF2);
    debugBin_SendByteCrc(DEBUG_BIN_PROTOCOL_VERSION, &crc);
    debugBin_SendByteCrc(DEBUG_BIN_MSG_RESPONSE, &crc);
    debugBin_SendByteCrc((u8)(sequence & 0xFFU), &crc);
    debugBin_SendByteCrc((u8)(sequence >> 8), &crc);
    debugBin_SendByteCrc((u8)(command & 0xFFU), &crc);
    debugBin_SendByteCrc((u8)(command >> 8), &crc);
    debugBin_SendByteCrc((u8)(status & 0xFFU), &crc);
    debugBin_SendByteCrc((u8)(status >> 8), &crc);
    debugBin_SendByteCrc((u8)(payloadLength & 0xFFU), &crc);
    debugBin_SendByteCrc((u8)(payloadLength >> 8), &crc);

    for (i = 0U; i < payloadLength; i++)
    {
        debugBin_SendByteCrc(payload[i], &crc);
    }

    uart1_WriteByte((u8)(crc & 0xFFU));
    uart1_WriteByte((u8)(crc >> 8));
}

static u8 debugBin_ReadDutPins(void)
{
    u8 value;
    value = 0U;
    if (PORT_IN(HW_DUT_PIN4_DAT) != 0U) value |= 0x01U;
    if (PORT_IN(HW_DUT_PIN5_DAT) != 0U) value |= 0x02U;
    if (PORT_IN(HW_DUT_PIN6_DAT) != 0U) value |= 0x04U;
    if (PORT_IN(HW_DUT_PIN7_DAT) != 0U) value |= 0x08U;
    if (PORT_IN(HW_DUT_PIN8_DAT) != 0U) value |= 0x10U;
    return value;
}

static void debugBin_SetPinState(u8 pinMask, u8 outputEnable, u8 pinValue)
{
    if ((pinMask & 0x01U) != 0U)
    {
        if (outputEnable != 0U) { DUT_PIN4_SET_OUTPUT; PORT_OUT(HW_DUT_PIN4_DAT) = pinValue; }
        else { DUT_PIN4_SET_INPUT; }
    }
    if ((pinMask & 0x02U) != 0U)
    {
        if (outputEnable != 0U) { DUT_PIN5_SET_OUTPUT; PORT_OUT(HW_DUT_PIN5_DAT) = pinValue; }
        else { DUT_PIN5_SET_INPUT; }
    }
    if ((pinMask & 0x04U) != 0U)
    {
        if (outputEnable != 0U) { DUT_PIN6_SET_OUTPUT; PORT_OUT(HW_DUT_PIN6_DAT) = pinValue; }
        else { DUT_PIN6_SET_INPUT; }
    }
    if ((pinMask & 0x08U) != 0U)
    {
        if (outputEnable != 0U) { DUT_PIN7_SET_OUTPUT; PORT_OUT(HW_DUT_PIN7_DAT) = pinValue; }
        else { DUT_PIN7_SET_INPUT; }
    }
    if ((pinMask & 0x10U) != 0U)
    {
        if (outputEnable != 0U) { DUT_PIN8_SET_OUTPUT; PORT_OUT(HW_DUT_PIN8_DAT) = pinValue; }
        else { DUT_PIN8_SET_INPUT; }
    }
}

static void debugBin_DutBusTest(void)
{
    u16 i;
    DUT_VPP_SET_VPP;
    DUT_VDD_SET_VDD;
    DUT_BUS_SET_OUTPUT;
    for (i = 0U; i < 1000U; i++)
    {
        if ((i % 200U) == 0U) { DUT_VPP_SET_GND; DUT_VDD_SET_GND; }
        else if ((i % 250U) == 0U) { DUT_VPP_SET_VPP; DUT_VDD_SET_VDD; }
        PORT_OUT(HW_DUT_PIN4_DAT) = 1; PORT_OUT(HW_DUT_PIN5_DAT) = 1;
        PORT_OUT(HW_DUT_PIN6_DAT) = 1; PORT_OUT(HW_DUT_PIN7_DAT) = 1;
        PORT_OUT(HW_DUT_PIN8_DAT) = 1;
        PORT_OUT(HW_DUT_PIN4_DAT) = 0; PORT_OUT(HW_DUT_PIN5_DAT) = 0;
        PORT_OUT(HW_DUT_PIN6_DAT) = 0; PORT_OUT(HW_DUT_PIN7_DAT) = 0;
        PORT_OUT(HW_DUT_PIN8_DAT) = 0;
    }
    DUT_VPP_SET_FLOAT;
    DUT_VDD_SET_FLOAT;
    DUT_BUS_SET_INPUT;
}

static void debugBin_DelayTest(void)
{
    u16 count;
    u8 data;
    data = 0U;
    DUT_VDD_SET_VDD;
    DUT_PIN8_SET_OUTPUT;
    for (count = 0U; count < 6U; count++)
    {
        timerMsDelay(10U);
        PORT_OUT(HW_DUT_PIN8_DAT) = data;
        data ^= 1U;
    }
    for (count = 0U; count < 10U; count++)
    {
        timerTicksDelay(10U);
        PORT_OUT(HW_DUT_PIN8_DAT) = data;
        data ^= 1U;
    }
    DUT_VDD_SET_FLOAT;
    DUT_PIN8_SET_INPUT;
}

static void debugBin_PeDirectTest(void)
{
    u8 i;
    PORT_RCC_CLK(HW_DUT_PIN7_CTRL);
    PORT_RCC_CLK(HW_DUT_PIN8_CTRL);
    PORT_SET_DIR_PP(HW_DUT_PIN7_CTRL);
    PORT_SET_DIR_PP(HW_DUT_PIN8_CTRL);
    for (i = 0U; i < 6U; i++)
    {
        PORT_OUT(HW_DUT_PIN7_CTRL) = 1;
        PORT_OUT(HW_DUT_PIN8_CTRL) = 1;
        delay_ms(200U);
        PORT_OUT(HW_DUT_PIN7_CTRL) = 0;
        PORT_OUT(HW_DUT_PIN8_CTRL) = 0;
        delay_ms(200U);
    }
}

static void debugBin_Dispatch(void)
{
    debugBinParser_t *p;
    u8 response[DEBUG_BIN_MAX_PAYLOAD];
    u16 responseLength;
    u16 status;
    u16 value16;
    u8 channel;
    u8 i;
    u8 result8;
    u16 scanDelayMs;

    p = &g_debugBinParser;
    responseLength = 0U;
    status = DEBUG_BIN_STATUS_OK;

    if (p->version != DEBUG_BIN_PROTOCOL_VERSION)
    {
        debugBin_SendResponse(p->sequence, p->command, DEBUG_BIN_STATUS_BAD_VERSION, 0, 0U);
        return;
    }
    if (p->type != DEBUG_BIN_MSG_REQUEST)
    {
        debugBin_SendResponse(p->sequence, p->command, DEBUG_BIN_STATUS_BAD_TYPE, 0, 0U);
        return;
    }

    switch (p->command)
    {
    case DEBUG_BIN_CMD_PING:
        if (p->payloadLength > DEBUG_BIN_MAX_PAYLOAD) status = DEBUG_BIN_STATUS_BAD_LENGTH;
        else
        {
            for (i = 0U; i < (u8)p->payloadLength; i++) response[i] = p->payload[i];
            responseLength = p->payloadLength;
        }
        break;

    case DEBUG_BIN_CMD_GET_INFO:
        if (p->payloadLength != 0U) status = DEBUG_BIN_STATUS_BAD_LENGTH;
        else
        {
            response[0] = DEBUG_BIN_PROTOCOL_VERSION;
            response[1] = 1U; /* firmware interface major */
            response[2] = 0U; /* firmware interface minor */
            response[3] = 0U;
            debugBin_WriteU16Le(&response[4], DEBUG_BIN_MAX_PAYLOAD);
            debugBin_WriteU16Le(&response[6], 14U); /* minimum complete frame size */
            responseLength = 8U;
        }
        break;

    case DEBUG_BIN_CMD_GET_CAPABILITIES:
        if (p->payloadLength != 0U) status = DEBUG_BIN_STATUS_BAD_LENGTH;
        else
        {
            debugBin_WriteU32Le(response,
                DEBUG_BIN_CAP_BOARD_IO | DEBUG_BIN_CAP_DUT_POWER |
                DEBUG_BIN_CAP_DUT_BUS | DEBUG_BIN_CAP_ADC |
                DEBUG_BIN_CAP_USB_REENUM);
            responseLength = 4U;
        }
        break;

    case DEBUG_BIN_CMD_USB_REENUMERATE:
        if (p->payloadLength != 0U) status = DEBUG_BIN_STATUS_BAD_LENGTH;
        else
        {
            BEEP = 1;
            usb_port_set(0U);
            delay_ms(300U);
            usb_port_set(1U);
            BEEP = 0;
        }
        break;

    case DEBUG_BIN_CMD_SOFT_RESET:
        if (p->payloadLength != 0U) status = DEBUG_BIN_STATUS_BAD_LENGTH;
        else
        {
            /* 先发送 OK 响应，确保上位机收到应答后再执行复位 */
            debugBin_SendResponse(p->sequence, p->command, DEBUG_BIN_STATUS_OK, 0, 0U);
            delay_ms(10U);
            Sys_Soft_Reset();
        }
        return;

    case DEBUG_BIN_CMD_LED_SET:
        if (p->payloadLength != 2U) status = DEBUG_BIN_STATUS_BAD_LENGTH;
        else if ((p->payload[0] > 2U) || (p->payload[1] > 2U)) status = DEBUG_BIN_STATUS_BAD_PARAM;
        else ledSetState(p->payload[0], p->payload[1]);
        break;

    case DEBUG_BIN_CMD_BEEP_SET:
        if (p->payloadLength != 1U) status = DEBUG_BIN_STATUS_BAD_LENGTH;
        else if (p->payload[0] > 1U) status = DEBUG_BIN_STATUS_BAD_PARAM;
        else BEEP = p->payload[0];
        break;

    case DEBUG_BIN_CMD_VBUS_SET:
        if (p->payloadLength != 1U) status = DEBUG_BIN_STATUS_BAD_LENGTH;
        else if (p->payload[0] > 1U) status = DEBUG_BIN_STATUS_BAD_PARAM;
        else PORT_OUT(HW_USB_ON) = p->payload[0];
        break;

    case DEBUG_BIN_CMD_VPP_SET:
        if (p->payloadLength != 2U) status = DEBUG_BIN_STATUS_BAD_LENGTH;
        else
        {
            value16 = debugBin_ReadU16Le(p->payload);
            if (value16 > 1500U) status = DEBUG_BIN_STATUS_BAD_PARAM;
            else
            {
                result8 = MCP4017_VPP_SetVoltage(value16);
                if (result8 == 0xFFU) status = DEBUG_BIN_STATUS_IO_ERROR;
                else { response[0] = result8; responseLength = 1U; }
            }
        }
        break;

    case DEBUG_BIN_CMD_VDD_SET:
        if (p->payloadLength != 2U) status = DEBUG_BIN_STATUS_BAD_LENGTH;
        else
        {
            value16 = debugBin_ReadU16Le(p->payload);
            if (value16 > 550U) status = DEBUG_BIN_STATUS_BAD_PARAM;
            else
            {
                result8 = MCP4017_VDD_SetVoltage(value16);
                if (result8 == 0xFFU) status = DEBUG_BIN_STATUS_IO_ERROR;
                else { response[0] = result8; responseLength = 1U; }
            }
        }
        break;

    case DEBUG_BIN_CMD_VPP_SCAN:
        if (p->payloadLength != 5U)
        {
            status = DEBUG_BIN_STATUS_BAD_LENGTH;
        }
        else
        {
            scanDelayMs = debugBin_ReadU16Le(&p->payload[3]);
            if ((p->payload[2] == 0U) || (p->payload[0] > 127U) ||
                (p->payload[1] > 127U) || (p->payload[0] > p->payload[1]))
            {
                status = DEBUG_BIN_STATUS_BAD_PARAM;
            }
            else
            {
                if (scanDelayMs < 20U) scanDelayMs = 20U;
                response[0] = p->payload[0];
                response[1] = p->payload[1];
                response[2] = p->payload[2];
                debugBin_WriteU16Le(&response[3], scanDelayMs);
                debugBin_SendResponse(p->sequence, p->command, DEBUG_BIN_STATUS_OK, response, 5U);
                delay_ms(10U);
                if (powerVppScan(p->payload[0], p->payload[1], p->payload[2], scanDelayMs) != 0U)
                {
                    printf("VPP scan failed after response\r\n");
                }
                return;
            }
        }
        break;

    case DEBUG_BIN_CMD_VDD_SCAN:
        if (p->payloadLength != 5U)
        {
            status = DEBUG_BIN_STATUS_BAD_LENGTH;
        }
        else
        {
            scanDelayMs = debugBin_ReadU16Le(&p->payload[3]);
            if ((p->payload[2] == 0U) || (p->payload[0] > 127U) ||
                (p->payload[1] > 127U) || (p->payload[0] > p->payload[1]))
            {
                status = DEBUG_BIN_STATUS_BAD_PARAM;
            }
            else
            {
                if (scanDelayMs < 20U) scanDelayMs = 20U;
                response[0] = p->payload[0];
                response[1] = p->payload[1];
                response[2] = p->payload[2];
                debugBin_WriteU16Le(&response[3], scanDelayMs);
                debugBin_SendResponse(p->sequence, p->command, DEBUG_BIN_STATUS_OK, response, 5U);
                delay_ms(10U);
                if (powerVddScan(p->payload[0], p->payload[1], p->payload[2], scanDelayMs) != 0U)
                {
                    printf("VDD scan failed after response\r\n");
                }
                return;
            }
        }
        break;
    case DEBUG_BIN_CMD_VPP_PARAM_SET:
    case DEBUG_BIN_CMD_VDD_PARAM_SET:
        if (p->payloadLength != 8U)
        {
            status = DEBUG_BIN_STATUS_BAD_LENGTH;
        }
        else
        {
            mcp4017_calibration_t record;
            u16 baseMv = debugBin_ReadU16Le(&p->payload[0]);
            u32 gain = debugBin_ReadU32Le(&p->payload[2]);
            u16 offsetMilli = debugBin_ReadU16Le(&p->payload[6]);

            if (debugBin_CalibrationParametersValid(baseMv, gain, offsetMilli) == 0U)
            {
                status = DEBUG_BIN_STATUS_BAD_PARAM;
            }
            else
            {
                SPI_EEPROM_Read(MCP4017_CALIBRATION_EEPROM_ADDR, (u8 *)&record, sizeof(record));
                if (debugBin_CalibrationRecordIsValid(&record) == 0U)
                    debugBin_CalibrationRecordInit(&record);

                if (p->command == DEBUG_BIN_CMD_VPP_PARAM_SET)
                {
                    record.vpp_base_mv = baseMv;
                    record.vpp_gain = gain;
                    record.vpp_offset_milli = offsetMilli;
                }
                else
                {
                    record.vdd_base_mv = baseMv;
                    record.vdd_gain = gain;
                    record.vdd_offset_milli = offsetMilli;
                }

                if (debugBin_CalibrationStoreRecord(&record) == 0U)
                {
                    status = DEBUG_BIN_STATUS_IO_ERROR;
                }
                else
                {
                    debugBin_CalibrationEncodeResponse(response, 1U, baseMv, gain, offsetMilli);
                    responseLength = 9U;
                }
            }
        }
        break;

    case DEBUG_BIN_CMD_VPP_PARAM_GET:
    case DEBUG_BIN_CMD_VDD_PARAM_GET:
        if (p->payloadLength != 0U)
        {
            status = DEBUG_BIN_STATUS_BAD_LENGTH;
        }
        else
        {
            mcp4017_calibration_t record;
            u8 valid = 0U;
            u16 baseMv = 0U;
            u32 gain = 0UL;
            u16 offsetMilli = 0U;

            SPI_EEPROM_Read(MCP4017_CALIBRATION_EEPROM_ADDR, (u8 *)&record, sizeof(record));
            if (debugBin_CalibrationRecordIsValid(&record) != 0U)
            {
                if (p->command == DEBUG_BIN_CMD_VPP_PARAM_GET)
                {
                    baseMv = record.vpp_base_mv;
                    gain = record.vpp_gain;
                    offsetMilli = record.vpp_offset_milli;
                }
                else
                {
                    baseMv = record.vdd_base_mv;
                    gain = record.vdd_gain;
                    offsetMilli = record.vdd_offset_milli;
                }
                valid = debugBin_CalibrationParametersValid(baseMv, gain, offsetMilli);
            }
            debugBin_CalibrationEncodeResponse(response, valid, baseMv, gain, offsetMilli);
            responseLength = 9U;
        }
        break;
    case DEBUG_BIN_CMD_DUT_VPP_ROUTE:
        if (p->payloadLength != 1U) status = DEBUG_BIN_STATUS_BAD_LENGTH;
        else if (p->payload[0] == 0U) { DUT_VPP_SET_FLOAT; }
        else if (p->payload[0] == 1U) { DUT_VPP_SET_GND; }
        else if (p->payload[0] == 2U) { DUT_VPP_SET_VPP; }
        else status = DEBUG_BIN_STATUS_BAD_PARAM;
        break;

    case DEBUG_BIN_CMD_DUT_VDD_ROUTE:
        if (p->payloadLength != 1U) status = DEBUG_BIN_STATUS_BAD_LENGTH;
        else if (p->payload[0] == 0U) { DUT_VDD_SET_FLOAT; }
        else if (p->payload[0] == 1U) { DUT_VDD_SET_GND; }
        else if (p->payload[0] == 2U) { DUT_VDD_SET_VDD; }
        else status = DEBUG_BIN_STATUS_BAD_PARAM;
        break;

    case DEBUG_BIN_CMD_DUT_PIN_CONFIG:
        if (p->payloadLength != 3U) status = DEBUG_BIN_STATUS_BAD_LENGTH;
        else if (((p->payload[0] & 0xE0U) != 0U) || (p->payload[1] > 1U) || (p->payload[2] > 1U))
            status = DEBUG_BIN_STATUS_BAD_PARAM;
        else debugBin_SetPinState(p->payload[0], p->payload[1], p->payload[2]);
        break;

    case DEBUG_BIN_CMD_DUT_PIN_READ:
        if (p->payloadLength != 0U) status = DEBUG_BIN_STATUS_BAD_LENGTH;
        else { response[0] = debugBin_ReadDutPins(); responseLength = 1U; }
        break;

    case DEBUG_BIN_CMD_DUT_BUS_TEST:
        if (p->payloadLength != 0U) status = DEBUG_BIN_STATUS_BAD_LENGTH;
        else debugBin_DutBusTest();
        break;

    case DEBUG_BIN_CMD_DELAY_TEST:
        if (p->payloadLength != 0U) status = DEBUG_BIN_STATUS_BAD_LENGTH;
        else debugBin_DelayTest();
        break;

    case DEBUG_BIN_CMD_PE_DIRECT_TEST:
        if (p->payloadLength != 0U) status = DEBUG_BIN_STATUS_BAD_LENGTH;
        else debugBin_PeDirectTest();
        break;

    case DEBUG_BIN_CMD_ADC_READ:
        if (p->payloadLength != 1U) status = DEBUG_BIN_STATUS_BAD_LENGTH;
        else
        {
            channel = p->payload[0];
            if (channel >= ADC_SCAN_CHANNELS) status = DEBUG_BIN_STATUS_BAD_PARAM;
            else
            {
                value16 = Adc_GetChannel(channel);
                debugBin_WriteU16Le(response, value16);
                responseLength = 2U;
                DEBUG_BIN_ADC_LOG("[ADC_RAW] ch=%u raw=%u\r\n", channel, value16);
            }
        }
        break;

    case DEBUG_BIN_CMD_ADC_READ_ALL:
        if (p->payloadLength != 0U) status = DEBUG_BIN_STATUS_BAD_LENGTH;
        else
        {
            for (channel = 0U; channel < ADC_SCAN_CHANNELS; channel++)
            {
                debugBin_WriteU16Le(&response[(u16)channel * 2U], Adc_GetChannel(channel));
            }
            responseLength = (u16)ADC_SCAN_CHANNELS * 2U;
            DEBUG_BIN_ADC_LOG("[ADC_RAW_ALL] done\r\n");
        }
        break;

    case DEBUG_BIN_CMD_ADC_READ_AVG:
        if (p->payloadLength != 1U) status = DEBUG_BIN_STATUS_BAD_LENGTH;
        else
        {
            channel = p->payload[0];
            if (channel >= ADC_SCAN_CHANNELS) status = DEBUG_BIN_STATUS_BAD_PARAM;
            else
            {
                value16 = Adc_GetChannelAverage(channel);
                debugBin_WriteU16Le(response, value16);
                responseLength = 2U;
                DEBUG_BIN_ADC_LOG("[ADC_AVG] ch=%u avg=%u\r\n", channel, value16);
            }
        }
        break;

    case DEBUG_BIN_CMD_ADC_READ_AVG_ALL:
        if (p->payloadLength != 0U) status = DEBUG_BIN_STATUS_BAD_LENGTH;
        else
        {
            u16 adcAverage[ADC_SCAN_CHANNELS];
            Adc_GetAllChannelAverage(adcAverage, ADC_SCAN_CHANNELS);
            for (channel = 0U; channel < ADC_SCAN_CHANNELS; channel++)
            {
                debugBin_WriteU16Le(&response[(u16)channel * 2U], adcAverage[channel]);
            }
            responseLength = (u16)ADC_SCAN_CHANNELS * 2U;
            DEBUG_BIN_ADC_LOG("[ADC_AVG_ALL] done\r\n");
        }
        break;

    case DEBUG_BIN_CMD_ADC_READ_REAL:
        if (p->payloadLength != 1U) status = DEBUG_BIN_STATUS_BAD_LENGTH;
        else
        {
            channel = p->payload[0];
            if (channel >= ADC_SCAN_CHANNELS) status = DEBUG_BIN_STATUS_BAD_PARAM;
            else
            {
                u32 realValue = Adc_GetChannelRealValue(channel);
                response[0] = (u8)Adc_GetChannelRealUnit(channel);
                response[1] = 0U;
                debugBin_WriteU32Le(&response[2], realValue);
                responseLength = 6U;
                DEBUG_BIN_ADC_LOG("[ADC_REAL] ch=%u unit=%u value=%lu\r\n", channel, (u8)Adc_GetChannelRealUnit(channel), (unsigned long)realValue);
            }
        }
        break;

    case DEBUG_BIN_CMD_ADC_READ_REAL_ALL:
        if (p->payloadLength != 0U) status = DEBUG_BIN_STATUS_BAD_LENGTH;
        else
        {
            u32 adcRealValue[ADC_SCAN_CHANNELS];
            Adc_GetAllChannelRealValue(adcRealValue, ADC_SCAN_CHANNELS);
            for (channel = 0U; channel < ADC_SCAN_CHANNELS; channel++)
            {
                response[(u16)channel * 5U] = (u8)Adc_GetChannelRealUnit(channel);
                debugBin_WriteU32Le(&response[(u16)channel * 5U + 1U], adcRealValue[channel]);
            }
            responseLength = (u16)ADC_SCAN_CHANNELS * 5U;
            DEBUG_BIN_ADC_LOG("[ADC_REAL_ALL] done\r\n");
        }
        break;
    case DEBUG_BIN_CMD_EEPROM_READ:
        /* 请求帧 payload = offset(2B LE) + len(2B LE)
         * 读取 SPI_EEPROM_CAPACITY (8192) 范围内的数据后回传 */
        if (p->payloadLength != 4U)
        {
            status = DEBUG_BIN_STATUS_BAD_LENGTH;
        }
        else
        {
            u16 readOffset = debugBin_ReadU16Le(&p->payload[0]);
            u16 readLen    = debugBin_ReadU16Le(&p->payload[2]);

            /* 地址越界校验 */
            if ((readOffset >= SPI_EEPROM_CAPACITY) || (readLen == 0U))
            {
                status = DEBUG_BIN_STATUS_BAD_PARAM;
            }
            else
            {
                /* 长度不超过 payload 上限，且不超过 EEPROM 剩余空间 */
                u16 maxRead = SPI_EEPROM_CAPACITY - readOffset;
                if (readLen > DEBUG_BIN_MAX_PAYLOAD)    readLen = DEBUG_BIN_MAX_PAYLOAD;
                if (readLen > maxRead)                  readLen = maxRead;

                SPI_EEPROM_Read((u32)readOffset, response, readLen);
                responseLength = readLen;
            }
        }
        break;

    case DEBUG_BIN_CMD_EEPROM_WRITE:
        /* 请求帧 payload = offset(2B LE) + data(N bytes)
         * 将 data 写入 EEPROM 指定地址，N 最大 126（128-2字节offset） */
        if (p->payloadLength < 3U)  /* 最少 offset(2B) + 1 字节数据 */
        {
            status = DEBUG_BIN_STATUS_BAD_LENGTH;
        }
        else
        {
            u16 writeOffset = debugBin_ReadU16Le(&p->payload[0]);
            u16 writeLen    = p->payloadLength - 2U;  /* 除掉 offset 后的数据长度 */

            /* 地址越界校验 */
            if (writeOffset >= SPI_EEPROM_CAPACITY)
            {
                status = DEBUG_BIN_STATUS_BAD_PARAM;
            }
            else
            {
                /* 长度不超过 EEPROM 剩余空间 */
                u16 maxWrite = SPI_EEPROM_CAPACITY - writeOffset;
                if (writeLen > maxWrite)  writeLen = maxWrite;

                SPI_EEPROM_Write((u32)writeOffset, &p->payload[2], writeLen);
                /* 响应中返回实际写入的字节数 */
                debugBin_WriteU16Le(response, writeLen);
                responseLength = 2U;
            }
        }
        break;

    case DEBUG_BIN_CMD_EEPROM_DEMO:
        /* EEPROM 调试演示 — 先回复 OK，再延迟 100ms 后调用 SPI_EEPROM_DebugDemo
         * 避免 SPI_EEPROM_DebugDemo 内部的 printf 干扰二进制通讯 */
        if (p->payloadLength != 0U)
        {
            status = DEBUG_BIN_STATUS_BAD_LENGTH;
        }
        else
        {
            /* 先发送 OK 响应（不含 payload）*/
            debugBin_SendResponse(p->sequence, p->command, DEBUG_BIN_STATUS_OK, 0, 0U);
            /* 等待 100ms，让上位机收到 OK 响应后再接收 printf 输出的字符串 */
            delay_ms(100U);
            SPI_EEPROM_DebugDemo();
            /* 注意：SPI_EEPROM_DebugDemo 已通过 printf 输出结果，
             * 不再额外发送响应帧。函数返回后即完成。 */
        }
        /* 因为已经提前回复过了，这里不再走末尾的公共 SendResponse */
        return;

    case DEBUG_BIN_CMD_EEPROM_ERASE:
        if (p->payloadLength != 0U)
        {
            status = DEBUG_BIN_STATUS_BAD_LENGTH;
        }
        else
        {
            SPI_EEPROM_EraseAll();
            debugBin_WriteU32Le(response, (u32)SPI_EEPROM_CAPACITY);
            responseLength = 4U;
        }
        break;

    case DEBUG_BIN_CMD_FLASH_READ:
        /* Flash 块读 — 用于与 USB HID 功能双向验证
         * 请求帧 payload = read_addr(4B LE) + read_len(2B LE)
         * 响应帧 payload = 读取到的数据（最多 DEBUG_BIN_MAX_PAYLOAD 字节） */
        if (p->payloadLength != 6U)
        {
            status = DEBUG_BIN_STATUS_BAD_LENGTH;
        }
        else
        {
            u32 flashAddr = (u32)p->payload[0] |
                           ((u32)p->payload[1] << 8) |
                           ((u32)p->payload[2] << 16) |
                           ((u32)p->payload[3] << 24);
            u16 readLen = debugBin_ReadU16Le(&p->payload[4]);

            if (flashAddr >= FLASH_CAPACITY || readLen == 0U)
            {
                status = DEBUG_BIN_STATUS_BAD_PARAM;
            }
            else
            {
                u32 maxRead = FLASH_CAPACITY - flashAddr;
                if ((u32)readLen > maxRead)
                    readLen = (u16)maxRead;
                if (readLen > DEBUG_BIN_MAX_PAYLOAD)
                    readLen = DEBUG_BIN_MAX_PAYLOAD;

                SPI_Flash_Read(response, flashAddr, readLen);
                responseLength = readLen;
            }
        }
        break;

    case DEBUG_BIN_CMD_FLASH_WRITE:
        /* Flash 块写 — 用于与 USB HID 功能双向验证
         * 请求帧 payload = write_addr(4B LE) + write_data(N bytes)
         * 响应帧 payload = 实际写入字节数(2B LE) */
        if (p->payloadLength < 5U)  /* 最少 addr(4B) + 1B 数据 */
        {
            status = DEBUG_BIN_STATUS_BAD_LENGTH;
        }
        else
        {
            u32 flashAddr = (u32)p->payload[0] |
                           ((u32)p->payload[1] << 8) |
                           ((u32)p->payload[2] << 16) |
                           ((u32)p->payload[3] << 24);
            u16 writeLen = p->payloadLength - 4U;

            if (flashAddr >= FLASH_CAPACITY)
            {
                status = DEBUG_BIN_STATUS_BAD_PARAM;
            }
            else
            {
                u32 maxWrite = FLASH_CAPACITY - flashAddr;
                if ((u32)writeLen > maxWrite)
                    writeLen = (u16)maxWrite;
                if (writeLen == 0U)
                {
                    status = DEBUG_BIN_STATUS_BAD_PARAM;
                }
                else
                {
                    SPI_Flash_Write(&p->payload[4], flashAddr, writeLen);
                    debugBin_WriteU16Le(response, writeLen);
                    responseLength = 2U;
                }
            }
        }
        break;

    case DEBUG_BIN_CMD_FLASH_DEMO:
        /* Flash 轮询调试演示 — 先回复 OK，延迟 100ms 后调用 SPI_Flash_DebugDemo */
        if (p->payloadLength != 0U)
        {
            status = DEBUG_BIN_STATUS_BAD_LENGTH;
        }
        else
        {
            debugBin_SendResponse(p->sequence, p->command, DEBUG_BIN_STATUS_OK, 0, 0U);
            delay_ms(100U);
            SPI_Flash_DebugDemo();
        }
        return;

    case DEBUG_BIN_CMD_FLASH_DEMO_DMA:
        /* Flash DMA 调试演示 — 先回复 OK，延迟 100ms 后调用 SPI_Flash_DebugDemo_DMA */
        if (p->payloadLength != 0U)
        {
            status = DEBUG_BIN_STATUS_BAD_LENGTH;
        }
        else
        {
            debugBin_SendResponse(p->sequence, p->command, DEBUG_BIN_STATUS_OK, 0, 0U);
            delay_ms(100U);
            SPI_Flash_DebugDemo_DMA();
        }
        return;

    case DEBUG_BIN_CMD_FLASH_ERASE:
        if (p->payloadLength != 0U)
        {
            status = DEBUG_BIN_STATUS_BAD_LENGTH;
        }
        else
        {
            SPI_Flash_Erase_Chip();
            debugBin_WriteU32Le(response, (u32)FLASH_CAPACITY);
            responseLength = 4U;
        }
        break;

    case DEBUG_BIN_CMD_OFFLINE_PACKAGE_INFO:
        /* This command is intentionally read-only: it only exposes the active
         * package location and immutable recording metadata for PC export. */
        if (p->payloadLength != 0U)
        {
            status = DEBUG_BIN_STATUS_BAD_LENGTH;
        }
        else
        {
            offline_package_index_t summary;
            offline_raw_package_header_t header;
            u16 activeIndex;

            if (offlinePgmerGetActivePackage(&activeIndex) != 0U ||
                offlinePgmerGetPackageSummary(activeIndex, &summary) != 0U ||
                summary.used == 0U ||
                summary.package_state != OFFLINE_PACKAGE_VALID ||
                summary.total_size == 0U ||
                summary.flash_addr >= FLASH_CAPACITY)
            {
                status = DEBUG_BIN_STATUS_IO_ERROR;
            }
            else
            {
                SPI_Flash_Read((u8 *)&header, summary.flash_addr, sizeof(header));
                if (header.magic != OFFLINE_RAW_MAGIC ||
                    header.version != OFFLINE_RAW_VERSION ||
                    header.header_size == 0U ||
                    header.packet_area_offset != header.header_size)
                {
                    status = DEBUG_BIN_STATUS_IO_ERROR;
                }
                else
                {
                    debugBin_WriteU16Le(&response[0], activeIndex);
                    debugBin_WriteU32Le(&response[2], summary.flash_addr);
                    debugBin_WriteU32Le(&response[6], summary.total_size);
                    debugBin_WriteU32Le(&response[10], header.packet_area_offset);
                    debugBin_WriteU32Le(&response[14], summary.packet_area_size);
                    debugBin_WriteU32Le(&response[18], summary.packet_count);
                    debugBin_WriteU32Le(&response[22], summary.crc32);
                    response[26] = summary.identity.arch;
                    debugBin_WriteU16Le(&response[27], summary.identity.index);
                    responseLength = 29U;
                }
            }
        }
        break;

    default:
        status = DEBUG_BIN_STATUS_UNKNOWN_CMD;
        break;
    }

    debugBin_SendResponse(p->sequence, p->command, status, response, responseLength);
}

void debugBin_ResetParser(void)
{
    g_debugBinParser.state = DBIN_RX_SOF1;
    g_debugBinParser.version = 0U;
    g_debugBinParser.type = 0U;
    g_debugBinParser.sequence = 0U;
    g_debugBinParser.command = 0U;
    g_debugBinParser.requestStatus = 0U;
    g_debugBinParser.payloadLength = 0U;
    g_debugBinParser.payloadIndex = 0U;
    g_debugBinParser.receivedCrc = 0U;
    g_debugBinParser.calculatedCrc = 0xFFFFU;
}

void debugBin_Init(void)
{
    debugBin_ResetParser();
}

static void debugBin_ParseByte(u8 data)
{
    debugBinParser_t *p;
    p = &g_debugBinParser;

    switch (p->state)
    {
    case DBIN_RX_SOF1:
        if (data == DEBUG_BIN_SOF1) p->state = DBIN_RX_SOF2;
        break;

    case DBIN_RX_SOF2:
        if (data == DEBUG_BIN_SOF2)
        {
            p->calculatedCrc = 0xFFFFU;
            p->payloadIndex = 0U;
            p->state = DBIN_RX_VERSION;
        }
        else if (data != DEBUG_BIN_SOF1) p->state = DBIN_RX_SOF1;
        break;

    case DBIN_RX_VERSION:
        p->version = data;
        p->calculatedCrc = debugBin_Crc16Update(p->calculatedCrc, data);
        p->state = DBIN_RX_TYPE;
        break;

    case DBIN_RX_TYPE:
        p->type = data;
        p->calculatedCrc = debugBin_Crc16Update(p->calculatedCrc, data);
        p->state = DBIN_RX_SEQ0;
        break;

    case DBIN_RX_SEQ0:
        p->sequence = data;
        p->calculatedCrc = debugBin_Crc16Update(p->calculatedCrc, data);
        p->state = DBIN_RX_SEQ1;
        break;

    case DBIN_RX_SEQ1:
        p->sequence |= (u16)data << 8;
        p->calculatedCrc = debugBin_Crc16Update(p->calculatedCrc, data);
        p->state = DBIN_RX_CMD0;
        break;

    case DBIN_RX_CMD0:
        p->command = data;
        p->calculatedCrc = debugBin_Crc16Update(p->calculatedCrc, data);
        p->state = DBIN_RX_CMD1;
        break;

    case DBIN_RX_CMD1:
        p->command |= (u16)data << 8;
        p->calculatedCrc = debugBin_Crc16Update(p->calculatedCrc, data);
        p->state = DBIN_RX_STATUS0;
        break;

    case DBIN_RX_STATUS0:
        p->requestStatus = data;
        p->calculatedCrc = debugBin_Crc16Update(p->calculatedCrc, data);
        p->state = DBIN_RX_STATUS1;
        break;

    case DBIN_RX_STATUS1:
        p->requestStatus |= (u16)data << 8;
        p->calculatedCrc = debugBin_Crc16Update(p->calculatedCrc, data);
        p->state = DBIN_RX_LEN0;
        break;

    case DBIN_RX_LEN0:
        p->payloadLength = data;
        p->calculatedCrc = debugBin_Crc16Update(p->calculatedCrc, data);
        p->state = DBIN_RX_LEN1;
        break;

    case DBIN_RX_LEN1:
        p->payloadLength |= (u16)data << 8;
        p->calculatedCrc = debugBin_Crc16Update(p->calculatedCrc, data);
        if (p->payloadLength > DEBUG_BIN_MAX_PAYLOAD)
        {
            debugBin_ResetParser();
        }
        else if (p->payloadLength == 0U) p->state = DBIN_RX_CRC0;
        else p->state = DBIN_RX_PAYLOAD;
        break;

    case DBIN_RX_PAYLOAD:
        p->payload[p->payloadIndex++] = data;
        p->calculatedCrc = debugBin_Crc16Update(p->calculatedCrc, data);
        if (p->payloadIndex >= p->payloadLength) p->state = DBIN_RX_CRC0;
        break;

    case DBIN_RX_CRC0:
        p->receivedCrc = data;
        p->state = DBIN_RX_CRC1;
        break;

    case DBIN_RX_CRC1:
        p->receivedCrc |= (u16)data << 8;
        if (p->receivedCrc == p->calculatedCrc)
        {
            debugBin_Dispatch();
        }
        else
        {
            debugBin_SendResponse(p->sequence, p->command, DEBUG_BIN_STATUS_CRC_ERROR, 0, 0U);
        }
        debugBin_ResetParser();
        break;

    default:
        debugBin_ResetParser();
        break;
    }
}

void debugBin_Task(void)
{
    u8 data;
    while (uart1_ReadByte(&data) != 0U)
    {
        debugBin_ParseByte(data);
    }
}
