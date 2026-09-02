/*
 * STK500v2 协�??解析模块
 *
 * �?模块负责解析上位机发送的 STK500v2 数据�? 并把命令分发�?
 * AVR ISP/HVSP/HVPP、PIC ICSP 以及离线数据包�?�录模块�??
 *
 * 数据来源�?持两�?: USB HID 在线通�??�?Flash 离线回放�?
 * stkEvaluateRxMessage() 通过 stkDataFrame_t 同时接收 RX/TX 缓冲,
 * 从而避免在�?TX 和�?�线判�??TX 互相交叉�?
 */

#include "Stk500Protocol.h"
#include <string.h>
#include "timer.h"
#include "isp.h"
#include "hvproc.h"
#include "icsp.h"
#include "picDeviceConst.h"
#include "offLineRecorder.h"
#include "eeprom.h"
#include "usart.h"
#include "MCP4017_VDD.h"
#include "MCP4017_VPP.h"
#include "adc.h"
#include "delay.h"

/* Session cleanup remains unconditional; suppress its routine UART chatter. */
#define STK_SESSION_TRACE 0

/* 上报给上位机�?STK500 版本号�?*/
#define STK_VERSION_HW      1
#define STK_VERSION_MAJOR   2
#define STK_VERSION_MINOR   4

/* USB HID 接收链使用的全局 RX 缓冲, �?保存当前在线收到的一帧�?*/
static uint8_t      rxBuffer[BUFFER_SIZE];
static uint16_t     rxPos;
static utilWord_t   rxLen;
static uint8_t      rxBlockAvailable;

/* USB HID 发送链使用的全局 TX 缓冲。Flash 回放使用调用方传入的�?�? TX 缓冲�?*/
static uint8_t      txBuffer[BUFFER_SIZE];
static uint16_t     txPos, txLen;

stkParam_t      stkParam = {{
                    0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0,
                    STK_VERSION_HW, STK_VERSION_MAJOR, STK_VERSION_MINOR, 0, 50, 0, 1, 0x80,
                    2, 0, 0xaa, 0, 0, 0, 0, 0,
                }};
utilDword_t     stkAddress;

/* 全局变量: 记录上位机下发的器件和项�?�?份信�?�??*/
static stkDeviceIdentity_t g_stkDeviceIdentity;
/* ICSP session-level deviceID precheck flag: reset after each ENTER_PROGMODE_ICSP */
static uint8_t g_stkIcspDeviceIdChecked;

/* 从小�?字节流中读�??16 位数�?�??*/
static uint16_t stkGetLe16(const uint8_t *bytes);
/* �?16 位数�?按小�?格式写入字节流�?*/
static void stkPutLe16(uint8_t *bytes, uint16_t value);
/* 保存上位机下发的器件和项�?�?份信�? 后续会写�?Raw 离线包头�?*/
static uint8_t stkSetDeviceIdentity(const uint8_t *payload, uint16_t payloadLen);
/* 将当前器件和项目�?份信�?打包返回给上位机�?*/
static uint16_t stkGetDeviceIdentity(uint8_t *out, uint16_t outSize);
/* 打包离线包总体信息: 有效包数量、激活包序号、最大包数量�?*/
static uint16_t stkPutOfflineInfo(uint8_t *out, uint16_t outSize);
/* 打包指定离线包摘�? 供上位机查看 Flash �?的�?�录内�?��??*/
static uint16_t stkPutOfflineSummary(uint8_t *out, uint16_t outSize, uint16_t index);
static uint16_t stkPutOfflineDeviceName(uint8_t *out, uint16_t outSize, uint8_t arch, uint16_t index);
static void stkTraceOfflineInfo(const offline_package_info_t *info, uint8_t ok);
static void stkTraceOfflineSummary(uint16_t slot, const offline_package_index_t *summary, uint8_t ok);
/* ICSP first-write deviceID verification helper. Skip when no valid expected value exists. */
static uint8_t stkEnsureIcspDeviceIdVerified(void);
/* Normalize PIC config readback to host-visible 16-bit form to avoid false verify mismatches. */
static uint16_t stkNormalizeIcspConfigValue(uint8_t idx, uint16_t value);

/* 保留 AVR-Doper �?switch 宏�?��?? 便于和原始协�?分发结构对照�??*/
#define SWITCH_START        switch(cmd){{
#define SWITCH_CASE(value)  }break; case (value):{
#define SWITCH_CASE2(v1,v2) }break; case (v1): case(v2):{
#define SWITCH_CASE3(v1,v2,v3) }break; case (v1): case(v2): case(v3):{
#define SWITCH_CASE4(v1,v2,v3,v4) }break; case (v1): case(v2): case(v3): case(v4):{
#define SWITCH_DEFAULT      }break; default:{
#define SWITCH_END          }}

/* 离线模式下的记录状�? 0=空闲(IDLE), 1=记录�?*/
uint8_t g_stkProgrammerState = STK500_PROGRAM_IDLE;

/* Firmware-upgrade request flag: set after the boot-control page is updated. */
static volatile uint8_t g_stkFwUpgradePending;

static uint32_t stkBootCtrlCrc32(const uint8_t *data, uint32_t len);
static uint8_t stkBootCtrlIsAppInfoValid(const boot_app_image_info_t *info);
static uint8_t stkBootCtrlReadAppInfo(boot_app_image_info_t *info);
static uint8_t stkBootCtrlIsValid(const boot_app_ctrl_t *ctrl);
static void stkBootCtrlInitBlank(boot_app_ctrl_t *ctrl);
static uint8_t stkBootCtrlLoadPage(uint32_t addr, boot_app_ctrl_t *ctrl);
static uint8_t stkBootCtrlRead(boot_app_ctrl_t *ctrl);
static uint8_t stkBootCtrlFlashWaitReady(void);
static void stkBootCtrlFlashUnlock(void);
static void stkBootCtrlFlashLock(void);
static uint8_t stkBootCtrlFlashErasePage(uint32_t pageAddr);
static uint8_t stkBootCtrlFlashProgramHalfWords(uint32_t addr, const uint8_t *data, uint16_t len);
static uint8_t stkBootCtrlWrite(const boot_app_ctrl_t *ctrl);
static uint8_t stkBootCtrlSaveState(uint16_t state, uint32_t bootCount, uint32_t lastError);

static void stkTraceOfflineInfo(const offline_package_info_t *info, uint8_t ok)
{
    uart1_WriteString("[STK] offline info ");
    if (ok == 0U || info == 0)
    {
        uart1_WriteString("fail\r\n");
        return;
    }

    uart1_WriteString("count=");
    uart1_WriteDec(info->package_count);
    uart1_WriteString(" active=");
    uart1_WriteHex16(info->active_index);
    uart1_WriteString(" max=");
    uart1_WriteDec(info->max_count);
    uart1_WriteString("\r\n");
}

static void stkTraceOfflineSummary(uint16_t slot, const offline_package_index_t *summary, uint8_t ok)
{
    uart1_WriteString("[STK] offline slot=");
    uart1_WriteDec(slot);
    if (ok == 0U || summary == 0)
    {
        uart1_WriteString(" fail\r\n");
        return;
    }

    uart1_WriteString(" used=");
    uart1_WriteDec(summary->used);
    uart1_WriteString(" state=");
    uart1_WriteDec(summary->package_state);
    uart1_WriteString(" pkg=");
    uart1_WriteDec(summary->package_index);
    uart1_WriteString(" dev=0x");
    uart1_WriteHex16(summary->identity.index);
    uart1_WriteString("\r\n");
}

