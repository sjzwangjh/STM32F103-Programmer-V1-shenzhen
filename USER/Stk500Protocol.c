/*
 * STK500v2 协议解析模块
 *
 * 本模块负责解析上位机发送的 STK500v2 数据帧, 并把命令分发到
 * AVR ISP/HVSP/HVPP、PIC ICSP 以及离线数据包记录模块。
 *
 * 数据来源支持两类: USB HID 在线通讯和 Flash 离线回放。
 * stkEvaluateRxMessage() 通过 stkDataFrame_t 同时接收 RX/TX 缓冲,
 * 从而避免在线 TX 和离线判定 TX 互相交叉。
 */

#include "Stk500Protocol.h"
#include <string.h>
#include "timer.h"
#include "isp.h"
#include "hvproc.h"
#include "icsp.h"
#include "offLineRecorder.h"
#include "eeprom.h"
#include "usart.h"
#include "usb_winusb_user.h"

/* 上报给上位机的 STK500 版本号。 */
#define STK_VERSION_HW      1
#define STK_VERSION_MAJOR   2
#define STK_VERSION_MINOR   4

/* USB HID 接收链使用的全局 RX 缓冲, 只保存当前在线收到的一帧。 */
static uint8_t      rxBuffer[BUFFER_SIZE];
static uint16_t     rxPos;
static utilWord_t   rxLen;
static uint8_t      rxBlockAvailable;

/* USB HID 发送链使用的全局 TX 缓冲。Flash 回放使用调用方传入的独立 TX 缓冲。 */
static uint8_t      txBuffer[BUFFER_SIZE];
static uint16_t     txPos, txLen;

stkParam_t      stkParam = {{
                    0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0,
                    STK_VERSION_HW, STK_VERSION_MAJOR, STK_VERSION_MINOR, 0, 50, 0, 1, 0x80,
                    2, 0, 0xaa, 0, 0, 0, 0, 0,
                }};
utilDword_t     stkAddress;

/* 全局变量: 记录上位机下发的器件和项目身份信息。 */
static stkDeviceIdentity_t g_stkDeviceIdentity;

/* 从小端字节流中读取 16 位数据。 */
static uint16_t stkGetLe16(const uint8_t *bytes);
/* 将 16 位数据按小端格式写入字节流。 */
static void stkPutLe16(uint8_t *bytes, uint16_t value);
/* 保存上位机下发的器件和项目身份信息, 后续会写入 Raw 离线包头。 */
static uint8_t stkSetDeviceIdentity(const uint8_t *payload, uint16_t payloadLen);
/* 将当前器件和项目身份信息打包返回给上位机。 */
static uint16_t stkGetDeviceIdentity(uint8_t *out, uint16_t outSize);
/* 打包离线包总体信息: 有效包数量、激活包序号、最大包数量。 */
static uint16_t stkPutOfflineInfo(uint8_t *out, uint16_t outSize);
/* 打包指定离线包摘要, 供上位机查看 Flash 中的记录内容。 */
static uint16_t stkPutOfflineSummary(uint8_t *out, uint16_t outSize, uint16_t index);

/* 保留 AVR-Doper 的 switch 宏风格, 便于和原始协议分发结构对照。 */
#define SWITCH_START        switch(cmd){{
#define SWITCH_CASE(value)  }break; case (value):{
#define SWITCH_CASE2(v1,v2) }break; case (v1): case(v2):{
#define SWITCH_CASE3(v1,v2,v3) }break; case (v1): case(v2): case(v3):{
#define SWITCH_CASE4(v1,v2,v3,v4) }break; case (v1): case(v2): case(v3): case(v4):{
#define SWITCH_DEFAULT      }break; default:{
#define SWITCH_END          }}

/* 离线模式下的记录状态: 0=空闲(IDLE), 1=记录中 */
uint8_t g_stkProgrammerState = STK500_PROGRAM_IDLE;

/* Firmware-upgrade request flag: set after the EEPROM boot-mode flag is written and read back OK. */
static volatile uint8_t g_stkFwUpgradePending;

uint8_t stkFwUpgradeRequested(void)
{
    return g_stkFwUpgradePending;
}

/* 从小端字节流中读取 16 位数据。 */
static uint16_t stkGetLe16(const uint8_t *bytes)
{
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
}

/* 将 16 位数据按小端格式写入字节流。 */
static void stkPutLe16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)(value & 0xFFU);
    bytes[1] = (uint8_t)(value >> 8);
}

/* 保存上位机下发的器件和项目身份信息, 后续会写入 Raw 离线包头。 */
static uint8_t stkSetDeviceIdentity(const uint8_t *payload, uint16_t payloadLen)
{
    if (payload == NULL)
        return STK_STATUS_CMD_FAILED;

    if (payloadLen < (uint16_t)(1U + 2U + STK_DEVICE_ITEM_ID_LEN + STK_DEVICE_ITEM_DESC_LEN))
        return STK_STATUS_CMD_FAILED;

    g_stkDeviceIdentity.arch = payload[0];
    g_stkDeviceIdentity.index = stkGetLe16(&payload[1]);
    memcpy(g_stkDeviceIdentity.itemId, &payload[3], STK_DEVICE_ITEM_ID_LEN);
    memcpy(g_stkDeviceIdentity.itemDesc,
           &payload[3 + STK_DEVICE_ITEM_ID_LEN],
           STK_DEVICE_ITEM_DESC_LEN);
    g_stkDeviceIdentity.itemDesc[STK_DEVICE_ITEM_DESC_LEN] = '\0';
    return STK_STATUS_CMD_OK;
}

/* 将当前器件和项目身份信息打包返回给上位机。 */
static uint16_t stkGetDeviceIdentity(uint8_t *out, uint16_t outSize)
{
    uint16_t needLen;

    if (out == NULL)
        return 0U;

    needLen = (uint16_t)(1U + 2U + STK_DEVICE_ITEM_ID_LEN + STK_DEVICE_ITEM_DESC_LEN);
    if (outSize < needLen)
        return 0U;

    out[0] = g_stkDeviceIdentity.arch;
    stkPutLe16(&out[1], g_stkDeviceIdentity.index);
    memcpy(&out[3], g_stkDeviceIdentity.itemId, STK_DEVICE_ITEM_ID_LEN);
    memcpy(&out[3 + STK_DEVICE_ITEM_ID_LEN],
           g_stkDeviceIdentity.itemDesc,
           STK_DEVICE_ITEM_DESC_LEN);
    return needLen;
}

