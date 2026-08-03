/*
 * USB HID用户层实现 - HID报告收发与应用层接口
 */

/*
 * USB HID Data Relay Layer - STK500 protocol integration
 * Ported from AVR-Doper firmware/main.c (C. Starkjohann, obdev.at)
 */

#include "usb_hid_user.h"
#include "Stk500Protocol.h"
#include "Hardware_Config.h"
#include "usart.h"

uint8_t  g_HidReportId = 0;
RequestType_t g_RequestType = REQUEST_TYPE_IDLE;

static uint16_t HID_GetPayloadSize(uint8_t reportId)
{
    switch (reportId)
    {
        case 1: return 14U;
        case 2: return 30U;
        case 3: return 62U;
        case 4: return 126U;
        default: return 0U;
    }
}

void HID_BeginReportRequest(uint8_t reportId, RequestType_t requestType)
{
    g_HidReportId = reportId;
    g_RequestType = requestType;
}

void HID_Rx_Store(uint8_t reportId, const uint8_t *data, uint8_t len)
{
    uint8_t i;
    uint8_t stkPayloadStart;
    uint8_t stkLen;

    if (data == NULL || len == 0U)
    {
        HID_ResetRequestState();
        return;
    }

    /*
     * AVR-Doper hid_send_feature_report() 发送格式:
     *   byte 0  = Report ID (0x01-0x04)
     *   byte 1  = 本 report 内的有效 STK 字节数
     *   byte 2..= STK message bytes
     *
     * hidapi 会按完整 Feature Report 长度发送，尾部可能是填充字节。
     * 这里必须严格按 byte1 取有效数据，不能把填充字节送进 STK 状态机。
     */
    if (len >= 2U &&
        data[0] == reportId &&
        reportId >= 1U && reportId <= 4U)
    {
        stkPayloadStart = 2U;
        stkLen = data[1];
        if (stkLen > (uint8_t)(len - stkPayloadStart))
        {
            stkLen = (uint8_t)(len - stkPayloadStart);
        }
    }
    else
    {
        /* 兼容旧调试数据: 找到 STK_STX 后，只送入后续实际存在的字节。 */
        stkPayloadStart = len;
        stkLen = 0U;
        for (i = 0; i < len; i++)
        {
            if (data[i] == STK_STX)
            {
                stkPayloadStart = i;
                stkLen = (uint8_t)(len - i);
                break;
            }
        }
    }

    for (i = stkPayloadStart; i < (uint8_t)(stkPayloadStart + stkLen) && i < len; i++)
    {
        stkSetRxChar(data[i]);
    }

    stkPoll();
    HID_ResetRequestState();
}
void HID_Task(void)
{
    stkPoll();
}

uint8_t HID_Rx_IsAvailable(void)
{
    return 0;
}

uint8_t HID_Rx_Read(uint8_t *buf, uint8_t maxLen)
{
    (void)buf;
    (void)maxLen;
    return 0;
}

uint8_t *HID_GetReport_Buffer(uint8_t reportId, uint16_t requestedLen, uint16_t *pOutLen)
{
    static uint8_t reportBuf[HID_REPORT_MAX_LOAD];
    uint16_t reportLen;
    uint16_t payloadSize;
    uint16_t txCount;
    uint16_t idx;
    int c;

    if (pOutLen == NULL) { return NULL; }

    payloadSize = HID_GetPayloadSize(reportId);
    if (payloadSize == 0U) { *pOutLen = 0; return NULL; }

    reportLen = (uint16_t)(payloadSize + 1U);
    if (requestedLen != 0U && requestedLen < reportLen) { reportLen = requestedLen; }
    if (reportLen > HID_REPORT_MAX_LOAD) { reportLen = HID_REPORT_MAX_LOAD; }
    if (reportLen < 2U) { *pOutLen = 0; return NULL; }

    /* Zero-fill the whole output report */
    for (idx = 0; idx < reportLen; idx++) { reportBuf[idx] = 0; }

    reportBuf[0] = reportId;

    txCount = (uint16_t)stkGetTxCount();
    reportBuf[1] = (uint8_t)(txCount & 0xFF);

    idx = 2U;
    while (idx < reportLen && (c = stkGetTxByte()) >= 0)
    {
        reportBuf[idx++] = (uint8_t)c;
    }

#if DEBUG_HARDWARE_CONFIG
    {
        static const char hexTable[] = "0123456789ABCDEF";
        uint16_t n;
        uart1_WriteString("HID_GET_REPORT id=");
        uart1_WriteByte((uint8_t)('0' + reportId));
        uart1_WriteString(" len=");
        uart1_WriteByte((uint8_t)hexTable[(reportLen >> 4) & 0x0F]);
        uart1_WriteByte((uint8_t)hexTable[reportLen & 0x0F]);
        uart1_WriteString(" data:");
        for (n = 0; n < reportLen; n++)
        {
            uart1_WriteByte((uint8_t)hexTable[(reportBuf[n] >> 4) & 0x0F]);
            uart1_WriteByte((uint8_t)hexTable[reportBuf[n] & 0x0F]);
            uart1_WriteByte(' ');
        }
        uart1_WriteString("\r\n");
    }
#endif
    *pOutLen = reportLen;
    return reportBuf;
}

void HID_ResetRequestState(void)
{
    g_RequestType = REQUEST_TYPE_IDLE;
    g_HidReportId = 0;
}