static uint16_t stkPutOfflineDeviceName(uint8_t *out, uint16_t outSize, uint8_t arch, uint16_t index)
{
    uint8_t i;

    if (out == 0 || outSize < DEVICE_NAME_CHAR_LENGTH)
        return 0U;

    memset(out, 0, DEVICE_NAME_CHAR_LENGTH);
    if (arch == STK_MCU_ARCH_AVR)
    {
        avr_prog_params_t dev;

        if (avrFindDeviceByIndex(index, &dev) != 0)
            return 0U;
        for (i = 0U; i < DEVICE_NAME_CHAR_LENGTH && dev.device_name[i] != 0U; ++i)
            out[i] = dev.device_name[i];
        return DEVICE_NAME_CHAR_LENGTH;
    }
    if (arch == STK_MCU_ARCH_PIC)
    {
        const pic8_device_index_t *entry;

        if (pic8GetDeviceEntry(index, &entry) != 0 || entry == 0)
            return 0U;
        for (i = 0U; i < DEVICE_NAME_CHAR_LENGTH && entry->name[i] != 0U; ++i)
            out[i] = entry->name[i];
        return DEVICE_NAME_CHAR_LENGTH;
    }

    return 0U;
}

uint8_t stkFwUpgradeRequested(void)
{
    return g_stkFwUpgradePending;
}

uint8_t stkBootConfirmApplicationReady(void)
{
    return stkBootCtrlSaveState(BOOT_APP_STATE_CONFIRMED, 0U, 0U);
}

static uint32_t stkBootCtrlCrc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;
    uint32_t i;
    uint8_t bit;

    for (i = 0U; i < len; ++i)
    {
        crc ^= (uint32_t)data[i];
        for (bit = 0U; bit < 8U; ++bit)
        {
            crc = (crc & 1UL) ? ((crc >> 1) ^ 0xEDB88320UL) : (crc >> 1);
        }
    }
    return ~crc;
}

static uint8_t stkBootCtrlIsAppInfoValid(const boot_app_image_info_t *info)
{
    uint32_t calcCrc;

    if (info == 0)
        return 0U;
    if (info->magic != BOOT_APP_IMAGE_INFO_MAGIC)
        return 0U;
    if (info->headerVersion != BOOT_APP_IMAGE_INFO_VERSION)
        return 0U;
    if (info->imageType != BOOT_APP_IMAGE_TYPE_APP)
        return 0U;
    if (info->imageBase != APP_CODE_BASE)
        return 0U;
    if (info->imageSize > APP_CODE_MAX_SIZE)
        return 0U;
    if (info->headerCrc32 != 0UL && info->headerCrc32 != 0xFFFFFFFFUL)
    {
        calcCrc = stkBootCtrlCrc32((const uint8_t *)info, (uint32_t)sizeof(*info) - 4U);
        if (calcCrc != info->headerCrc32)
            return 0U;
    }
    return 1U;
}

static uint8_t stkBootCtrlReadAppInfo(boot_app_image_info_t *info)
{
    if (info == 0)
        return 0U;
    memcpy(info, (const void *)APP_INFO_ADDR, sizeof(*info));
    return stkBootCtrlIsAppInfoValid(info);
}

static uint8_t stkBootCtrlIsValid(const boot_app_ctrl_t *ctrl)
{
    uint32_t calcCrc;

    if (ctrl == 0)
        return 0U;
    if (ctrl->magic != BOOT_APP_CTRL_MAGIC)
        return 0U;
    if (ctrl->headerVersion != BOOT_APP_CTRL_VERSION)
        return 0U;
    if (ctrl->state > BOOT_APP_STATE_INVALID)
        return 0U;
    calcCrc = stkBootCtrlCrc32((const uint8_t *)ctrl, (uint32_t)sizeof(*ctrl) - 4U);
    if (calcCrc != ctrl->headerCrc32)
        return 0U;
    return 1U;
}

static void stkBootCtrlInitBlank(boot_app_ctrl_t *ctrl)
{
    if (ctrl == 0)
        return;
    memset(ctrl, 0, sizeof(*ctrl));
    ctrl->magic = BOOT_APP_CTRL_MAGIC;
    ctrl->headerVersion = BOOT_APP_CTRL_VERSION;
    ctrl->state = BOOT_APP_STATE_BLANK;
    ctrl->appBase = APP_CODE_BASE;
}

static uint8_t stkBootCtrlLoadPage(uint32_t addr, boot_app_ctrl_t *ctrl)
{
    if (ctrl == 0)
        return 0U;
    memcpy(ctrl, (const void *)addr, sizeof(*ctrl));
    return stkBootCtrlIsValid(ctrl);
}

static uint8_t stkBootCtrlRead(boot_app_ctrl_t *ctrl)
{
    boot_app_ctrl_t ctrlA;
    uint8_t validA = stkBootCtrlLoadPage(BOOT_CTRL_A_ADDR, &ctrlA);

    if (ctrl == 0)
        return validA;
    if (!validA)
    {
        stkBootCtrlInitBlank(ctrl);
        return 0U;
    }

    *ctrl = ctrlA;
    return 1U;
}

static uint8_t stkBootCtrlFlashWaitReady(void)
{
    uint32_t sr;
    uint32_t timeout = 0x0FFFFFFFUL;

    do
    {
        sr = FLASH->SR;
        if (timeout-- == 0U)
        {
            FLASH->SR = 0x14U;
            return 0U;
        }
    } while ((sr & 0x01U) != 0U);

    if ((sr & 0x14U) != 0U)
    {
        FLASH->SR = 0x14U;
        return 0U;
    }
    return 1U;
}

static void stkBootCtrlFlashUnlock(void)
{
    FLASH->KEYR = 0x45670123U;
    FLASH->KEYR = 0xCDEF89ABU;
}

static void stkBootCtrlFlashLock(void)
{
    FLASH->CR |= 0x80U;
}

static uint8_t stkBootCtrlFlashErasePage(uint32_t pageAddr)
{
    stkBootCtrlFlashUnlock();
    FLASH->CR |= 0x02U;
    FLASH->AR = pageAddr;
    FLASH->CR |= 0x40U;
    if (!stkBootCtrlFlashWaitReady())
    {
        FLASH->CR &= ~0x02U;
        stkBootCtrlFlashLock();
        return 0U;
    }
    FLASH->CR &= ~0x02U;
    stkBootCtrlFlashLock();
    return 1U;
}

static uint8_t stkBootCtrlFlashProgramHalfWords(uint32_t addr, const uint8_t *data, uint16_t len)
{
    uint16_t i;

    if (((addr & 1U) != 0U) || ((len & 1U) != 0U))
        return 0U;

    stkBootCtrlFlashUnlock();
    for (i = 0U; i < len; i += 2U)
    {
        uint16_t hw = (uint16_t)data[i] | ((uint16_t)data[i + 1U] << 8);
        FLASH->CR |= 0x01U;
        *(volatile uint16_t *)(addr + i) = hw;
        if (!stkBootCtrlFlashWaitReady())
        {
            FLASH->CR &= ~0x01U;
            stkBootCtrlFlashLock();
            return 0U;
        }
        FLASH->CR &= ~0x01U;
    }
    stkBootCtrlFlashLock();
    return 1U;
}

static uint8_t stkBootCtrlWrite(const boot_app_ctrl_t *ctrl)
{
    if (ctrl == 0)
        return 0U;
    if (!stkBootCtrlFlashErasePage(BOOT_CTRL_A_ADDR))
        return 0U;
    if (!stkBootCtrlFlashProgramHalfWords(BOOT_CTRL_A_ADDR, (const uint8_t *)ctrl, (uint16_t)sizeof(*ctrl)))
        return 0U;
    return stkBootCtrlIsValid((const boot_app_ctrl_t *)BOOT_CTRL_A_ADDR);
}