static void stkPutLe32(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)(value & 0xFFU);
    bytes[1] = (uint8_t)((value >> 8) & 0xFFU);
    bytes[2] = (uint8_t)((value >> 16) & 0xFFU);
    bytes[3] = (uint8_t)((value >> 24) & 0xFFU);
}

/* 打包离线包总体信息: 有效包数量、激活包序号、最大包数量。 */
static uint16_t stkPutOfflineInfo(uint8_t *out, uint16_t outSize)
{
    offline_package_info_t info;

    if (out == 0 || outSize < 6U)
        return 0U;

    if (offlinePgmerGetOfflineInfo(&info) != 0U)
        return 0U;

    stkPutLe16(&out[0], info.package_count);
    stkPutLe16(&out[2], info.active_index);
    stkPutLe16(&out[4], info.max_count);
    return 6U;
}

/* 打包指定离线包摘要, 供上位机查看 Flash 中的记录内容。 */
static uint16_t stkPutOfflineSummary(uint8_t *out, uint16_t outSize, uint16_t index)
{
    offline_package_index_t summary;
    uint16_t pos = 0U;

    if (out == 0 || outSize < 93U)
        return 0U;

    if (offlinePgmerGetPackageSummary(index, &summary) != 0U)
        return 0U;

    out[pos++] = summary.used;
    out[pos++] = summary.package_state;
    stkPutLe16(&out[pos], summary.package_index); pos += 2U;
    stkPutLe32(&out[pos], summary.flash_addr); pos += 4U;
    stkPutLe32(&out[pos], summary.total_size); pos += 4U;
    stkPutLe32(&out[pos], summary.packet_count); pos += 4U;
    stkPutLe32(&out[pos], summary.crc32); pos += 4U;
    out[pos++] = summary.identity.arch;
    stkPutLe16(&out[pos], summary.identity.index); pos += 2U;
    memcpy(&out[pos], summary.identity.itemId, STK_DEVICE_ITEM_ID_LEN);
    pos += STK_DEVICE_ITEM_ID_LEN;
    memcpy(&out[pos], summary.identity.itemDesc, STK_DEVICE_ITEM_DESC_LEN);
    pos += STK_DEVICE_ITEM_DESC_LEN;
    return pos;
}

/* 按当前 STK 地址写入 PIC Flash, 成功后同步推进协议层地址。 */
static uint8_t stkIcspProgramFlash(const stkProgramFlashIcsp_t *param)
{
    uint16_t wordCount;
    uint16_t i;

    if (param == NULL)
        return STK_STATUS_CMD_FAILED;

    wordCount = stkGetLe16(param->numWords);
    if (icspSetProgramAddress(stkAddress.dword) != ICSP_OK)
        return STK_STATUS_CMD_FAILED;

    for (i = 0; i < wordCount; i++)
    {
        uint16_t word = stkGetLe16(&param->data[i * 2U]);
        if (icspProgWord(word) != ICSP_OK)
            return STK_STATUS_CMD_FAILED;
        if (i + 1U < wordCount)
            stkAddress.dword++;
    }
    if (wordCount != 0U)
        stkAddress.dword++;
    return STK_STATUS_CMD_OK;
}

/* 按当前 STK 地址写入 PIC Flash, 成功后同步推进协议层地址。 */
static uint16_t stkIcspReadFlash(const stkReadFlashIcsp_t *param, uint8_t *out)
{
    uint16_t wordCount;
    uint16_t i;

    if (param == NULL || out == NULL)
        return 0U;

    wordCount = stkGetLe16(param->numWords);
    if (icspSetProgramAddress(stkAddress.dword) != ICSP_OK)
    {
        out[0] = STK_STATUS_CMD_FAILED;
        return 1U;
    }

    out[0] = STK_STATUS_CMD_OK;
    for (i = 0; i < wordCount; i++)
    {
        uint16_t word = icspReadWord();
        stkPutLe16(&out[1 + i * 2U], word);
        if (i + 1U < wordCount)
            stkAddress.dword++;
    }
    if (wordCount != 0U)
        stkAddress.dword++;
    return (uint16_t)(1U + wordCount * 2U);
}

/* 按当前 STK 地址写入 PIC EEPROM, 成功后同步推进协议层地址。 */
static uint8_t stkIcspProgramEeprom(const stkProgramEepromIcsp_t *param)
{
    uint16_t byteCount;
    uint16_t i;

    if (param == NULL)
        return STK_STATUS_CMD_FAILED;
    byteCount = stkGetLe16(param->numBytes);
    if (icspSetEeAddress(stkAddress.dword) != ICSP_OK)
        return STK_STATUS_CMD_FAILED;

    for (i = 0; i < byteCount; i++)
    {
        if (icspProgEE(param->data[i]) != ICSP_OK)
            return STK_STATUS_CMD_FAILED;
        if (i + 1U < byteCount)
            stkAddress.dword++;
    }
    if (byteCount != 0U)
        stkAddress.dword++;
    return STK_STATUS_CMD_OK;
}

/* 按当前 STK 地址写入 PIC EEPROM, 成功后同步推进协议层地址。 */
static uint16_t stkIcspReadEeprom(const stkReadEepromIcsp_t *param, uint8_t *out)
{
    uint16_t byteCount;
    uint16_t i;
    uint8_t value;

    if (param == NULL || out == NULL)
        return 0U;
    byteCount = stkGetLe16(param->numBytes);
    if (icspSetEeAddress(stkAddress.dword) != ICSP_OK)
    {
        out[0] = STK_STATUS_CMD_FAILED;
        return 1U;
    }

    out[0] = STK_STATUS_CMD_OK;
    for (i = 0; i < byteCount; i++)
    {
        if (icspReadEE(&value) != ICSP_OK)
        {
            out[0] = STK_STATUS_CMD_FAILED;
            return 1U;
        }
        out[1 + i] = value;
        if (i + 1U < byteCount)
            stkAddress.dword++;
    }
    if (byteCount != 0U)
        stkAddress.dword++;
    return (uint16_t)(1U + byteCount);
}

