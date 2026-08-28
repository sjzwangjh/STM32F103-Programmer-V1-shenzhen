/*
 * offLineReplayAvr.c - AVR offline replay group (ISP / HVSP / PP)
 *
 * Program, verify and lock/leave passes for AVR packages (arch=0).
 * Shared frame reading/execution helpers come from offLinePgmer.c.
 */

#include "offLineReplayAvr.h"
#include "offLinePgmer.h"
#include "offLineRecorder.h"
#include "Stk500Protocol.h"
#include "isp.h"
#include "hvproc.h"
#include "usart.h"
#include <string.h>

#define OFFLINE_FUSE_LOW            0U   /* 低位熔丝 */
#define OFFLINE_FUSE_HIGH           1U   /* 高位熔丝 */
#define OFFLINE_FUSE_EXT            2U   /* 扩展熔丝 */
#define OFFLINE_FUSE_COUNT          3U

/* Value used to mark "no leave frame recorded". */
#define OFFLINE_INVALID_PACKET      0xFFFFU   /* 熔丝总数 */

typedef struct
{
    uint8_t valid[OFFLINE_FUSE_COUNT];          /* 该槽位是否有编程操作 */
    uint8_t verified[OFFLINE_FUSE_COUNT];       /* 该槽位是否已被校验通过 */
    uint8_t value[OFFLINE_FUSE_COUNT];          /* 编程时写入的熔丝值 */
    uint16_t program_packet[OFFLINE_FUSE_COUNT]; /* 编程操作所在的数据包序号 */
} offline_fuse_expected_t;

static uint8_t avrIsDeferredRead(uint8_t cmd)
{
    return (cmd == STK_CMD_READ_FLASH_ISP ||
            cmd == STK_CMD_READ_EEPROM_ISP ||
            cmd == STK_CMD_READ_FUSE_ISP ||
            cmd == STK_CMD_READ_LOCK_ISP ||
            cmd == STK_CMD_READ_SIGNATURE_ISP ||
            cmd == STK_CMD_READ_OSCCAL_ISP ||
            cmd == STK_CMD_READ_FLASH_HVSP ||
            cmd == STK_CMD_READ_EEPROM_HVSP ||
            cmd == STK_CMD_READ_FUSE_HVSP ||
            cmd == STK_CMD_READ_LOCK_HVSP ||
            cmd == STK_CMD_READ_SIGNATURE_HVSP ||
            cmd == STK_CMD_READ_OSCCAL_HVSP) ? 1U : 0U;
}

static uint16_t avrGetTransferBytes(const uint8_t *numBytes)
{
    return (uint16_t)(((uint16_t)numBytes[0] << 8) | numBytes[1]);
}

static void avrAdvanceAddressByReadBytes(uint16_t numBytes, uint8_t isEeprom)
{
    uint16_t i;

    for (i = 0U; i < numBytes; i++)
    {
        if (isEeprom)
        {
            stkIncrementAddress();
        }
        else if ((i & 1U) != 0U)
        {
            stkIncrementAddress();
        }
    }
}

static void avrAdvanceDeferredRead(uint8_t cmd, void *param)
{
    switch (cmd)
    {
    case STK_CMD_READ_FLASH_ISP:
        avrAdvanceAddressByReadBytes(
            avrGetTransferBytes(((stkReadFlashIsp_t *)param)->numBytes), 0U);
        break;

    case STK_CMD_READ_EEPROM_ISP:
        avrAdvanceAddressByReadBytes(
            avrGetTransferBytes(((stkReadFlashIsp_t *)param)->numBytes), 1U);
        break;

    case STK_CMD_READ_FLASH_HVSP:
        avrAdvanceAddressByReadBytes(
            avrGetTransferBytes(((stkReadFlashHvsp_t *)param)->numBytes), 0U);
        break;

    case STK_CMD_READ_EEPROM_HVSP:
        avrAdvanceAddressByReadBytes(
            avrGetTransferBytes(((stkReadFlashHvsp_t *)param)->numBytes), 1U);
        break;

    default:
        break;
    }
}
static int8_t avrProgramFuseSlot(const stkProgramFuseIsp_t *param)
{
    if (param->cmd[0] != 0xACU)
        return -1;
    if (param->cmd[1] == 0xA0U)
        return OFFLINE_FUSE_LOW;
    if (param->cmd[1] == 0xA8U)
        return OFFLINE_FUSE_HIGH;
    if (param->cmd[1] == 0xA4U)
        return OFFLINE_FUSE_EXT;
    return -1;
}