static uint8_t stkBootCtrlSaveState(uint16_t state, uint32_t bootCount, uint32_t lastError)
{
    boot_app_ctrl_t currentCtrl;
    boot_app_ctrl_t nextCtrl;
    boot_app_image_info_t appInfo;
    boot_app_image_info_t *appInfoPtr = 0;

    (void)stkBootCtrlRead(&currentCtrl);
    if (stkBootCtrlReadAppInfo(&appInfo))
        appInfoPtr = &appInfo;

    memset(&nextCtrl, 0, sizeof(nextCtrl));
    nextCtrl.magic = BOOT_APP_CTRL_MAGIC;
    nextCtrl.headerVersion = BOOT_APP_CTRL_VERSION;
    nextCtrl.state = state;
    nextCtrl.sequence = currentCtrl.sequence + 1U;
    nextCtrl.bootCount = bootCount;
    nextCtrl.appBase = APP_CODE_BASE;
    nextCtrl.lastError = lastError;
    if (appInfoPtr != 0)
    {
        nextCtrl.appBase = appInfoPtr->imageBase;
        nextCtrl.appSize = appInfoPtr->imageSize;
        nextCtrl.appCrc32 = appInfoPtr->imageCrc32;
        memcpy(nextCtrl.appVersionText, appInfoPtr->versionText, BOOT_APP_VERSION_TEXT_LEN);
    }
    nextCtrl.headerCrc32 = stkBootCtrlCrc32((const uint8_t *)&nextCtrl, (uint32_t)sizeof(nextCtrl) - 4U);
    return stkBootCtrlWrite(&nextCtrl);
}

void stkResetIspSession(void)
{
    stkLeaveProgIsp_t leaveParam;

    leaveParam.preDelay = 0U;
    leaveParam.postDelay = 0U;

    ispLeaveProgmode(&leaveParam);
#if DEBUG_HARDWARE_CONFIG && STK_SESSION_TRACE
    uart1_WriteString("STK cleanup: force AVR leave\r\n");
#endif
}

void stkResetIcspSession(void)
{
    pic8LeaveProgmode();
    g_stkIcspDeviceIdChecked = 0U;
#if DEBUG_HARDWARE_CONFIG && STK_SESSION_TRACE
    uart1_WriteString("STK cleanup: force PIC leave\r\n");
#endif
}

void stkResetAllProgrammingSessions(void)
{
    stkResetIspSession();
    stkResetIcspSession();
}

/* 从小�?字节流中读�??16 位数�?�??*/
static uint16_t stkGetLe16(const uint8_t *bytes)
{
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
}

/* �?16 位数�?按小�?格式写入字节流�?*/
static void stkPutLe16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)(value & 0xFFU);
    bytes[1] = (uint8_t)(value >> 8);
}

static uint8_t stkEnsureIcspDeviceIdVerified(void)
{
    pic_prog_params_t picParams;
    uint16_t readValue = 0U;
    uint16_t mask;
    uint16_t expect;

    if (g_stkIcspDeviceIdChecked != 0U)
        return STK_STATUS_CMD_OK;

    g_stkIcspDeviceIdChecked = 1U;

    if (g_stkDeviceIdentity.arch != 1U)
        return STK_STATUS_CMD_OK;

    if (pic8FindDeviceByIndex(g_stkDeviceIdentity.index, &picParams) != 0)
    {
        uart1_WriteString("ICSP deviceID check failed: PIC device params not found\r\n");
        return STK_STATUS_CMD_FAILED;
    }

    if (picParams.common.deviceid_addr == 0U ||
        picParams.common.deviceid_expected == 0U)
    {
        uart1_WriteString("ICSP deviceID check skipped: no valid expected deviceID\r\n");
        return STK_STATUS_CMD_OK;
    }

#if DEBUG_HARDWARE_CONFIG
    uart1_WriteString("ICSP deviceID precheck idx=");
    uart1_WriteDec(g_stkDeviceIdentity.index);
    uart1_WriteString(" addr=0x");
    uart1_WriteHex16((uint16_t)picParams.common.deviceid_addr);
    uart1_WriteString(" expect=0x");
    uart1_WriteHex16(picParams.common.deviceid_expected);
    uart1_WriteString(" mask=0x");
    uart1_WriteHex16(picParams.common.deviceid_mask);
    uart1_WriteString("\r\n");
#endif

    if (icspReadSignature(&readValue) != ICSP_OK)
    {
        uart1_WriteString("ICSP deviceID read failed\r\n");
        return STK_STATUS_CMD_FAILED;
    }

    mask = picParams.common.deviceid_mask;
    expect = picParams.common.deviceid_expected;
    if ((readValue & mask) != (expect & mask))
    {
        uart1_WriteString("ICSP deviceID mismatch: read=0x");
        uart1_WriteHex16(readValue);
        uart1_WriteString(" expect=0x");
        uart1_WriteHex16(expect);
        uart1_WriteString(" mask=0x");
        uart1_WriteHex16(mask);
        uart1_WriteString("\r\n");
        return STK_STATUS_CMD_FAILED;
    }

    uart1_WriteString("ICSP deviceID verified: read=0x");
    uart1_WriteHex16(readValue);
    uart1_WriteString(" mask=0x");
    uart1_WriteHex16(mask);
    uart1_WriteString("\r\n");
    return STK_STATUS_CMD_OK;
}

/* 保存上位机下发的器件和项�?�?份信�? 后续会写�?Raw 离线包头。�?�指令每次编程都要下�? */

static uint16_t stkNormalizeIcspConfigValue(uint8_t idx, uint16_t value)
{
    pic_prog_params_t picParams;
    uint16_t implMask;
    uint16_t cfgBitMask;
    uint16_t unimplFill;
    uint8_t cfgBits;

    if (g_stkDeviceIdentity.arch != 1U)
        return value;

    if (pic8FindDeviceByIndex(g_stkDeviceIdentity.index, &picParams) != 0)
        return value;

    if (idx >= MAX_CONFIG_WORDS || idx >= picParams.common.config_word_count)
        return value;

    implMask = picParams.common.config_dcr[idx].impl_mask;
    if (implMask == 0U)
        return value;

    cfgBits = picParams.common.config_dcr[idx].nzwidth;
    if (cfgBits >= 16U)
        cfgBitMask = 0xFFFFU;
    else
        cfgBitMask = (uint16_t)((1UL << cfgBits) - 1UL);

    if (picParams.common.config_dcr[idx].unimpl_val != 0U)
        unimplFill = (uint16_t)(~implMask);
    else
        unimplFill = (uint16_t)((picParams.common.config_dcr[idx].default_value &
                                  (uint16_t)~implMask & cfgBitMask));

    /* Host config bytes represent the PIC word width, not a 16-bit padded word. */
    return (uint16_t)(((value & implMask) | unimplFill) & cfgBitMask);
}
static uint8_t stkSetDeviceIdentity(const uint8_t *payload, uint16_t payloadLen)
{
    if (payload == NULL)
        return STK_STATUS_CMD_FAILED;

    if (payloadLen < (uint16_t)(1U + 2U + STK_PARAM_ITEM_ID_LEN + STK_PARAM_ITEM_DESC_LEN))
        return STK_STATUS_CMD_FAILED;

    g_stkDeviceIdentity.arch = payload[0];
    g_stkDeviceIdentity.index = stkGetLe16(&payload[1]);
    memcpy(g_stkDeviceIdentity.itemId, &payload[3], STK_PARAM_ITEM_ID_LEN);
    memcpy(g_stkDeviceIdentity.itemDesc,
           &payload[3 + STK_PARAM_ITEM_ID_LEN],
           STK_PARAM_ITEM_DESC_LEN);
    g_stkDeviceIdentity.itemDesc[STK_PARAM_ITEM_DESC_LEN] = '\0';
    offlinePgmerInitWith(&g_stkDeviceIdentity);
    /* DFM: resolve the PIC device for the ICSP engine (arch 1 = PIC) */
    if (g_stkDeviceIdentity.arch == 1U) {
        static pic_prog_params_t picParams;
        if (pic8FindDeviceByIndex(g_stkDeviceIdentity.index, &picParams) == 0)
            pic8Init(&picParams);
    }
    g_stkIcspDeviceIdChecked = 0U;
    return STK_STATUS_CMD_OK;
}

/* 将当前器件和项目�?份信�?打包返回给上位机�?*/
static uint16_t stkGetDeviceIdentity(uint8_t *out, uint16_t outSize)
{
    uint16_t needLen;

    if (out == NULL)
        return 0U;

    needLen = (uint16_t)(1U + 2U + STK_PARAM_ITEM_ID_LEN + STK_PARAM_ITEM_DESC_LEN);
    if (outSize < needLen)
        return 0U;

    out[0] = g_stkDeviceIdentity.arch;
    stkPutLe16(&out[1], g_stkDeviceIdentity.index);
    memcpy(&out[3], g_stkDeviceIdentity.itemId, STK_PARAM_ITEM_ID_LEN);
    memcpy(&out[3 + STK_PARAM_ITEM_ID_LEN],
           g_stkDeviceIdentity.itemDesc,
           STK_PARAM_ITEM_DESC_LEN);
    return needLen;
}