/* 根据 STK500 payload 生成完整 TX 帧, 输出到调用方指定的 TX 缓冲。 */
static uint16_t stkSetTxMessage(uint8_t *out, uint16_t outSize, uint16_t len, uint8_t seq)
{
    uint8_t *p;
    uint8_t sum = 0;
    uint16_t frameLen;
    uint16_t remain;

    if (out == 0)
        return 0U;

    frameLen = (uint16_t)(len + 6U);
    if (outSize < frameLen)
        return 0U;

    p = out;
    *p++ = STK_STX;
    *p++ = seq;
    *p++ = (uint8_t)(len >> 8);
    *p++ = (uint8_t)len;
    *p++ = STK_TOKEN;

    remain = (uint16_t)(frameLen - 1U);
    p = out;
    while (remain--)
        sum ^= *p++;
    *p = sum;

    return frameLen;
}

static uint8_t stkProgramStatus(uint8_t onlineStatus, uint8_t recordStatus)
{
    if (onlineStatus != STK_STATUS_CMD_OK)
        return onlineStatus;
    return recordStatus;
}

static uint8_t setParameter(uint8_t index, uint8_t value)
{
    index &= 0x1f;
    stkParam.bytes[index] = value;
    return STK_STATUS_CMD_OK;
}

static uint8_t getParameter(uint8_t index)
{
    index &= 0x1f;
    return stkParam.bytes[index];
}