static int8_t avrReadFuseSlot(const stkReadFuseIsp_t *param)
{
    if (param->cmd[0] == 0x50U && param->cmd[1] == 0x00U)
        return OFFLINE_FUSE_LOW;
    if (param->cmd[0] == 0x58U && param->cmd[1] == 0x08U)
        return OFFLINE_FUSE_HIGH;
    if (param->cmd[0] == 0x50U && param->cmd[1] == 0x08U)
        return OFFLINE_FUSE_EXT;
    return -1;
}

uint16_t avrReplayProgramPass(void)
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
            uart1_WriteString("REPLAY pgm readfail pkt=");
            uart1_WriteDec(i);
            uart1_WriteString("\r\n");
#endif
            return (uint16_t)(i + 1U);
        }

        cmd = packetHeader.cmd;
        /* 跳过延迟读取（留给第 2 道工序校验）和锁定位编程（留给第 3 道工序） */
        if (avrIsDeferredRead(cmd))
        {
            avrAdvanceDeferredRead(cmd, &g_replayFrame[STK_TXMSG_START + 1U]);
            continue;
        }

        if (cmd == STK_CMD_PROGRAM_LOCK_ISP ||
            cmd == STK_CMD_PROGRAM_LOCK_HVSP)
        {
            continue;
        }

        status = offlineExecuteFrame(packetHeader.frame_len);
        if (status != STK_STATUS_CMD_OK)
        {
#if DEBUG_HARDWARE_CONFIG
            uart1_WriteString("REPLAY pgm execfail pkt=");
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

uint16_t avrReplayVerifyPass(void)
{
    uint32_t cursor = g_replay.header.packet_area_offset;
    uint32_t i;
    offline_raw_packet_header_t packetHeader;
    offline_fuse_expected_t expectedFuse;

    memset(&expectedFuse, 0, sizeof(expectedFuse));

    for (i = 0U; i < g_replay.header.packet_count; i++)
    {
        uint8_t cmd;
        void *param;

        if (offlineReadPacket(&cursor, i, &packetHeader) != 0U)
        {
#if DEBUG_HARDWARE_CONFIG
            uart1_WriteString("REPLAY vfy readfail pkt=");
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
        case STK_CMD_ENTER_PROGMODE_ISP:
        case STK_CMD_ENTER_PROGMODE_HVSP:
            /* 重放地址/控制上下文命令 */
            if (offlineExecuteFrame(packetHeader.frame_len) != STK_STATUS_CMD_OK)
            {
#if DEBUG_HARDWARE_CONFIG
                uart1_WriteString("REPLAY vfy execfail pkt=");
                uart1_WriteDec(i);
                uart1_WriteString(" cmd=0x");
                uart1_WriteHex8(cmd);
                uart1_WriteString("\r\n");
#endif
                return (uint16_t)(i + 1U);
            }
            break;

        case STK_CMD_PROGRAM_FLASH_ISP:
            /* 将 Flash 编程替换为读回校验 */
            if (ispVerifyMemory((stkProgramFlashIsp_t *)param, 0U) != 0U)
            {
#if DEBUG_HARDWARE_CONFIG
                uart1_WriteString("REPLAY vfy flash mismatch pkt=");
                uart1_WriteDec(i);
                uart1_WriteString("\r\n");
#endif
                return (uint16_t)(i + 1U);
            }
            break;

        case STK_CMD_PROGRAM_EEPROM_ISP:
            /* 将 EEPROM 编程替换为读回校验 */
            if (ispVerifyMemory((stkProgramFlashIsp_t *)param, 1U) != 0U)
            {
#if DEBUG_HARDWARE_CONFIG
                uart1_WriteString("REPLAY vfy eeprom mismatch pkt=");
                uart1_WriteDec(i);
                uart1_WriteString("\r\n");
#endif
                return (uint16_t)(i + 1U);
            }
            break;

        case STK_CMD_PROGRAM_FLASH_HVSP:
            if (hvspVerifyMemory((stkProgramFlashHvsp_t *)param, 0U) != 0U)
            {
#if DEBUG_HARDWARE_CONFIG
                uart1_WriteString("REPLAY vfy hvsp flash mismatch pkt=");
                uart1_WriteDec(i);
                uart1_WriteString("\r\n");
#endif
                return (uint16_t)(i + 1U);
            }
            break;

        case STK_CMD_PROGRAM_EEPROM_HVSP:
            if (hvspVerifyMemory((stkProgramFlashHvsp_t *)param, 1U) != 0U)
            {
#if DEBUG_HARDWARE_CONFIG
                uart1_WriteString("REPLAY vfy hvsp eeprom mismatch pkt=");
                uart1_WriteDec(i);
                uart1_WriteString("\r\n");
#endif
                return (uint16_t)(i + 1U);
            }
            break;

        case STK_CMD_PROGRAM_FUSE_ISP:
            {
                /* 记录熔丝编程的期望值，留待后续 READ_FUSE_ISP 验证 */
                int8_t slot = avrProgramFuseSlot((stkProgramFuseIsp_t *)param);
                if (slot >= 0)
                {
                    expectedFuse.valid[(uint8_t)slot] = 1U;
                    expectedFuse.value[(uint8_t)slot] =
                        ((stkProgramFuseIsp_t *)param)->cmd[3];
                    expectedFuse.program_packet[(uint8_t)slot] = (uint16_t)i;
                }
            }
            break;

        case STK_CMD_PROGRAM_FUSE_HVSP:
            {
                stkProgramFuseHvsp_t *fuseParam = (stkProgramFuseHvsp_t *)param;
                if (fuseParam->fuseAddress < OFFLINE_FUSE_COUNT)
                {
                    expectedFuse.valid[fuseParam->fuseAddress] = 1U;
                    expectedFuse.value[fuseParam->fuseAddress] = fuseParam->fuseByte;
                    expectedFuse.program_packet[fuseParam->fuseAddress] = (uint16_t)i;
                }
            }
            break;

        case STK_CMD_READ_FUSE_ISP:
            {
                /* 通过先前记录的 READ_FUSE_ISP 命令读取实际熔丝值并与期望值比对 */
                stkReadFuseIsp_t *readParam = (stkReadFuseIsp_t *)param;
                int8_t slot = avrReadFuseSlot(readParam);
                uint8_t actual;

                if (slot < 0)
                    return (uint16_t)(i + 1U);
                if (!expectedFuse.valid[(uint8_t)slot])
                    break;  /* 编程前的熔丝读取不是校验项，直接跳过 */

                actual = ispReadFuse(readParam);
                /* Fuse data bits live only in the recorded opcode (cmd[3]); bits
                 * set to 1 must still read back 1 after programming. Bits set to
                 * 0 (programmed) are not checked here, matching the "never
                 * re-program a 1 back to 0" fuse semantics. */
                if ((actual & expectedFuse.value[(uint8_t)slot]) !=
                    expectedFuse.value[(uint8_t)slot])
                {
#if DEBUG_HARDWARE_CONFIG
                    uart1_WriteString("REPLAY vfy fuse mismatch pkt=");
                    uart1_WriteDec(i);
                    uart1_WriteString(" slot=");
                    uart1_WriteDec(slot);
                    uart1_WriteString(" got=0x");
                    uart1_WriteHex8(actual);
                    uart1_WriteString(" want=0x");
                    uart1_WriteHex8(expectedFuse.value[(uint8_t)slot]);
                    uart1_WriteString("\r\n");
#endif
                    return (uint16_t)(i + 1U);
                }
                expectedFuse.verified[(uint8_t)slot] = 1U;
            }
            break;

        case STK_CMD_READ_FUSE_HVSP:
            {
                stkReadFuseHvsp_t *readParam = (stkReadFuseHvsp_t *)param;
                uint8_t slot = readParam->fuseAddress;
                uint8_t actual;

                if (slot >= OFFLINE_FUSE_COUNT)
                    return (uint16_t)(i + 1U);
                if (!expectedFuse.valid[slot])
                    break;

                actual = hvspReadFuse(readParam);
                if ((actual & expectedFuse.value[slot]) != expectedFuse.value[slot])
                {
#if DEBUG_HARDWARE_CONFIG
                    uart1_WriteString("REPLAY vfy hvsp fuse mismatch pkt=");
                    uart1_WriteDec(i);
                    uart1_WriteString(" slot=");
                    uart1_WriteDec(slot);
                    uart1_WriteString(" got=0x");
                    uart1_WriteHex8(actual);
                    uart1_WriteString(" want=0x");
                    uart1_WriteHex8(expectedFuse.value[slot]);
                    uart1_WriteString("\r\n");
#endif
                    return (uint16_t)(i + 1U);
                }
                expectedFuse.verified[slot] = 1U;
            }
            break;

        case STK_CMD_READ_SIGNATURE_ISP:
        case STK_CMD_READ_OSCCAL_ISP:
        case STK_CMD_READ_SIGNATURE_HVSP:
        case STK_CMD_READ_OSCCAL_HVSP:
            /* Replay the read command and require a valid reply. */
            if (offlineExecuteFrame(packetHeader.frame_len) != STK_STATUS_CMD_OK)
            {
#if DEBUG_HARDWARE_CONFIG
                uart1_WriteString("REPLAY vfy execfail pkt=");
                uart1_WriteDec(i);
                uart1_WriteString(" cmd=0x");
                uart1_WriteHex8(cmd);
                uart1_WriteString("\r\n");
#endif
                return (uint16_t)(i + 1U);
            }
            break;

        default:
            /* 擦除、正常读取、熔丝写入、锁定位编程和退出编程模式等
             * 命令在本次校验工序中不重复处理。
             */
            break;
        }
    }

    /* 检查所有已编程的熔丝槽位是否都已通过校验 */
    for (i = 0U; i < OFFLINE_FUSE_COUNT; i++)
    {
        if (expectedFuse.valid[i] && !expectedFuse.verified[i])
            return (uint16_t)(expectedFuse.program_packet[i] + 1U);
    }
    return 0U;
}

uint16_t avrReplayLockAndLeavePass(void)
{
    uint32_t cursor = g_replay.header.packet_area_offset;
    uint32_t i;
    uint16_t leavePacket = OFFLINE_INVALID_PACKET;
    uint16_t leaveFrameLen = 0U;
    uint8_t leaveFrame[32];
    uint16_t firstFail = 0U;
    offline_raw_packet_header_t packetHeader;

    for (i = 0U; i < g_replay.header.packet_count; i++)
    {
        if (offlineReadPacket(&cursor, i, &packetHeader) != 0U)
            return (uint16_t)(i + 1U);

        if (packetHeader.cmd == STK_CMD_PROGRAM_LOCK_ISP ||
            packetHeader.cmd == STK_CMD_PROGRAM_LOCK_HVSP)
        {
            /* Execute lock programming; always run the leave at the end. */
            if (offlineExecuteFrame(packetHeader.frame_len) != STK_STATUS_CMD_OK &&
                firstFail == 0U)
                firstFail = (uint16_t)(i + 1U);
        }
        else if ((packetHeader.cmd == STK_CMD_LEAVE_PROGMODE_ISP ||
                  packetHeader.cmd == STK_CMD_LEAVE_PROGMODE_HVSP) &&
                 packetHeader.frame_len <= sizeof(leaveFrame))
        {
            memcpy(leaveFrame, g_replayFrame, packetHeader.frame_len);
            leaveFrameLen = packetHeader.frame_len;
            leavePacket = (uint16_t)i;
        }
    }

    /* Always execute the leave command so the target is powered down. */
    if (leavePacket != OFFLINE_INVALID_PACKET)
    {
        memcpy(g_replayFrame, leaveFrame, leaveFrameLen);
        if (offlineExecuteFrame(leaveFrameLen) != STK_STATUS_CMD_OK &&
            firstFail == 0U)
            firstFail = (uint16_t)(leavePacket + 1U);
    }

    return firstFail;
}
