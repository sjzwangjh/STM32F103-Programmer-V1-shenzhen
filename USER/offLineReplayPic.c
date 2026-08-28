/*
 * offLineReplayPic.c - PIC offline replay group (ICSP)
 *
 * Program, verify and lock/leave passes for PIC packages (arch=1).
 * Shared frame reading/execution helpers come from offLinePgmer.c.
 * Flow mirrors the AVR (ISP) replay group in offLineReplayAvr.c:
 *   program pass executes real writes, verify pass re-enters and
 *   compares read-back data, lock/leave pass always powers down.
 */

#include "offLineReplayPic.h"
#include "offLinePgmer.h"
#include "offLineRecorder.h"
#include "Stk500Protocol.h"
#include "icsp.h"
#include "picDeviceConst.h"
#include "usart.h"
#include <string.h>

#define PIC_USERID_WORDS_MAX        8U

/* Read commands are deferred to the verify pass. */
static uint8_t picIsDeferredRead(uint8_t cmd)
{
    return (cmd == STK_CMD_READ_FLASH_ICSP ||
            cmd == STK_CMD_READ_EEPROM_ICSP ||
            cmd == STK_CMD_READ_CONFIG_ICSP ||
            cmd == STK_CMD_READ_USER_ID_ICSP ||
            cmd == STK_CMD_READ_SIGNATURE_ICSP ||
            cmd == STK_CMD_READ_OSCCAL_ICSP) ? 1U : 0U;
}

/* Read-back scratch must be large enough for the largest ICSP block
 * (stkReadFlashIcspResult_t only carries one data byte). */
static uint8_t picVerifyBuf[BUFFER_SIZE];

/* Read back the block recorded by a PROGRAM_FLASH/EEPROM frame and compare.
 * Returns 0 on match, non-zero on mismatch/failure. */
static int8_t picVerifyMemory(stkProgramFlashIcsp_t *param, uint8_t isEeprom)
{
    stkReadFlashIcspResult_t *res = (stkReadFlashIcspResult_t *)picVerifyBuf;
    uint16_t count = (uint16_t)(param->numWords[0] | (param->numWords[1] << 8));
    uint16_t cmpLen;

    memset(picVerifyBuf, 0, sizeof(picVerifyBuf));
    if (icspReadMemory((stkReadFlashIcsp_t *)param, res, isEeprom) == 0U)
        return 1;
    if (res->status1 != STK_STATUS_CMD_OK)
        return 1;
    cmpLen = isEeprom ? count : (uint16_t)(count * 2U);
    return (memcmp(res->data, param->data, cmpLen) == 0) ? 0 : 1;
}

uint16_t picReplayProgramPass(void)
{
    uint32_t cursor = g_replay.header.packet_area_offset;
    uint32_t i;
    offline_raw_packet_header_t packetHeader;

    for (i = 0U; i < g_replay.header.packet_count; i++)
    {
        uint8_t cmd;
        uint8_t status;

        if (offlineReadPacket(&cursor, i, &packetHeader) != 0U)
        {
#if DEBUG_HARDWARE_CONFIG
            uart1_WriteString("REPLAY pic pgm readfail pkt=");
            uart1_WriteDec(i);
            uart1_WriteString("\r\n");
#endif
            return (uint16_t)(i + 1U);
        }

        cmd = packetHeader.cmd;
        if (picIsDeferredRead(cmd))
            continue;

        status = offlineExecuteFrame(packetHeader.frame_len);
        if (status != STK_STATUS_CMD_OK)
        {
#if DEBUG_HARDWARE_CONFIG
            uart1_WriteString("REPLAY pic pgm execfail pkt=");
            uart1_WriteDec(i);
            uart1_WriteString(" cmd=0x");
            uart1_WriteHex8(cmd);
            uart1_WriteString("\r\n");
#endif
            return (uint16_t)(i + 1U);
        }
    }
    return 0U;
}