/* 解析一帧完整 STK500 数据。USB 来源会进入 HID TX, Flash 来源只生成本地 TX 判定结果。 */
void stkEvaluateRxMessage(stkDataFrame_t *pDataFrame)
{
    uint8_t     i, cmd;
    utilWord_t  len = {2};
    void        *param;
    const uint8_t *pRx;
    uint8_t *pTx;
    uint16_t payloadLen;

    if (pDataFrame == 0 || pDataFrame->frame == 0 || pDataFrame->frameLen < 6U ||
        pDataFrame->txFrame == 0 || pDataFrame->txFrameSize < 6U)
        return;

    pDataFrame->txFrameLen = 0U;

    pRx = pDataFrame->frame;
    pTx = pDataFrame->txFrame;
    payloadLen = ((uint16_t)pRx[2] << 8) | pRx[3];
    if (pDataFrame->frameLen < (uint16_t)(payloadLen + 6U))
        return;

#if DEBUG_HARDWARE_CONFIG
    /* 调试: 将收到的数据包内容转换成十六进制字符串通过 UART1 发送 */
    if (pDataFrame->source == STK_DATA_SOURCE_USB_HID ||
        pDataFrame->source == STK_DATA_SOURCE_USB_CDC)
    {
        static const char hexTable[] = "0123456789ABCDEF";
        uint16_t i;
        uart1_WriteString("Rece:");
        for (i = 0; i < pDataFrame->frameLen; i++)
        {
            uart1_WriteByte((uint8_t)hexTable[(pRx[i] >> 4) & 0x0F]);
            uart1_WriteByte((uint8_t)hexTable[pRx[i] & 0x0F]);
            uart1_WriteByte(' ');
        }
        uart1_WriteString((const char*)"\r\n");
    }
#endif

    cmd = pRx[STK_TXMSG_START];
    pTx[STK_TXMSG_START] = cmd;
    pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_OK;
    param = (void *)(pRx + STK_TXMSG_START + 1);

    if ((pDataFrame->source == STK_DATA_SOURCE_USB_HID ||
         pDataFrame->source == STK_DATA_SOURCE_USB_CDC) &&
        stkIsRecordMode() &&
        g_stkProgrammerState == STK500_PROGRAM_RECORDING &&
        cmd != STK_CMD_FIRMWARE_UPGRADE &&
        cmd != STK_CMD_SET_PROG_STATE &&
        cmd != STK_CMD_GET_OFFLINE_INFO &&
        cmd != STK_CMD_GET_OFFLINE_PACKAGE &&
        cmd != STK_CMD_SET_OFFLINE_ACTIVE)
    {
        (void)offlinePgmerRawAppendRxPacket(pRx, pDataFrame->frameLen);
    }
    
    SWITCH_START
    SWITCH_CASE(STK_CMD_SIGN_ON)
        /* 获取烧录器标识。 */
        static const char string[] = {8, 'S', 'T', 'K', '5', '0', '0', '_', '2', 0};
        memcpy(&pTx[STK_TXMSG_START + 2], string, sizeof(string));
        len.bytes[0] = 11;
    SWITCH_CASE(STK_CMD_SET_WORK_STATE)
        /* 设置工作模式: 0=simulate, 1=online, 2=record, 3=online+record。 */
        if (stkSetWorkMode(pRx[STK_TXMSG_START + 1]) == 0U)
            pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_FAILED;
        else
            pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_OK;
    SWITCH_CASE(STK_CMD_SET_PROG_STATE)
        /* 设置编程会话状态: 0=STOP_PROG 关闭离线包, 1=START_PROG 创建离线包。 */
        if (pRx[STK_TXMSG_START + 1] > STK500_PROGRAM_RECORDING)
        {
            pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_FAILED;
        }
        else if (pRx[STK_TXMSG_START + 1] == STK500_PROGRAM_RECORDING)
        {
            if (offlinePgmerRawBegin(&g_stkDeviceIdentity) == 0U)
            {
                g_stkProgrammerState = STK500_PROGRAM_RECORDING;
                pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_OK;
            }
            else
            {
                pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_FAILED;
            }
        }
        else
        {
            if (offlinePgmerRawEnd() == 0U)
            {
                g_stkProgrammerState = STK500_PROGRAM_IDLE;
                pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_OK;
            }
            else
            {
                pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_FAILED;
            }
        }
    SWITCH_CASE(STK_CMD_GET_OFFLINE_INFO)
        {
            uint16_t payloadLen = stkPutOfflineInfo(
                &pTx[STK_TXMSG_START + 2],
                (uint16_t)(BUFFER_SIZE - (STK_TXMSG_START + 2)));
            if (payloadLen == 0U)
            {
                pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_FAILED;
                len.bytes[0] = 2;
            }
            else
            {
                pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_OK;
                len.word = (uint16_t)(payloadLen + 2U);
            }
        }
    SWITCH_CASE(STK_CMD_GET_OFFLINE_PACKAGE)
        {
            uint16_t index = stkGetLe16(&pRx[STK_TXMSG_START + 1]);
            uint16_t payloadLen = stkPutOfflineSummary(
                &pTx[STK_TXMSG_START + 2],
                (uint16_t)(BUFFER_SIZE - (STK_TXMSG_START + 2)),
                index);
            if (payloadLen == 0U)
            {
                pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_FAILED;
                len.bytes[0] = 2;
            }
            else
            {
                pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_OK;
                len.word = (uint16_t)(payloadLen + 2U);
            }
        }
    SWITCH_CASE(STK_CMD_SET_OFFLINE_ACTIVE)
        {
            uint16_t index = stkGetLe16(&pRx[STK_TXMSG_START + 1]);
            pTx[STK_TXMSG_START + 1] =
                (offlinePgmerSetActivePackage(index) == 0U) ?
                STK_STATUS_CMD_OK : STK_STATUS_CMD_FAILED;
        }
    SWITCH_CASE(STK_CMD_SET_PARAMETER)  /* 设置 STK 参数或器件身份信息。 */
        if (pRx[STK_TXMSG_START + 1] == STK_PARAM_DEVICE_IDENTITY)
        {
            pTx[STK_TXMSG_START + 1] = stkSetDeviceIdentity(
                &pRx[STK_TXMSG_START + 2],
                (uint16_t)(payloadLen - 2U));
        }
        else
        {
            pTx[STK_TXMSG_START + 1] =
                setParameter(pRx[STK_TXMSG_START + 1], pRx[STK_TXMSG_START + 2]);
        }
    SWITCH_CASE(STK_CMD_GET_PARAMETER)
        if (pRx[STK_TXMSG_START + 1] == STK_PARAM_DEVICE_IDENTITY)
        {
            uint16_t payloadLen = stkGetDeviceIdentity(
                &pTx[STK_TXMSG_START + 2],
                (uint16_t)(BUFFER_SIZE - (STK_TXMSG_START + 2)));
            if (payloadLen == 0U)
            {
                pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_FAILED;
                len.bytes[0] = 2;
            }
            else
            {
                len.word = (uint16_t)(2U + payloadLen);
            }
        }
        else
        {
            pTx[STK_TXMSG_START + 2] = getParameter(pRx[STK_TXMSG_START + 1]);
            len.bytes[0] = 3;
        }
    SWITCH_CASE(STK_CMD_OSCCAL)
        pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_FAILED;
    SWITCH_CASE(STK_CMD_LOAD_ADDRESS)
        for(i = 0; i < 4; i++){
            stkAddress.bytes[3 - i] = pRx[STK_TXMSG_START + 1 + i];
        }
    SWITCH_CASE(STK_CMD_FIRMWARE_UPGRADE)   /* 该命令由上位机在升级固件前发送, 以便烧录器进入升级模式。 */
        {
            /* Payload must carry the magic bytes: cmd + 0xA5 0x5A. */
            if (payloadLen >= 3U &&
                pRx[STK_TXMSG_START + 1] == STK_FW_UPGRADE_MAGIC0 &&
                pRx[STK_TXMSG_START + 2] == STK_FW_UPGRADE_MAGIC1)
            {
                /* Only act for online USB sources; flash replay only replies, no side effects. */
                if (pDataFrame->source == STK_DATA_SOURCE_USB_HID ||
                    pDataFrame->source == STK_DATA_SOURCE_USB_CDC)
                {
                    SPI_EEPROM_WriteByte(EEPROM_BOOT_MODE_ADDR, EEPROM_BOOT_MODE_UPDATE);
                    if (SPI_EEPROM_ReadByte(EEPROM_BOOT_MODE_ADDR) == EEPROM_BOOT_MODE_UPDATE)
                    {
                        pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_OK;
                        g_stkFwUpgradePending = 1U;
                    }
                    else
                    {
                        pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_FAILED;
                    }
                }
            }
            else
            {
                pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_FAILED;
            }
        }
    SWITCH_CASE(STK_CMD_SET_CONTROL_STACK)
        /* AVR Studio 探测能力时会发送该命令, 这里保持 AVR-Doper 的兼容行为。 */
#if ENABLE_HVPROG
    SWITCH_CASE(STK_CMD_ENTER_PROGMODE_HVSP)
        {
            uint8_t recStatus = STK_STATUS_CMD_OK;
            if (stkIsOnlineMode())
                hvspEnterProgmode((stkEnterProgHvsp_t *)param);
            pTx[STK_TXMSG_START + 1] = recStatus;
        }
    SWITCH_CASE(STK_CMD_LEAVE_PROGMODE_HVSP)
        {
            if (stkIsOnlineMode())
                hvspLeaveProgmode((stkLeaveProgHvsp_t *)param);
        }
    SWITCH_CASE(STK_CMD_CHIP_ERASE_HVSP)
        pTx[STK_TXMSG_START + 1] = stkIsOnlineMode() ?
            hvspChipErase((stkChipEraseHvsp_t *)param) : STK_STATUS_CMD_OK;
    SWITCH_CASE(STK_CMD_PROGRAM_FLASH_HVSP)
        {
            stkProgramFlashHvsp_t *hvParam = (stkProgramFlashHvsp_t *)param;
            uint8_t onlineStatus = stkIsOnlineMode() ?
                hvspProgramMemory(hvParam, 0) : STK_STATUS_CMD_OK;
            uint8_t recStatus = STK_STATUS_CMD_OK;
            pTx[STK_TXMSG_START + 1] = stkProgramStatus(onlineStatus, recStatus);
        }
    SWITCH_CASE(STK_CMD_READ_FLASH_HVSP)
        if (stkIsOnlineMode())
            len.word = 1 + hvspReadMemory((stkReadFlashHvsp_t *)param, (void *)&pTx[STK_TXMSG_START + 1], 0);
        else
            pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_FAILED;
    SWITCH_CASE(STK_CMD_PROGRAM_EEPROM_HVSP)
        {
            stkProgramFlashHvsp_t *hvParam = (stkProgramFlashHvsp_t *)param;
            uint8_t onlineStatus = stkIsOnlineMode() ?
                hvspProgramMemory(hvParam, 1) : STK_STATUS_CMD_OK;
            uint8_t recStatus = STK_STATUS_CMD_OK;
            pTx[STK_TXMSG_START + 1] = stkProgramStatus(onlineStatus, recStatus);
        }
    SWITCH_CASE(STK_CMD_READ_EEPROM_HVSP)
        if (stkIsOnlineMode())
            len.word = 1 + hvspReadMemory((stkReadFlashHvsp_t *)param, (void *)&pTx[STK_TXMSG_START + 1], 1);
        else
            pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_FAILED;
    SWITCH_CASE(STK_CMD_PROGRAM_FUSE_HVSP)
        {
            stkProgramFuseHvsp_t *hvParam = (stkProgramFuseHvsp_t *)param;
            uint8_t onlineStatus = stkIsOnlineMode() ?
                hvspProgramFuse(hvParam) : STK_STATUS_CMD_OK;
            uint8_t recStatus = STK_STATUS_CMD_OK;
            pTx[STK_TXMSG_START + 1] = stkProgramStatus(onlineStatus, recStatus);
        }
    SWITCH_CASE(STK_CMD_READ_FUSE_HVSP)
        pTx[STK_TXMSG_START + 2] = stkIsOnlineMode() ?
            hvspReadFuse((stkReadFuseHvsp_t *)param) : 0xFFU;
        pTx[STK_TXMSG_START + 1] = stkIsOnlineMode() ?
            STK_STATUS_CMD_OK : STK_STATUS_CMD_FAILED;
        len.bytes[0] = 3;
    SWITCH_CASE(STK_CMD_PROGRAM_LOCK_HVSP)
        {
            stkProgramFuseHvsp_t *hvParam = (stkProgramFuseHvsp_t *)param;
            uint8_t onlineStatus = stkIsOnlineMode() ?
                hvspProgramLock(hvParam) : STK_STATUS_CMD_OK;
            uint8_t recStatus = STK_STATUS_CMD_OK;
            pTx[STK_TXMSG_START + 1] = stkProgramStatus(onlineStatus, recStatus);
        }
    SWITCH_CASE(STK_CMD_READ_LOCK_HVSP)
        pTx[STK_TXMSG_START + 2] = stkIsOnlineMode() ? hvspReadLock() : 0xFFU;
        pTx[STK_TXMSG_START + 1] = stkIsOnlineMode() ?
            STK_STATUS_CMD_OK : STK_STATUS_CMD_FAILED;
        len.bytes[0] = 3;
    SWITCH_CASE(STK_CMD_READ_SIGNATURE_HVSP)
        pTx[STK_TXMSG_START + 2] = stkIsOnlineMode() ?
            hvspReadSignature((stkReadFuseHvsp_t *)param) : 0xFFU;
        pTx[STK_TXMSG_START + 1] = stkIsOnlineMode() ?
            STK_STATUS_CMD_OK : STK_STATUS_CMD_FAILED;
        len.bytes[0] = 3;
    SWITCH_CASE(STK_CMD_READ_OSCCAL_HVSP)
        pTx[STK_TXMSG_START + 2] = stkIsOnlineMode() ? hvspReadOsccal() : 0xFFU;
        pTx[STK_TXMSG_START + 1] = stkIsOnlineMode() ?
            STK_STATUS_CMD_OK : STK_STATUS_CMD_FAILED;
        len.bytes[0] = 3;

    SWITCH_CASE(STK_CMD_ENTER_PROGMODE_PP)
        {
            uint8_t recStatus = STK_STATUS_CMD_OK;
            if (stkIsOnlineMode())
                ppEnterProgmode((stkEnterProgPp_t *)param);
            pTx[STK_TXMSG_START + 1] = recStatus;
        }
    SWITCH_CASE(STK_CMD_LEAVE_PROGMODE_PP)
        {
            if (stkIsOnlineMode())
                ppLeaveProgmode((stkLeaveProgPp_t *)param);
        }
    SWITCH_CASE(STK_CMD_CHIP_ERASE_PP)
        pTx[STK_TXMSG_START + 1] = stkIsOnlineMode() ?
            ppChipErase((stkChipErasePp_t *)param) : STK_STATUS_CMD_OK;
    SWITCH_CASE(STK_CMD_PROGRAM_FLASH_PP)
        {
            stkProgramFlashPp_t *ppParam = (stkProgramFlashPp_t *)param;
            uint8_t onlineStatus = stkIsOnlineMode() ?
                ppProgramMemory(ppParam, 0) : STK_STATUS_CMD_OK;
            uint8_t recStatus = STK_STATUS_CMD_OK;
            pTx[STK_TXMSG_START + 1] = stkProgramStatus(onlineStatus, recStatus);
        }
    SWITCH_CASE(STK_CMD_READ_FLASH_PP)
        if (stkIsOnlineMode())
            len.word = 1 + ppReadMemory((stkReadFlashPp_t *)param, (void *)&pTx[STK_TXMSG_START + 1], 0);
        else
            pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_FAILED;
    SWITCH_CASE(STK_CMD_PROGRAM_EEPROM_PP)
        {
            stkProgramFlashPp_t *ppParam = (stkProgramFlashPp_t *)param;
            uint8_t onlineStatus = stkIsOnlineMode() ?
                ppProgramMemory(ppParam, 1) : STK_STATUS_CMD_OK;
            uint8_t recStatus = STK_STATUS_CMD_OK;
            pTx[STK_TXMSG_START + 1] = stkProgramStatus(onlineStatus, recStatus);
        }
    SWITCH_CASE(STK_CMD_READ_EEPROM_PP)
        if (stkIsOnlineMode())
            len.word = 1 + ppReadMemory((stkReadFlashPp_t *)param, (void *)&pTx[STK_TXMSG_START + 1], 1);
        else
            pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_FAILED;
    SWITCH_CASE(STK_CMD_PROGRAM_FUSE_PP)
        {
            stkProgramFusePp_t *ppParam = (stkProgramFusePp_t *)param;
            uint8_t onlineStatus = stkIsOnlineMode() ?
                ppProgramFuse(ppParam) : STK_STATUS_CMD_OK;
            uint8_t recStatus = STK_STATUS_CMD_OK;
            pTx[STK_TXMSG_START + 1] = stkProgramStatus(onlineStatus, recStatus);
        }
    SWITCH_CASE(STK_CMD_READ_FUSE_PP)
        pTx[STK_TXMSG_START + 2] = stkIsOnlineMode() ?
            ppReadFuse((stkReadFusePp_t *)param) : 0xFFU;
        pTx[STK_TXMSG_START + 1] = stkIsOnlineMode() ?
            STK_STATUS_CMD_OK : STK_STATUS_CMD_FAILED;
        len.bytes[0] = 3;
    SWITCH_CASE(STK_CMD_PROGRAM_LOCK_PP)
        {
            stkProgramFusePp_t *ppParam = (stkProgramFusePp_t *)param;
            uint8_t onlineStatus = stkIsOnlineMode() ?
                ppProgramLock(ppParam) : STK_STATUS_CMD_OK;
            uint8_t recStatus = STK_STATUS_CMD_OK;
            pTx[STK_TXMSG_START + 1] = stkProgramStatus(onlineStatus, recStatus);
        }
    SWITCH_CASE(STK_CMD_READ_LOCK_PP)
        pTx[STK_TXMSG_START + 2] = stkIsOnlineMode() ? ppReadLock() : 0xFFU;
        pTx[STK_TXMSG_START + 1] = stkIsOnlineMode() ?
            STK_STATUS_CMD_OK : STK_STATUS_CMD_FAILED;
        len.bytes[0] = 3;
    SWITCH_CASE(STK_CMD_READ_SIGNATURE_PP)
        pTx[STK_TXMSG_START + 2] = stkIsOnlineMode() ?
            ppReadSignature((stkReadFusePp_t *)param) : 0xFFU;
        pTx[STK_TXMSG_START + 1] = stkIsOnlineMode() ?
            STK_STATUS_CMD_OK : STK_STATUS_CMD_FAILED;
        len.bytes[0] = 3;
    SWITCH_CASE(STK_CMD_READ_OSCCAL_PP)
        pTx[STK_TXMSG_START + 2] = stkIsOnlineMode() ? ppReadOsccal() : 0xFFU;
        pTx[STK_TXMSG_START + 1] = stkIsOnlineMode() ?
            STK_STATUS_CMD_OK : STK_STATUS_CMD_FAILED;
        len.bytes[0] = 3;
#endif
    SWITCH_CASE(STK_CMD_ENTER_PROGMODE_ISP)
        {
            uint8_t recStatus = STK_STATUS_CMD_OK;
            uint8_t onlineStatus = stkIsOnlineMode() ?
                ispEnterProgmode((stkEnterProgIsp_t *)param) : STK_STATUS_CMD_OK;
            pTx[STK_TXMSG_START + 1] = stkProgramStatus(onlineStatus, recStatus);
        }
    SWITCH_CASE(STK_CMD_LEAVE_PROGMODE_ISP)
        {
            if (stkIsOnlineMode())
                ispLeaveProgmode((stkLeaveProgIsp_t *)param);
        }
    SWITCH_CASE(STK_CMD_CHIP_ERASE_ISP)
        pTx[STK_TXMSG_START + 1] = stkIsOnlineMode() ?
            ispChipErase((stkChipEraseIsp_t *)param) : STK_STATUS_CMD_OK;
    SWITCH_CASE(STK_CMD_PROGRAM_FLASH_ISP)
        {
            stkProgramFlashIsp_t *ispParam = (stkProgramFlashIsp_t *)param;
            uint8_t onlineStatus = stkIsOnlineMode() ?
                ispProgramMemory(ispParam, 0) : STK_STATUS_CMD_OK;
            uint8_t recStatus = STK_STATUS_CMD_OK;
            pTx[STK_TXMSG_START + 1] = stkProgramStatus(onlineStatus, recStatus);
        }
    SWITCH_CASE(STK_CMD_READ_FLASH_ISP)
        if (stkIsOnlineMode())
            len.word = 1 + ispReadMemory((stkReadFlashIsp_t *)param, (void *)&pTx[STK_TXMSG_START + 1], 0);
        else
            pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_FAILED;
    SWITCH_CASE(STK_CMD_PROGRAM_EEPROM_ISP)
        {
            stkProgramFlashIsp_t *ispParam = (stkProgramFlashIsp_t *)param;
            uint8_t onlineStatus = stkIsOnlineMode() ?
                ispProgramMemory(ispParam, 1) : STK_STATUS_CMD_OK;
            uint8_t recStatus = STK_STATUS_CMD_OK;
            pTx[STK_TXMSG_START + 1] = stkProgramStatus(onlineStatus, recStatus);
        }
    SWITCH_CASE(STK_CMD_READ_EEPROM_ISP)
        if (stkIsOnlineMode())
            len.word = 1 + ispReadMemory((stkReadFlashIsp_t *)param, (void *)&pTx[STK_TXMSG_START + 1], 1);
        else
            pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_FAILED;
    SWITCH_CASE(STK_CMD_PROGRAM_FUSE_ISP)
        {
            stkProgramFuseIsp_t *ispParam = (stkProgramFuseIsp_t *)param;
            uint8_t onlineStatus = stkIsOnlineMode() ?
                ispProgramFuse(ispParam) : STK_STATUS_CMD_OK;
            uint8_t recStatus = STK_STATUS_CMD_OK;
            pTx[STK_TXMSG_START + 1] = stkProgramStatus(onlineStatus, recStatus);
        }
    SWITCH_CASE4(STK_CMD_READ_FUSE_ISP, STK_CMD_READ_LOCK_ISP, STK_CMD_READ_SIGNATURE_ISP, STK_CMD_READ_OSCCAL_ISP)
        if (stkIsOnlineMode())
        {
            pTx[STK_TXMSG_START + 2] = ispReadFuse((stkReadFuseIsp_t *)param);
            pTx[STK_TXMSG_START + 3] = STK_STATUS_CMD_OK;
        }
        else
        {
            pTx[STK_TXMSG_START + 2] = 0xFF;
            pTx[STK_TXMSG_START + 3] = STK_STATUS_CMD_FAILED;
        }
        len.bytes[0] = 4;
    SWITCH_CASE(STK_CMD_PROGRAM_LOCK_ISP)
        {
            stkProgramFuseIsp_t *ispParam = (stkProgramFuseIsp_t *)param;
            uint8_t onlineStatus = stkIsOnlineMode() ?
                ispProgramFuse(ispParam) : STK_STATUS_CMD_OK;
            uint8_t recStatus = STK_STATUS_CMD_OK;
            pTx[STK_TXMSG_START + 1] = stkProgramStatus(onlineStatus, recStatus);
        }
    SWITCH_CASE(STK_CMD_SPI_MULTI)
        if (stkIsOnlineMode())
            len.word = 1 + ispMulti((stkMultiIsp_t *)param, (void *)&pTx[STK_TXMSG_START + 1]);
        else
            pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_FAILED;
        /*------------------- PIC ICSP 命令处理开始 -------------------*/
    SWITCH_CASE(STK_CMD_ENTER_PROGMODE_ICSP)
        {
            /* 进入 ICSP 模式时, 上位机下发模式: 0=高压, 1=低压。 */
            uint8_t recStatus = STK_STATUS_CMD_OK;
            uint8_t onlineStatus = stkIsOnlineMode() ?
                pic8EnterProgmode((uint8_t)(pRx[STK_TXMSG_START + 1] & 0x01U)) :
                STK_STATUS_CMD_OK;
            pTx[STK_TXMSG_START + 1] = stkProgramStatus(onlineStatus, recStatus);
        }
    SWITCH_CASE(STK_CMD_LEAVE_PROGMODE_ICSP)
        {
            if (stkIsOnlineMode())
                pic8LeaveProgmode();
        }
    SWITCH_CASE(STK_CMD_CHIP_ERASE_ICSP)
        pTx[STK_TXMSG_START + 1] = stkIsOnlineMode() ?
            icspBulkErase() : STK_STATUS_CMD_OK;
    SWITCH_CASE(STK_CMD_PROGRAM_FLASH_ICSP)
        {
            stkProgramFlashIcsp_t *icspParam = (stkProgramFlashIcsp_t *)param;
            uint8_t onlineStatus = stkIsOnlineMode() ?
                stkIcspProgramFlash(icspParam) : STK_STATUS_CMD_OK;
            pTx[STK_TXMSG_START + 1] = onlineStatus;
        }
    SWITCH_CASE(STK_CMD_READ_FLASH_ICSP)
        if (stkIsOnlineMode())
            len.word = 1 + stkIcspReadFlash((stkReadFlashIcsp_t *)param, (void *)&pTx[STK_TXMSG_START + 1]);
        else
            pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_FAILED;
    SWITCH_CASE(STK_CMD_PROGRAM_EEPROM_ICSP)
        {
            stkProgramEepromIcsp_t *icspParam = (stkProgramEepromIcsp_t *)param;
            uint8_t onlineStatus = stkIsOnlineMode() ?
                stkIcspProgramEeprom(icspParam) : STK_STATUS_CMD_OK;
            pTx[STK_TXMSG_START + 1] = onlineStatus;
        }
    SWITCH_CASE(STK_CMD_READ_EEPROM_ICSP)
        if (stkIsOnlineMode())
            len.word = 1 + stkIcspReadEeprom((stkReadEepromIcsp_t *)param, (void *)&pTx[STK_TXMSG_START + 1]);
        else
            pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_FAILED;
    SWITCH_CASE(STK_CMD_PROGRAM_CONFIG_ICSP)
        {
            stkProgramConfigIcsp_t *icspParam = (stkProgramConfigIcsp_t *)param;
            uint16_t cfgValue = stkGetLe16(icspParam->value);
            uint8_t onlineStatus = stkIsOnlineMode() ?
                icspProgCfg(icspParam->index, cfgValue) : STK_STATUS_CMD_OK;
            uint8_t recStatus = STK_STATUS_CMD_OK;
            pTx[STK_TXMSG_START + 1] = stkProgramStatus(onlineStatus, recStatus);
        }
    SWITCH_CASE(STK_CMD_READ_CONFIG_ICSP)
        {
            uint16_t value = 0xFFFFU;
            if (stkIsOnlineMode())
                value = icspReadCfg(((stkReadConfigIcsp_t *)param)->index);
            pTx[STK_TXMSG_START + 1] =
                (stkIsOnlineMode() && value != 0xFFFFU) ? STK_STATUS_CMD_OK : STK_STATUS_CMD_FAILED;
            stkPutLe16(&pTx[STK_TXMSG_START + 2], value);
            len.bytes[0] = 4;
        }
    SWITCH_CASE(STK_CMD_PROGRAM_USER_ID_ICSP)
        {
            stkProgramUserIdIcsp_t *icspParam = (stkProgramUserIdIcsp_t *)param;
            uint16_t value = stkGetLe16(icspParam->value);
            uint8_t onlineStatus = stkIsOnlineMode() ?
                icspProgUID(icspParam->index, value) : STK_STATUS_CMD_OK;
            uint8_t recStatus = STK_STATUS_CMD_OK;
            pTx[STK_TXMSG_START + 1] = stkProgramStatus(onlineStatus, recStatus);
        }
    SWITCH_CASE(STK_CMD_READ_USER_ID_ICSP)
        {
            uint16_t value = 0xFFFFU;
            if (stkIsOnlineMode())
                value = icspReadUID(((stkReadUserIdIcsp_t *)param)->index);
            pTx[STK_TXMSG_START + 1] =
                (stkIsOnlineMode() && value != 0xFFFFU) ? STK_STATUS_CMD_OK : STK_STATUS_CMD_FAILED;
            stkPutLe16(&pTx[STK_TXMSG_START + 2], value);
            len.bytes[0] = 4;
        }
    SWITCH_CASE(STK_CMD_READ_SIGNATURE_ICSP)
        {
            uint16_t value = 0U;
            pTx[STK_TXMSG_START + 1] = stkIsOnlineMode() ?
                icspReadSignature(&value) : STK_STATUS_CMD_FAILED;
            stkPutLe16(&pTx[STK_TXMSG_START + 2], value);
            len.bytes[0] = 4;
        }
    SWITCH_CASE(STK_CMD_READ_OSCCAL_ICSP)
        {
            uint16_t value = 0xFFFFU;
            if (stkIsOnlineMode())
                value = icspReadOSCCAL(((stkReadOsccalIcsp_t *)param)->index);
            pTx[STK_TXMSG_START + 1] =
                (stkIsOnlineMode() && value != 0xFFFFU) ? STK_STATUS_CMD_OK : STK_STATUS_CMD_FAILED;
            stkPutLe16(&pTx[STK_TXMSG_START + 2], value);
            len.bytes[0] = 4;
        }
    SWITCH_CASE(STK_CMD_WRITE_OSCCAL_ICSP)
        {
            stkWriteOsccalIcsp_t *icspParam = (stkWriteOsccalIcsp_t *)param;
            uint16_t value = stkGetLe16(icspParam->value);
            uint8_t onlineStatus = stkIsOnlineMode() ?
                icspWriteOSCCAL(icspParam->index, value) : STK_STATUS_CMD_OK;
            uint8_t recStatus = STK_STATUS_CMD_OK;
            pTx[STK_TXMSG_START + 1] = stkProgramStatus(onlineStatus, recStatus);
        }
    /* PIC ICSP 命令处理结束。 */
    SWITCH_DEFAULT
        pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_FAILED;
    SWITCH_END


    pDataFrame->txFrameLen = stkSetTxMessage(pTx, pDataFrame->txFrameSize, len.word, pRx[1]);
    if (pDataFrame->source == STK_DATA_SOURCE_USB_HID ||
        pDataFrame->source == STK_DATA_SOURCE_USB_WINUSB)
    {
        txPos = 0U;
        txLen = pDataFrame->txFrameLen;
    }

#if DEBUG_HARDWARE_CONFIG
    /* 调试: 必须在 stkSetTxMessage() 之后打印，此时长度和 XOR 校验和才完整。 */
    if ((pDataFrame->source == STK_DATA_SOURCE_USB_HID ||
         pDataFrame->source == STK_DATA_SOURCE_USB_CDC) &&
        pDataFrame->txFrameLen != 0U)
    {
        static const char hexTable[] = "0123456789ABCDEF";
        uint16_t i;
        uart1_WriteString("Send:");
        for (i = 0; i < pDataFrame->txFrameLen; i++)
        {
            uart1_WriteByte((uint8_t)hexTable[(pTx[i] >> 4) & 0x0F]);
            uart1_WriteByte((uint8_t)hexTable[pTx[i] & 0x0F]);
            uart1_WriteByte(' ');
        }
        uart1_WriteString("\r\n");
    }
#endif
}