static void stkPutLe32(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)(value & 0xFFU);
    bytes[1] = (uint8_t)((value >> 8) & 0xFFU);
    bytes[2] = (uint8_t)((value >> 16) & 0xFFU);
    bytes[3] = (uint8_t)((value >> 24) & 0xFFU);
}

/* PIC flash/userid erase to (1<<inst_bits)-1: 14-bit -> 0x3FFF, 12-bit -> 0x0FFF. */
static uint16_t icspErasedWordValue(void)
{
    uint8_t bits = g_activeDeviceParams.device_params.picParam.common.inst_bits;
    if (bits == 0U)
        bits = 14U;
    return (uint16_t)((1U << bits) - 1U);
}

/* Fill a word buffer with the device erased-word pattern (record-mode base). */
static void icspFillErasedWords(uint8_t *buf, uint16_t words)
{
    uint16_t ev = icspErasedWordValue();
    uint16_t w;
    for (w = 0U; w < words; w++)
    {
        buf[w * 2U] = (uint8_t)(ev & 0xFFU);
        buf[w * 2U + 1U] = (uint8_t)(ev >> 8);
    }
}

/* 打包离线包总体信息: 有效包数量、激活包序号、最大包数量�?*/
static uint16_t stkPutOfflineInfo(uint8_t *out, uint16_t outSize)
{
    offline_package_info_t info;

    if (out == 0 || outSize < 6U)
    {
        stkTraceOfflineInfo(0, 0U);
        return 0U;
    }

    if (offlinePgmerGetOfflineInfo(&info) != 0U)
    {
        stkTraceOfflineInfo(0, 0U);
        return 0U;
    }

    stkTraceOfflineInfo(&info, 1U);
    stkPutLe16(&out[0], info.package_count);
    stkPutLe16(&out[2], info.active_index);
    stkPutLe16(&out[4], info.max_count);
    return 6U;
}

/* 打包指定离线包摘�? 供上位机查看 Flash �?的�?�录内�?��??*/
static uint16_t stkPutOfflineSummary(uint8_t *out, uint16_t outSize, uint16_t index)
{
    offline_package_index_t summary;
    uint16_t pos = 0U;
    uint16_t minSize = (uint16_t)(1U + 1U + 2U + 4U + 4U + 4U + 4U + 1U + 2U +
                                  STK_PARAM_ITEM_ID_LEN + STK_PARAM_ITEM_DESC_LEN +
                                  DEVICE_NAME_CHAR_LENGTH);

    if (out == 0 || outSize < minSize)
    {
        stkTraceOfflineSummary(index, 0, 0U);
        return 0U;
    }

    if (offlinePgmerGetPackageSummary(index, &summary) != 0U)
    {
        stkTraceOfflineSummary(index, 0, 0U);
        return 0U;
    }

    stkTraceOfflineSummary(index, &summary, 1U);
    out[pos++] = summary.used;
    out[pos++] = summary.package_state;
    stkPutLe16(&out[pos], summary.package_index); pos += 2U;
    stkPutLe32(&out[pos], summary.flash_addr); pos += 4U;
    stkPutLe32(&out[pos], summary.total_size); pos += 4U;
    stkPutLe32(&out[pos], summary.packet_count); pos += 4U;
    stkPutLe32(&out[pos], summary.crc32); pos += 4U;
    out[pos++] = summary.identity.arch;
    stkPutLe16(&out[pos], summary.identity.index); pos += 2U;
    memcpy(&out[pos], summary.identity.itemId, STK_PARAM_ITEM_ID_LEN);
    pos += STK_PARAM_ITEM_ID_LEN;
    memcpy(&out[pos], summary.identity.itemDesc, STK_PARAM_ITEM_DESC_LEN);
    pos += STK_PARAM_ITEM_DESC_LEN;
    pos += stkPutOfflineDeviceName(&out[pos],
                                   (uint16_t)(outSize - pos),
                                   summary.identity.arch,
                                   summary.identity.index);
    return pos;
}

/* 根据 STK500 payload 生成完整 TX �? 输出到调用方指定�?TX 缓冲�?*/
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
    uint8_t paramIndex = (uint8_t)(index & 0x1fU);
    uint16_t voltageX100 = (uint16_t)value * 10U;
    uint8_t hwStatus = STK_STATUS_CMD_OK;

    switch (index)
    {
    case STK_PARAM_VTARGET:
        if (MCP4017_VDD_SetVoltage(voltageX100) == 0xFFU)
            hwStatus = STK_STATUS_CMD_FAILED;
        break;

    case STK_PARAM_VADJUST:
        if (MCP4017_VPP_SetVoltage(voltageX100) == 0xFFU)
            hwStatus = STK_STATUS_CMD_FAILED;
        break;

    default:
        break;
    }

    if (hwStatus == STK_STATUS_CMD_OK)
        stkParam.bytes[paramIndex] = value;

    return hwStatus;
}

static uint8_t getParameter(uint8_t index)
{
    uint8_t paramIndex = (uint8_t)(index & 0x1fU);
    u32 voltageMv;

    switch (index)
    {
    case STK_PARAM_VTARGET:
        voltageMv = Adc_GetChannelRealValue(ADC_CH_VDD_MAIN_FBACK);
        return (uint8_t)((voltageMv >= 25500U) ? 255U : ((voltageMv + 50U) / 100U));

    case STK_PARAM_VADJUST:
        voltageMv = Adc_GetChannelRealValue(ADC_CH_VPP_MAIN_FBACK);
        return (uint8_t)((voltageMv >= 25500U) ? 255U : ((voltageMv + 50U) / 100U));

    default:
        return stkParam.bytes[paramIndex];
    }
}

/* ---- avrdude -B 编程速度 -> ICSP 位时钟挡�?----
 * 上位�?-B �?STK500 �?式编码�??STK_PARAM_SCK_DURATION(0x98) 挡位�?d:
 *   d=0:最�? d=1:4M  d=2:2M  d=3:1M  d>=4:一�?500K(封底不再降低)
 * 0xFF = �?会话�?下发 -B, 保持 pic8Init 默�?? 4MHz�?
 * 注意: -B 参数�?avrdude open 阶�?�先于器件身份下�?, 不能�?setParameter
 * 里立即应�?会�?? pic8Init 的默认档覆盖), 统一延迟�?ENTER_PROGMODE_ICSP 生效�?*/
#define STK_SCK_TIER_IDX        (STK_PARAM_SCK_DURATION & 0x1fU)
#define STK_SCK_TIER_UNSET      0xFFU

static void stkApplyIcspSckTier(uint8_t tier)
{
    uint32_t hz;

    switch (tier)
    {
    case 0U: hz = 4500000UL; break;         /* ��� */
    case 1U: hz = 4000000UL; break;
    case 2U: hz = 2000000UL; break;
    case 3U: hz = 1000000UL; break;
    default: hz = 500000UL; break;
    }
    (void)icspSetIcspClock(hz);
}

#if UART1_TRACE
/* 简洁调�? 数据来源�?*/
static const char *stkSourceName(uint8_t src)
{
    switch (src)
    {
    case STK_DATA_SOURCE_USB_HID:    return "HID";
    case STK_DATA_SOURCE_USB_CDC:    return "CDC";
    case STK_DATA_SOURCE_USB_WINUSB: return "WINUSB";
    case STK_DATA_SOURCE_FLASH_RECORD: return "FLASH";
    default:                         return "?";
    }
}