uint16_t picReplayVerifyPass(void)
{
    uint32_t cursor = g_replay.header.packet_area_offset;
    uint32_t i;
    offline_raw_packet_header_t packetHeader;
    uint16_t expectedCfg[MAX_CONFIG_WORDS];
    uint8_t  cfgValid[MAX_CONFIG_WORDS];
    uint16_t expectedUid[PIC_USERID_WORDS_MAX];
    uint8_t  uidValid[PIC_USERID_WORDS_MAX];
    pic_prog_params_t picParams;
    uint8_t  havePicParams = 0U;

    memset(expectedCfg, 0, sizeof(expectedCfg));
    memset(cfgValid, 0, sizeof(cfgValid));
    memset(expectedUid, 0, sizeof(expectedUid));
    memset(uidValid, 0, sizeof(uidValid));
    memset(&picParams, 0, sizeof(picParams));
    if (g_replay.header.identity.arch == STK_MCU_ARCH_PIC &&
        pic8FindDeviceByIndex(g_replay.header.identity.index, &picParams) == 0)
    {
        havePicParams = 1U;
    }

    for (i = 0U; i < g_replay.header.packet_count; i++)
    {
        uint8_t cmd;
        void *param;

        if (offlineReadPacket(&cursor, i, &packetHeader) != 0U)
        {
#if DEBUG_HARDWARE_CONFIG
            uart1_WriteString("REPLAY pic vfy readfail pkt=");
            uart1_WriteDec(i);
            uart1_WriteString("\r\n");
#endif
            return (uint16_t)(i + 1U);
        }

        cmd = packetHeader.cmd;
        param = &g_replayFrame[STK_TXMSG_START + 1U];

        switch (cmd)
        {
        case STK_CMD_SET_PARAMETER:
        case STK_CMD_LOAD_ADDRESS:
        case STK_CMD_ENTER_PROGMODE_ICSP:
            if (offlineExecuteFrame(packetHeader.frame_len) != STK_STATUS_CMD_OK)
            {
#if DEBUG_HARDWARE_CONFIG
                uart1_WriteString("REPLAY pic vfy execfail pkt=");
                uart1_WriteDec(i);
                uart1_WriteString(" cmd=0x");
                uart1_WriteHex8(cmd);
                uart1_WriteString("\r\n");
#endif
                return (uint16_t)(i + 1U);
            }
            break;

        case STK_CMD_PROGRAM_FLASH_ICSP:
            if (picVerifyMemory((stkProgramFlashIcsp_t *)param, 0U) != 0)
            {
#if DEBUG_HARDWARE_CONFIG
                uart1_WriteString("REPLAY pic vfy flash mismatch pkt=");
                uart1_WriteDec(i);
                uart1_WriteString("\r\n");
#endif
                return (uint16_t)(i + 1U);
            }
            break;

        case STK_CMD_PROGRAM_EEPROM_ICSP:
            if (picVerifyMemory((stkProgramFlashIcsp_t *)param, 1U) != 0)
            {
#if DEBUG_HARDWARE_CONFIG
                uart1_WriteString("REPLAY pic vfy eeprom mismatch pkt=");
                uart1_WriteDec(i);
                uart1_WriteString("\r\n");
#endif
                return (uint16_t)(i + 1U);
            }
            break;

        case STK_CMD_PROGRAM_CONFIG_ICSP:
            {
                stkProgramFlashIcsp_t *cfgParam = (stkProgramFlashIcsp_t *)param;
                uint16_t n = (uint16_t)(cfgParam->numWords[0] | (cfgParam->numWords[1] << 8));
                uint16_t w;
                for (w = 0U; w < n && w < MAX_CONFIG_WORDS; w++)
                {
                    expectedCfg[w] = (uint16_t)(cfgParam->data[w * 2U] |
                                                (cfgParam->data[w * 2U + 1U] << 8));
                    cfgValid[w] = 1U;
                }
            }
            break;

        case STK_CMD_READ_CONFIG_ICSP:
            {
                stkReadFlashIcsp_t *readParam = (stkReadFlashIcsp_t *)param;
                uint16_t cfgCount = (uint16_t)(readParam->numWords[0] |
                                               (readParam->numWords[1] << 8));
                uint16_t w;
                for (w = 0U; w < cfgCount && w < MAX_CONFIG_WORDS; w++)
                {
                    uint16_t actual = icspReadCfg(w);
                    uint16_t expect = expectedCfg[w];
                    uint16_t implMask = 0xFFFFU;
                    if (havePicParams != 0U && w < picParams.common.config_word_count)
                    {
                        if (picParams.common.config_dcr[w].impl_mask != 0U)
                            implMask = picParams.common.config_dcr[w].impl_mask;
                    }
                    if (cfgValid[w] &&
                        ((uint16_t)(actual & implMask) != (uint16_t)(expect & implMask)))
                    {
#if DEBUG_HARDWARE_CONFIG
                        uart1_WriteString("REPLAY pic vfy cfg mismatch pkt=");
                        uart1_WriteDec(i);
                        uart1_WriteString(" idx=");
                        uart1_WriteDec(w);
                        uart1_WriteString(" mask=0x");
                        uart1_WriteHex16(implMask);
                        uart1_WriteString(" got=0x");
                        uart1_WriteHex16(actual);
                        uart1_WriteString(" want=0x");
                        uart1_WriteHex16(expect);
                        uart1_WriteString("\r\n");
#endif
                        return (uint16_t)(i + 1U);
                    }
                }
            }
            break;

        case STK_CMD_PROGRAM_USER_ID_ICSP:
            {
                stkProgramFlashIcsp_t *uidParam = (stkProgramFlashIcsp_t *)param;
                uint16_t n = (uint16_t)(uidParam->numWords[0] | (uidParam->numWords[1] << 8));
                uint16_t w;
                for (w = 0U; w < n && w < PIC_USERID_WORDS_MAX; w++)
                {
                    expectedUid[w] = (uint16_t)(uidParam->data[w * 2U] |
                                                (uidParam->data[w * 2U + 1U] << 8));
                    uidValid[w] = 1U;
                }
            }
            break;

        case STK_CMD_READ_USER_ID_ICSP:
            {
                stkReadFlashIcsp_t *readParam = (stkReadFlashIcsp_t *)param;
                uint16_t uidCount = (uint16_t)(readParam->numWords[0] |
                                               (readParam->numWords[1] << 8));
                uint8_t buf[PIC_USERID_WORDS_MAX * 2U];
                uint16_t w;
                if (uidCount > PIC_USERID_WORDS_MAX)
                    uidCount = PIC_USERID_WORDS_MAX;
                if (icspReadUserIdWords(stkAddress.dword, buf, uidCount) != ICSP_OK)
                {
#if DEBUG_HARDWARE_CONFIG
                    uart1_WriteString("REPLAY pic vfy uid readfail pkt=");
                    uart1_WriteDec(i);
                    uart1_WriteString("\r\n");
#endif
                    return (uint16_t)(i + 1U);
                }
                for (w = 0U; w < uidCount; w++)
                {
                    uint16_t actual = (uint16_t)(buf[w * 2U] | (buf[w * 2U + 1U] << 8));
                    if (uidValid[w] && actual != expectedUid[w])
                    {
#if DEBUG_HARDWARE_CONFIG
                        uart1_WriteString("REPLAY pic vfy uid mismatch pkt=");
                        uart1_WriteDec(i);
                        uart1_WriteString(" idx=");
                        uart1_WriteDec(w);
                        uart1_WriteString(" got=0x");
                        uart1_WriteHex16(actual);
                        uart1_WriteString(" want=0x");
                        uart1_WriteHex16(expectedUid[w]);
                        uart1_WriteString("\r\n");
#endif
                        return (uint16_t)(i + 1U);
                    }
                }
            }
            break;

        case STK_CMD_READ_SIGNATURE_ICSP:
            {
                uint8_t status;
#if DEBUG_HARDWARE_CONFIG
                uart1_WriteString("REPLAY pic vfy sig pkt=");
                uart1_WriteDec(i);
                uart1_WriteString("\r\n");
#endif
                status = offlineExecuteFrame(packetHeader.frame_len);
                if (status != STK_STATUS_CMD_OK)
                {
#if DEBUG_HARDWARE_CONFIG
                    uart1_WriteString("REPLAY pic vfy execfail pkt=");
                    uart1_WriteDec(i);
                    uart1_WriteString(" cmd=0x");
                    uart1_WriteHex8(cmd);
                    uart1_WriteString(" status=0x");
                    uart1_WriteHex8(status);
                    uart1_WriteString("\r\n");
#endif
                    return (uint16_t)(i + 1U);
                }
            }
            break;

        case STK_CMD_READ_OSCCAL_ICSP:
            {
                uint8_t status;
                status = offlineExecuteFrame(packetHeader.frame_len);
                if (status != STK_STATUS_CMD_OK)
                {
#if DEBUG_HARDWARE_CONFIG
                    uart1_WriteString("REPLAY pic vfy execfail pkt=");
                    uart1_WriteDec(i);
                    uart1_WriteString(" cmd=0x");
                    uart1_WriteHex8(cmd);
                    uart1_WriteString(" status=0x");
                    uart1_WriteHex8(status);
                    uart1_WriteString("\r\n");
#endif
                    return (uint16_t)(i + 1U);
                }
            }
            break;

        default:
            break;
        }
    }
    return 0U;
}
uint16_t picReplayLockAndLeavePass(void)
{
    uint32_t cursor = g_replay.header.packet_area_offset;
    uint32_t i;
    uint16_t result = 0U;
    uint16_t leavePacket = 0xFFFFU;
    uint16_t leaveFrameLen = 0U;
    uint8_t leaveFrame[32];
    offline_raw_packet_header_t packetHeader;

    for (i = 0U; i < g_replay.header.packet_count; i++)
    {
        if (offlineReadPacket(&cursor, i, &packetHeader) != 0U)
            return (uint16_t)(i + 1U);

        if (packetHeader.cmd == STK_CMD_LEAVE_PROGMODE_ICSP &&
            packetHeader.frame_len <= sizeof(leaveFrame))
        {
            memcpy(leaveFrame, g_replayFrame, packetHeader.frame_len);
            leaveFrameLen = packetHeader.frame_len;
            leavePacket = (uint16_t)i;
        }
    }

    /* Always execute the leave command so the target is powered down. */
    if (leavePacket != 0xFFFFU)
    {
        memcpy(g_replayFrame, leaveFrame, leaveFrameLen);
        if (offlineExecuteFrame(leaveFrameLen) != STK_STATUS_CMD_OK)
            result = (uint16_t)(leavePacket + 1U);
    }

    /* Always force the ICSP engine back to idle so the next replay does not
     * depend on a clean LEAVE frame or a power-cycle. */
    pic8LeaveProgmode();
#if DEBUG_HARDWARE_CONFIG
    uart1_WriteString("REPLAY pic force leave\r\n");
#endif

    return result;
}