/* USB HID 在线入口: 逐字节组装 STK500 帧并校验 XOR 校验和。 */
static uint8_t g_rxSource = STK_DATA_SOURCE_USB_HID;
static uint8_t g_txSource = STK_DATA_SOURCE_USB_HID;
static void stkAssemble(uint8_t c);

void stkSetRxChar(uint8_t c)
{
    g_rxSource = STK_DATA_SOURCE_USB_HID;
    stkAssemble(c);
}

void stkSetRxCharEx(uint8_t src, uint8_t c)
{
    g_rxSource = src;
    stkAssemble(c);
}

static void stkAssemble(uint8_t c)
{
    if(timerLongTimeoutOccurred()){
        rxPos = 0;
    }
    if(rxPos == 0){
        if(c == STK_STX){
            rxBuffer[rxPos++] = c;
        }
    }else{
        if(rxPos < BUFFER_SIZE){
            rxBuffer[rxPos++] = c;
            if(rxPos == 4){
                rxLen.bytes[0] = rxBuffer[3];
                rxLen.bytes[1] = rxBuffer[2];
                rxLen.word += 6;
                if(rxLen.word > BUFFER_SIZE){
                    rxPos = 0;
                }
            }else if(rxPos == 5){
                if(c != STK_TOKEN){
                    rxPos = 0;
                }
            }else if(rxPos > 4 && rxPos == rxLen.word){
                uint8_t sum = 0;
                uint8_t *p = rxBuffer;
                while(rxPos){
                    sum ^= *p++;
                    rxPos--;
                }
                if(sum == 0){
                    rxBlockAvailable = 1;
                }else{
                    txBuffer[STK_TXMSG_START] = STK_ANSWER_CKSUM_ERROR;
                    txBuffer[STK_TXMSG_START + 1] = STK_ANSWER_CKSUM_ERROR;
                    txLen = stkSetTxMessage(txBuffer, BUFFER_SIZE, 2, rxBuffer[1]);
                    txPos = 0U;
                }
            }
        }else{
            rxPos = 0;
        }
    }
    timerSetupLongTimeout(RX_TIMEOUT);
}