static void stkIcspTrace(const char *tag, uint32_t addr, const uint8_t *data, uint16_t n)
{
    uint16_t i;
    uart1_WriteString(tag);
    uart1_WriteString(" a=0x");
    uart1_WriteHex8((uint8_t)(addr >> 24U));
    uart1_WriteHex8((uint8_t)(addr >> 16U));
    uart1_WriteHex8((uint8_t)(addr >> 8U));
    uart1_WriteHex8((uint8_t)addr);
    if (data != NULL && n != 0U)
    {
        uart1_WriteString(" d:");
        for (i = 0U; i < n; i++)
        {
            uart1_WriteString(" ");
            uart1_WriteHex8(data[i]);
        }
    }
    uart1_WriteString("\r\n");
}
#endif

/* 解析一帧完�?STK500 数据。USB 来源会进�?HID TX, Flash 来源�?生成�?�?TX 判定结果�?*/
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

#if UART1_TRACE
    /* 简洁调�? �?打印数据来源、方向、命�?ID、数�?包大小�??*/
    uart1_WriteString(stkSourceName(pDataFrame->source));
    uart1_WriteString(" RX cmd=0x");
    uart1_WriteHex8(pRx[STK_TXMSG_START]);
    uart1_WriteString(" len=");
    uart1_WriteDec(pDataFrame->frameLen);
    uart1_WriteString("\r\n");