int stkGetTxCount(void)
{
    return txLen - txPos;
}

uint8_t stkGetTxSource(void)
{
    return g_txSource;
}

void stkWinUSBTask(void)
{
    uint8_t buf[64];
    uint16_t n, i;

    while ((n = WinUSB_Bulk_Available()) != 0U)
    {
        if (n > sizeof(buf))
            n = sizeof(buf);

        n = WinUSB_Bulk_Recv(buf, n);
        for (i = 0U; i < n; i++)
        {
            stkSetRxCharEx(STK_DATA_SOURCE_USB_WINUSB, buf[i]);
        }
    }

    stkPoll();
}

void stkWinUSBFlush(void)
{
    uint8_t out[64];
    uint16_t n, i;
    if (stkGetTxCount() <= 0) return;
    if (stkGetTxSource() != STK_DATA_SOURCE_USB_WINUSB) return;
    if (WinUSB_Bulk_TxBusy()) return;
    n = (uint16_t)stkGetTxCount();
    if (n > sizeof(out)) n = sizeof(out);
    for (i = 0U; i < n; i++)
    {
        int c = stkGetTxByte();
        if (c < 0) break;
        out[i] = (uint8_t)c;
    }
    if (i > 0U)
    {
        WinUSB_Bulk_Send(out, i);
    }
}

int stkGetTxByte(void)
{
    uint8_t c;

    if(txLen == 0){
        return -1;
    }
    c = txBuffer[txPos++];
    if(txPos >= txLen){
        txPos = txLen = 0;
    }
    return c;
}

/* 主循环轮询入口: 将 USB RX/TX 缓冲包装成 stkDataFrame_t 后交给协议解析。 */
void stkPoll(void)
{
    if(rxBlockAvailable){
        rxBlockAvailable = 0;
        {
            stkDataFrame_t frame;
            frame.frame = rxBuffer;
            frame.frameLen = rxLen.word;
            frame.txFrame = txBuffer;
            frame.txFrameSize = BUFFER_SIZE;
            frame.txFrameLen = 0U;
            g_txSource = g_rxSource;
            frame.source = g_rxSource;
            stkEvaluateRxMessage(&frame);
        }
    }
}

void stkIncrementAddress(void)
{
    stkAddress.dword++;
}