#endif

    cmd = pRx[STK_TXMSG_START];
    pTx[STK_TXMSG_START] = cmd;
    pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_OK;
    param = (void *)(pRx + STK_TXMSG_START + 1);

    if ((pDataFrame->source == STK_DATA_SOURCE_USB_HID ||
         pDataFrame->source == STK_DATA_SOURCE_USB_CDC ||
         pDataFrame->source == STK_DATA_SOURCE_USB_WINUSB) &&
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
        /* Reset to online only for host USB sessions; never during flash replay. */
        if (pDataFrame->source == STK_DATA_SOURCE_USB_HID ||
            pDataFrame->source == STK_DATA_SOURCE_USB_CDC ||
            pDataFrame->source == STK_DATA_SOURCE_USB_WINUSB)
        {
            stkResetAllProgrammingSessions();
            (void)stkSetWorkMode(STK500_WORK_MODE_ONLINE);
        }
        stkParam.bytes[STK_SCK_TIER_IDX] = STK_SCK_TIER_UNSET;  /* 新会�? -B 挡位待下�?*/
        /* 获取烧录器标识�?*/
        static const char string[] = PROGRAMMER_ID_STR;
        memcpy(&pTx[STK_TXMSG_START + 2], string, sizeof(string));
        len.bytes[0] = (uint8_t)(sizeof(string) + 1U);
    SWITCH_CASE(STK_CMD_SET_WORK_STATE)
        /* 设置工作模式: 0=simulate, 1=online, 2=record, 3=online+record。开机默�?online */
        if (pRx[STK_TXMSG_START + 1] == STK500_WORK_MODE_ONLINE ||
            pRx[STK_TXMSG_START + 1] == STK500_WORK_MODE_RECORD)
        {
            if (stkSetWorkMode(pRx[STK_TXMSG_START + 1]) == 0U)
                pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_FAILED;
            else
                pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_OK;
        }
        else
        {
            pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_FAILED;
        }
    SWITCH_CASE(STK_CMD_SET_PROG_STATE)
        /* 设置编程会话状�? 0=STOP_PROG 关闭离线�? 1=START_PROG 创建离线包�?----�?指令�?在“�?�录”模式下才会下发---- */
        if (pRx[STK_TXMSG_START + 1] > STK500_PROGRAM_RECORDING)
        {
            pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_FAILED;
        }
        else if (pRx[STK_TXMSG_START + 1] == STK500_PROGRAM_RECORDING)
        {
            if (g_stkProgrammerState == STK500_PROGRAM_RECORDING)
            {
                /* Idempotent: already recording, keep current package. */
                pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_OK;
            }
            else if (offlinePgmerRawBegin(&g_stkDeviceIdentity) != 0U)
            {
                pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_FAILED;
            }
            else
            {
                g_stkProgrammerState = STK500_PROGRAM_RECORDING;
                pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_OK;
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
    SWITCH_CASE(STK_CMD_SET_PARAMETER)  /* 设置 STK 参数或器件身份信�?�??*/
        if (pRx[STK_TXMSG_START + 1] == STK_PARAM_DEVICE_IDENTITY)// 设置器件“身份信�?�??
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
#if UART1_TRACE
        /* 诊断�?：打印上位机每�?�下发的字地址，确认是否在同一页反复�?�写�??*/
        uart1_WriteString(stkSourceName(pDataFrame->source));
        uart1_WriteString(" LOAD addr=0x");
        uart1_WriteHex8(stkAddress.bytes[3]);
        uart1_WriteHex8(stkAddress.bytes[2]);
        uart1_WriteHex8(stkAddress.bytes[1]);
        uart1_WriteHex8(stkAddress.bytes[0]);
        uart1_WriteString("\r\n");
#endif
    SWITCH_CASE(STK_CMD_FIRMWARE_UPGRADE)   /* 该命令由上位机在升级固件前发�? 以便烧录器进入升级模式�?*/
        {
            /* Payload must carry the magic bytes: cmd + 0xA5 0x5A. */
            if (payloadLen >= 3U &&
                pRx[STK_TXMSG_START + 1] == STK_FW_UPGRADE_MAGIC0 &&
                pRx[STK_TXMSG_START + 2] == STK_FW_UPGRADE_MAGIC1)
            {
                /* Only act for online USB sources; flash replay only replies, no side effects. */
                if (pDataFrame->source == STK_DATA_SOURCE_USB_HID ||
                    pDataFrame->source == STK_DATA_SOURCE_USB_CDC ||
                    pDataFrame->source == STK_DATA_SOURCE_USB_WINUSB)
                {
                    if (stkBootCtrlSaveState(BOOT_APP_STATE_FORCE_BOOT, 0U, 0U))
                    {
                        pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_OK;
                        g_stkFwUpgradePending = 1U;
                    }
                    else
                    {
                        pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_FAILED;
                    }
                }
                else
                {
                    /* Non-USB replay source: explicit reply, no side effects. */
                    pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_FAILED;
                }
            }
            else
            {
                pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_FAILED;
            }
        }
    SWITCH_CASE(STK_CMD_SET_CONTROL_STACK)
        /* AVR Studio 探测能力时会发送�?�命�?, 这里保持 AVR-Doper 的兼容�?�为�??*/
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
        {
            stkReadFlashHvsp_t *rdParam = (stkReadFlashHvsp_t *)param;
            uint16_t numBytes = ((uint16_t)rdParam->numBytes[0] << 8) | rdParam->numBytes[1];
            memset(&pTx[STK_TXMSG_START + 2], 0xFF, numBytes);
            (void)offlinePgmerRawReadBack(STK_CMD_READ_FLASH_HVSP, stkAddress.dword,
                                          pRx, &pTx[STK_TXMSG_START + 2], numBytes);
            pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_OK;
            len.word = (uint16_t)(numBytes + 2U);
        }
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
        {
            stkReadFlashHvsp_t *rdParam = (stkReadFlashHvsp_t *)param;
            uint16_t numBytes = ((uint16_t)rdParam->numBytes[0] << 8) | rdParam->numBytes[1];
            memset(&pTx[STK_TXMSG_START + 2], 0xFF, numBytes);
            (void)offlinePgmerRawReadBack(STK_CMD_READ_EEPROM_HVSP, stkAddress.dword,
                                          pRx, &pTx[STK_TXMSG_START + 2], numBytes);
            pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_OK;
            len.word = (uint16_t)(numBytes + 2U);
        }
    SWITCH_CASE(STK_CMD_PROGRAM_FUSE_HVSP)
        {
            stkProgramFuseHvsp_t *hvParam = (stkProgramFuseHvsp_t *)param;
            uint8_t onlineStatus = stkIsOnlineMode() ?
                hvspProgramFuse(hvParam) : STK_STATUS_CMD_OK;
            uint8_t recStatus = STK_STATUS_CMD_OK;
            pTx[STK_TXMSG_START + 1] = stkProgramStatus(onlineStatus, recStatus);
        }
    SWITCH_CASE(STK_CMD_READ_FUSE_HVSP)
        if (stkIsOnlineMode())
        {
            pTx[STK_TXMSG_START + 2] = hvspReadFuse((stkReadFuseHvsp_t *)param);
            pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_OK;
        }
        else
        {
            uint8_t rdValue = 0xFFU;
            if (offlinePgmerRawReadBack(STK_CMD_READ_FUSE_HVSP, 0U, pRx, &rdValue, 1U) != 0U)
                pTx[STK_TXMSG_START + 2] = rdValue;
            else
                pTx[STK_TXMSG_START + 2] = 0xFFU;
            pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_OK;
        }
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
        if (stkIsOnlineMode())
        {
            pTx[STK_TXMSG_START + 2] = hvspReadLock();
            pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_OK;
        }
        else
        {
            uint8_t rdValue = 0xFFU;
            if (offlinePgmerRawReadBack(STK_CMD_READ_LOCK_HVSP, 0U, pRx, &rdValue, 1U) != 0U)
                pTx[STK_TXMSG_START + 2] = rdValue;
            else
                pTx[STK_TXMSG_START + 2] = 0xFFU;
            pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_OK;
        }
        len.bytes[0] = 3;
    SWITCH_CASE(STK_CMD_READ_SIGNATURE_HVSP)
        if (stkIsOnlineMode())
        {
            pTx[STK_TXMSG_START + 2] = hvspReadSignature((stkReadFuseHvsp_t *)param);
            pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_OK;
        }
        else
        {
            /* Record mode: report the signature from the AVR device table. */
            uint8_t rdValue = 0xFFU;
            if (g_activeDeviceParams.device_arch == STK_MCU_ARCH_AVR)
            {
                stkReadFuseHvsp_t *rdParam = (stkReadFuseHvsp_t *)param;
                uint8_t idx = (uint8_t)(rdParam->fuseAddress & 0x0FU);
                if (idx < 3U)
                    rdValue = g_activeDeviceParams.device_params.avrParam.signature[idx];
                pTx[STK_TXMSG_START + 2] = rdValue;
                pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_OK;
            }
            else
            {
                pTx[STK_TXMSG_START + 2] = 0xFFU;
                pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_FAILED;
            }
        }
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
            uint8_t onlineStatus = STK_STATUS_CMD_OK;
            if (stkIsOnlineMode())
                onlineStatus = ispEnterProgmode((stkEnterProgIsp_t *)param);
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
#if UART1_TRACE
            /* 诊断�?：打印页写命令实际返回的状态（0=OK）�?*/
            uart1_WriteString(stkSourceName(pDataFrame->source));
            uart1_WriteString(" WRITE st=0x");
            uart1_WriteHex8(pTx[STK_TXMSG_START + 1]);
            uart1_WriteString("\r\n");
#endif
        }
    SWITCH_CASE(STK_CMD_READ_FLASH_ISP)
        if (stkIsOnlineMode())
            len.word = 1 + ispReadMemory((stkReadFlashIsp_t *)param, (void *)&pTx[STK_TXMSG_START + 1], 0);
        else
        {
            stkReadFlashIsp_t *rdParam = (stkReadFlashIsp_t *)param;
            uint16_t numBytes = ((uint16_t)rdParam->numBytes[0] << 8) | rdParam->numBytes[1];
            /* Record mode read-back: start from all-0xFF, then splice the
             * recorded pages within the requested address range. */
            memset(&pTx[STK_TXMSG_START + 2], 0xFF, numBytes);
            (void)offlinePgmerRawReadBack(STK_CMD_READ_FLASH_ISP, stkAddress.dword,
                                          pRx, &pTx[STK_TXMSG_START + 2], numBytes);
            pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_OK;
            len.word = (uint16_t)(numBytes + 2U);
        }
#if UART1_TRACE
        /* 诊断�?：打印�?�回数据�?4 字节�?xFF 说明�?标片没写进去）�??*/
        uart1_WriteString(stkSourceName(pDataFrame->source));
        uart1_WriteString(" READ data=");
        uart1_WriteHex8(pTx[STK_TXMSG_START + 2]);
        uart1_WriteHex8(pTx[STK_TXMSG_START + 3]);
        uart1_WriteHex8(pTx[STK_TXMSG_START + 4]);
        uart1_WriteHex8(pTx[STK_TXMSG_START + 5]);
        uart1_WriteString("\r\n");
#endif
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
        {
            stkReadFlashIsp_t *rdParam = (stkReadFlashIsp_t *)param;
            uint16_t numBytes = ((uint16_t)rdParam->numBytes[0] << 8) | rdParam->numBytes[1];
            memset(&pTx[STK_TXMSG_START + 2], 0xFF, numBytes);
            (void)offlinePgmerRawReadBack(STK_CMD_READ_EEPROM_ISP, stkAddress.dword,
                                          pRx, &pTx[STK_TXMSG_START + 2], numBytes);
            pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_OK;
            len.word = (uint16_t)(numBytes + 2U);
        }
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
            uint8_t rdValue = 0xFFU;
            if (cmd == STK_CMD_READ_FUSE_ISP || cmd == STK_CMD_READ_LOCK_ISP)
            {
                /* Record mode: reply with the value recorded by the matching
                 * PROGRAM_FUSE/LOCK frame on the board flash. */
                if (offlinePgmerRawReadBack(cmd, 0U, pRx, &rdValue, 1U) != 0U)
                    pTx[STK_TXMSG_START + 2] = rdValue;
                else
                    pTx[STK_TXMSG_START + 2] = 0xFFU;
                pTx[STK_TXMSG_START + 3] = STK_STATUS_CMD_OK;
            }
            else if (cmd == STK_CMD_READ_SIGNATURE_ISP)
            {
                /* Record mode: report the signature from the AVR device table
                 * resolved via the DEVICE_IDENTITY set by the host. */
                if (g_activeDeviceParams.device_arch == STK_MCU_ARCH_AVR)
                {
                    stkReadFuseIsp_t *rdParam = (stkReadFuseIsp_t *)param;
                    uint8_t idx = (uint8_t)(rdParam->cmd[2] & 0x0FU);
                    if (idx < 3U)
                        rdValue = g_activeDeviceParams.device_params.avrParam.signature[idx];
                    pTx[STK_TXMSG_START + 2] = rdValue;
                    pTx[STK_TXMSG_START + 3] = STK_STATUS_CMD_OK;
                }
                else
                {
                    pTx[STK_TXMSG_START + 2] = 0xFFU;
                    pTx[STK_TXMSG_START + 3] = STK_STATUS_CMD_FAILED;
                }
            }
            else /* STK_CMD_READ_OSCCAL_ISP: not simulated */
            {
                pTx[STK_TXMSG_START + 2] = 0xFFU;
                pTx[STK_TXMSG_START + 3] = STK_STATUS_CMD_FAILED;
            }
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
        /*------------------- PIC ICSP 命令处理开�?-------------------*/
    SWITCH_CASE(STK_CMD_ENTER_PROGMODE_ICSP)
        {
            /* avrdude -B: 挡位在进入编程模式前生效 (仅在线模式有�?作�?? */
            if (stkIsOnlineMode() &&
                stkParam.bytes[STK_SCK_TIER_IDX] != STK_SCK_TIER_UNSET)
            {
                stkApplyIcspSckTier(stkParam.bytes[STK_SCK_TIER_IDX]);
            }
            /* 进入 ICSP 模式�? 上位机下发模�? 0=高压, 1=低压�?*/
            uint8_t onlineStatus = stkIsOnlineMode() ?
                pic8EnterProgmode((uint8_t)(pRx[STK_TXMSG_START + 1] & 0x01U)) :
                STK_STATUS_CMD_OK;
            g_stkIcspDeviceIdChecked = 0U;
#if UART1_TRACE
            stkIcspTrace("ICSP40", 0U, &pRx[STK_TXMSG_START + 1], 4U);
#endif
#if DEBUG_HARDWARE_CONFIG
            uart1_WriteString("ICSP40 enter mode=");
            uart1_WriteDec((uint8_t)(pRx[STK_TXMSG_START + 1] & 0x01U));
            uart1_WriteString(" status=0x");
            uart1_WriteHex8(onlineStatus);
            uart1_WriteString("\r\n");
#endif
            if (onlineStatus == STK_STATUS_CMD_OK &&
                stkGetWorkMode() == STK500_WORK_MODE_REPLAY)
            {
                u32 vddMv;
                u32 vppMv;
                delay_ms(30);
                vddMv = Adc_GetChannelRealValue(ADC_CH_VDD_FBACK);
                vppMv = Adc_GetChannelRealValue(ADC_CH_DUT_UVPP);
#if DEBUG_HARDWARE_CONFIG
                uart1_WriteString("ICSP40 replay settle vdd=");
                uart1_WriteDec(vddMv);
                uart1_WriteString("mV vpp=");
                uart1_WriteDec(vppMv);
                uart1_WriteString("mV\r\n");
#endif
            }
            pTx[STK_TXMSG_START + 1] = (onlineStatus != STK_STATUS_CMD_OK) ?
                STK_STATUS_CMD_FAILED : STK_STATUS_CMD_OK;
        }
    SWITCH_CASE(STK_CMD_LEAVE_PROGMODE_ICSP)
        {
            if (stkIsOnlineMode())
                pic8LeaveProgmode();
            g_stkIcspDeviceIdChecked = 0U;
#if UART1_TRACE
            stkIcspTrace("ICSP41", stkAddress.dword, NULL, 0U);
#endif
        }
    SWITCH_CASE(STK_CMD_CHIP_ERASE_ICSP)
        {
            uint8_t eraseStatus = STK_STATUS_CMD_OK;
            if (stkIsOnlineMode())
            {
                eraseStatus = stkEnsureIcspDeviceIdVerified();
                if (eraseStatus == STK_STATUS_CMD_OK)
                {
                    uint8_t icspStatus = icspBulkErase();
                    if (icspStatus == ICSP_ERR_CAL_LOST)
                        eraseStatus = STK_STATUS_CAL_LOST;
                    else if (icspStatus != ICSP_OK)
                        eraseStatus = STK_STATUS_CMD_FAILED;
                }
            }
#if UART1_TRACE
            stkIcspTrace("ICSP42", stkAddress.dword, &pRx[STK_TXMSG_START + 1], 2U);
#endif
            pTx[STK_TXMSG_START + 1] = (eraseStatus == STK_STATUS_CAL_LOST) ?
                STK_STATUS_CAL_LOST :
                ((eraseStatus != STK_STATUS_CMD_OK) ? STK_STATUS_CMD_FAILED : STK_STATUS_CMD_OK);
#if DEBUG_HARDWARE_CONFIG
            if (eraseStatus != STK_STATUS_CMD_OK)
            {
                uart1_WriteString("ICSP42 fail eraseStatus=0x");
                uart1_WriteHex8(eraseStatus);
                uart1_WriteString("\r\n");
            }
#endif
        }
    SWITCH_CASE(STK_CMD_PROGRAM_FLASH_ICSP)
        {
            stkProgramFlashIcsp_t *icspParam = (stkProgramFlashIcsp_t *)param;
            uint8_t onlineStatus = STK_STATUS_CMD_OK;
            if (stkIsOnlineMode())
            {
                onlineStatus = stkEnsureIcspDeviceIdVerified();
                if (onlineStatus == STK_STATUS_CMD_OK)
                    onlineStatus = icspProgramMemory(icspParam, 0U);
            }
#if UART1_TRACE
            stkIcspTrace("ICSP43", stkAddress.dword, &pRx[STK_TXMSG_START + 1],
                         (uint16_t)((payloadLen > 1U) ? (payloadLen - 1U) : 0U));
#endif
            pTx[STK_TXMSG_START + 1] = onlineStatus;
        }
    SWITCH_CASE(STK_CMD_READ_FLASH_ICSP)
        {
#if UART1_TRACE
            stkIcspTrace("ICSP44Q", stkAddress.dword, &pRx[STK_TXMSG_START + 1], 3U);
#endif
            if (stkIsOnlineMode())
                len.word = 1 + icspReadMemory((stkReadFlashIcsp_t *)param,
                                              (stkReadFlashIcspResult_t *)&pTx[STK_TXMSG_START + 1],
                                              0U);
            else
            {
                stkReadFlashIcsp_t *rdParam = (stkReadFlashIcsp_t *)param;
                uint16_t numWords = stkGetLe16(rdParam->numWords);
                uint16_t numBytes = (uint16_t)(numWords * 2U);
                icspFillErasedWords(&pTx[STK_TXMSG_START + 2], numWords);
                (void)offlinePgmerRawReadBack(STK_CMD_READ_FLASH_ICSP, stkAddress.dword, pRx,
                                              &pTx[STK_TXMSG_START + 2], numBytes);
                pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_OK;
                len.word = (uint16_t)(numBytes + 2U);
            }
#if UART1_TRACE
            stkIcspTrace("ICSP44A", stkAddress.dword, &pTx[STK_TXMSG_START + 2],
                         (uint16_t)((len.word > 2U) ? (len.word - 2U) : 0U));
#endif
        }
    SWITCH_CASE(STK_CMD_PROGRAM_EEPROM_ICSP)
        {
            stkProgramEepromIcsp_t *icspParam = (stkProgramEepromIcsp_t *)param;
            uint8_t onlineStatus = STK_STATUS_CMD_OK;
            if (stkIsOnlineMode())
            {
                onlineStatus = stkEnsureIcspDeviceIdVerified();
                if (onlineStatus == STK_STATUS_CMD_OK)
                    onlineStatus = icspProgramMemory((stkProgramFlashIcsp_t *)icspParam, 1U);
            }
#if UART1_TRACE
            stkIcspTrace("ICSP45", stkAddress.dword, &pRx[STK_TXMSG_START + 1],
                         (uint16_t)((payloadLen > 1U) ? (payloadLen - 1U) : 0U));
#endif
            pTx[STK_TXMSG_START + 1] = onlineStatus;
        }
    SWITCH_CASE(STK_CMD_READ_EEPROM_ICSP)
        {
#if UART1_TRACE
            stkIcspTrace("ICSP46Q", stkAddress.dword, &pRx[STK_TXMSG_START + 1], 3U);
#endif
            if (stkIsOnlineMode())
                len.word = 1 + icspReadMemory((stkReadFlashIcsp_t *)param,
                                              (stkReadFlashIcspResult_t *)&pTx[STK_TXMSG_START + 1],
                                              1U);
            else
            {
                stkReadFlashIcsp_t *rdParam = (stkReadFlashIcsp_t *)param;
                uint16_t numBytes = stkGetLe16(rdParam->numWords);
                memset(&pTx[STK_TXMSG_START + 2], 0xFF, numBytes);
                (void)offlinePgmerRawReadBack(STK_CMD_READ_EEPROM_ICSP, stkAddress.dword, pRx,
                                              &pTx[STK_TXMSG_START + 2], numBytes);
                pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_OK;
                len.word = (uint16_t)(numBytes + 2U);
            }
#if UART1_TRACE
            stkIcspTrace("ICSP46A", stkAddress.dword, &pTx[STK_TXMSG_START + 2],
                         (uint16_t)((len.word > 2U) ? (len.word - 2U) : 0U));
#endif
        }
    SWITCH_CASE(STK_CMD_PROGRAM_CONFIG_ICSP)
        {
            stkProgramFlashIcsp_t *icspParam = (stkProgramFlashIcsp_t *)param;
            uint16_t cfgCount = stkGetLe16(icspParam->numWords);
            uint16_t cfgIdx;
            uint8_t onlineStatus = STK_STATUS_CMD_OK;
            if (stkIsOnlineMode())
            {
                onlineStatus = stkEnsureIcspDeviceIdVerified();
                #if UART1_TRACE
                uart1_WriteString("ICSP47 raw=");
                uart1_WriteHex16(stkGetLe16(icspParam->data));
                uart1_WriteString("\r\n");
                #endif
                for (cfgIdx = 0U; cfgIdx < cfgCount && cfgIdx < MAX_CONFIG_WORDS && onlineStatus == STK_STATUS_CMD_OK; cfgIdx++)
                {
                    uint16_t cfgValue = stkGetLe16(&icspParam->data[cfgIdx * 2U]);
                    onlineStatus = (icspProgCfg(cfgIdx, cfgValue) != ICSP_OK) ?
                                   STK_STATUS_CMD_FAILED : STK_STATUS_CMD_OK;
                }
            }
            pTx[STK_TXMSG_START + 1] = (onlineStatus != STK_STATUS_CMD_OK) ?
                STK_STATUS_CMD_FAILED : STK_STATUS_CMD_OK;
        }
    SWITCH_CASE(STK_CMD_READ_CONFIG_ICSP)
        {
            stkReadFlashIcsp_t *icspParam = (stkReadFlashIcsp_t *)param;
            uint16_t cfgCount = stkGetLe16(icspParam->numWords);
            uint16_t cfgIdx;
            pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_OK;
            if (stkIsOnlineMode())
            {
                for (cfgIdx = 0U; cfgIdx < cfgCount && cfgIdx < MAX_CONFIG_WORDS; cfgIdx++)
                {
                    uint16_t cfgValue = icspReadCfg(cfgIdx);
                    #if UART1_TRACE
                    uint16_t rawCfgValue = cfgValue;
                    #endif
                    if (cfgValue == 0xFFFFU)
                    {
                        pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_FAILED;
                        break;
                    }
                    cfgValue = stkNormalizeIcspConfigValue((uint8_t)cfgIdx, cfgValue);
                    #if UART1_TRACE
                    uart1_WriteString("ICSP cfg host idx=");
                    uart1_WriteDec(cfgIdx);
                    uart1_WriteString(" raw=0x");
                    uart1_WriteHex16(rawCfgValue);
                    uart1_WriteString(" norm=0x");
                    uart1_WriteHex16(cfgValue);
                    uart1_WriteString("\r\n");
                    #endif
                    stkPutLe16(&pTx[STK_TXMSG_START + 2 + cfgIdx * 2U], cfgValue);
                }
            }
            else
            {
                uint16_t cfgBytes = (uint16_t)(cfgCount * 2U);
                memset(&pTx[STK_TXMSG_START + 2], 0xFF, cfgBytes);
                (void)offlinePgmerRawReadBack(STK_CMD_READ_CONFIG_ICSP, 0U, pRx,
                                              &pTx[STK_TXMSG_START + 2], cfgBytes);
                pTx[STK_TXMSG_START + 1] = STK_STATUS_CMD_OK;
            }
            /* cmd echo + status + cfgCount*2 data bytes */
            len.word = (uint16_t)(2U + cfgCount * 2U);
        }
    SWITCH_CASE(STK_CMD_PROGRAM_USER_ID_ICSP)
        {
            stkProgramFlashIcsp_t *icspParam = (stkProgramFlashIcsp_t *)param;
            uint16_t uidCount = stkGetLe16(icspParam->numWords);
            uint8_t onlineStatus = STK_STATUS_CMD_OK;
            if (stkIsOnlineMode())
            {
                onlineStatus = stkEnsureIcspDeviceIdVerified();
                if (onlineStatus == STK_STATUS_CMD_OK)
                    onlineStatus = (icspProgUserIdWords(stkAddress.dword, icspParam->data, uidCount) != ICSP_OK) ?
                                   STK_STATUS_CMD_FAILED : STK_STATUS_CMD_OK;
                if (onlineStatus == STK_STATUS_CMD_OK && uidCount != 0U)
                    stkAddress.dword += uidCount;
            }
            pTx[STK_TXMSG_START + 1] = (onlineStatus != STK_STATUS_CMD_OK) ?
                STK_STATUS_CMD_FAILED : STK_STATUS_CMD_OK;
        }
    SWITCH_CASE(STK_CMD_READ_USER_ID_ICSP)
        {
            stkReadFlashIcsp_t *icspParam = (stkReadFlashIcsp_t *)param;
            uint16_t uidCount = stkGetLe16(icspParam->numWords);
            uint8_t rdStatus = STK_STATUS_CMD_FAILED;
            if (stkIsOnlineMode())
            {
                rdStatus = (icspReadUserIdWords(stkAddress.dword, (uint8_t *)&pTx[STK_TXMSG_START + 2], uidCount) != ICSP_OK) ?
                           STK_STATUS_CMD_FAILED : STK_STATUS_CMD_OK;
            }
            else
            {
                uint16_t uidBytes = (uint16_t)(uidCount * 2U);
                icspFillErasedWords(&pTx[STK_TXMSG_START + 2], uidCount);
                (void)offlinePgmerRawReadBack(STK_CMD_READ_USER_ID_ICSP, stkAddress.dword, pRx,
                                              &pTx[STK_TXMSG_START + 2], uidBytes);
                rdStatus = STK_STATUS_CMD_OK;
            }
            pTx[STK_TXMSG_START + 1] = rdStatus;
            /* cmd echo + status + uidCount*2 data bytes */
            len.word = (uint16_t)(2U + uidCount * 2U);
        }
    SWITCH_CASE(STK_CMD_READ_SIGNATURE_ICSP)
        {
            uint16_t value = 0U;
            uint8_t sigStatus = STK_STATUS_CMD_FAILED;
            uint8_t online = stkIsOnlineMode();
            if (online)
            {
                if (icspReadSignature(&value) == ICSP_OK)
                    sigStatus = STK_STATUS_CMD_OK;
            }
            else if (g_activeDeviceParams.device_arch == STK_MCU_ARCH_PIC)
            {
                /* Record mode: report the expected deviceID from the table. */
                value = g_activeDeviceParams.device_params.picParam.common.deviceid_expected;
                sigStatus = STK_STATUS_CMD_OK;
            }
            pTx[STK_TXMSG_START + 1] = sigStatus;
            stkPutLe16(&pTx[STK_TXMSG_START + 2], value);
            len.bytes[0] = 4;
#if DEBUG_HARDWARE_CONFIG
            uart1_WriteString("ICSP4B src=");
            uart1_WriteDec(pDataFrame->source);
            uart1_WriteString(" online=");
            uart1_WriteDec(online);
            uart1_WriteString(" arch=");
            uart1_WriteDec(g_activeDeviceParams.device_arch);
            uart1_WriteString(" idx=");
            uart1_WriteDec(g_stkDeviceIdentity.index);
            uart1_WriteString(" status=0x");
            uart1_WriteHex8(sigStatus);
            uart1_WriteString(" value=0x");
            uart1_WriteHex16(value);
            uart1_WriteString("\r\n");
#endif
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
            uint8_t onlineStatus = STK_STATUS_CMD_OK;
            if (stkIsOnlineMode())
            {
                onlineStatus = stkEnsureIcspDeviceIdVerified();
                if (onlineStatus == STK_STATUS_CMD_OK)
                    onlineStatus = icspWriteOSCCAL(icspParam->index, value);
            }
            pTx[STK_TXMSG_START + 1] = (onlineStatus != STK_STATUS_CMD_OK) ?
                STK_STATUS_CMD_FAILED : STK_STATUS_CMD_OK;
        }
    /* PIC ICSP 命令处理结束�?*/
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

#if UART1_TRACE
    if (pDataFrame->txFrameLen != 0U)
    {
        uart1_WriteString(stkSourceName(pDataFrame->source));
        uart1_WriteString(" TX cmd=0x");
        uart1_WriteHex8(cmd);
        uart1_WriteString(" len=");
        uart1_WriteDec(pDataFrame->txFrameLen);
        uart1_WriteString("\r\n");
    }
#endif
}

/* USB HID 在线入口: 逐字节组�?STK500 帧并校验 XOR 校验和�?*/
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

/* 主循�?�?询入�? �?USB RX/TX 缓冲包�?��??stkDataFrame_t 后交给协�?解析�??*/
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
