/*
 * USB HID User Implementation - Interrupt Endpoint Transport
 *
 * Migration from EP0 Feature Reports to EP1 Interrupt IN/OUT endpoints.
 * Modeled after CMSIS-DAP HID transport.
 *
 * Architecture:
 *   EP1 OUT (host->device): ISR stores bytes in ring buffer, returns immediately
 *   Main loop: HID_Task() drains ring buffer, feeds STK parser, calls stkPoll()
 *   EP1 IN (device->host): HID_Tx_Flush() pushes STK txBuffer data via EP1 IN
 *
 * AVR-Doper wire format (preserved):
 *   byte 0 = Report ID (1-2)
 *   byte 1 = payload length
 *   byte 2.. = STK message bytes
 */

#include "usb_hid_user.h"
#include "usb_lib.h"
#include "usb_conf.h"
#include "Stk500Protocol.h"
#include "usart.h"

/* ---- EP1 OUT: ring buffer for bytes from host ---- */
static uint8_t  g_hidRxBuf[HID_RX_RING_SIZE];
static uint16_t g_hidRxHead;
static uint16_t g_hidRxTail;

/* ---- EP1 IN: TX state ---- */
static uint8_t  g_hidTxBusy;

/* ---- Helper: choose report ID based on payload size ---- */
static uint8_t HID_ChooseReportId(uint16_t payloadLen)
{
    if (payloadLen <= 13U) return 1U;   /* Report 1: 15 bytes total */
    return 2U;                           /* Report 2: 31 bytes total */
}

/* ---- Helper: poll for missed EP1 IN completion (self-heal) ---- */
static void HID_PollTxDone(void)
{
    if (g_hidTxBusy == 0U) return;
    if ((_GetENDPOINT(ENDP1) & EP_CTR_TX) != 0U)
    {
        ClearEP_CTR_TX(ENDP1);
        g_hidTxBusy = 0U;
    }
    else if (_GetEPTxStatus(ENDP1) == EP_TX_NAK)
    {
        g_hidTxBusy = 0U;
    }
}

/* =================================================================
 * EP1 OUT Callback (USB ISR context)
 * ================================================================= */
void HID_EP1_OUT_Callback(void)
{
    uint8_t  buf[HID_EP_BUF_SIZE];
    uint16_t i, rx_count;

    rx_count = GetEPRxCount(ENDP1);
    if (rx_count == 0U || rx_count > HID_EP_BUF_SIZE)
    {
        SetEPRxStatus(ENDP1, EP_RX_VALID);
        return;
    }

    PMAToUserBufferCopy(buf, ENDP1_RXADDR, rx_count);

    {
        uint8_t payloadLen = buf[1];
        uint8_t startIdx   = 2U;
        if (payloadLen > (uint8_t)(rx_count - startIdx))
            payloadLen = (uint8_t)(rx_count - startIdx);

        for (i = startIdx; i < (uint16_t)(startIdx + payloadLen) && i < rx_count; i++)
        {
            uint16_t next = (uint16_t)((g_hidRxHead + 1U) & (HID_RX_RING_SIZE - 1U));
            if (next != g_hidRxTail)
            {
                g_hidRxBuf[g_hidRxHead] = buf[i];
                g_hidRxHead = next;
            }
        }
    }

    SetEPRxStatus(ENDP1, EP_RX_VALID);
}

/* =================================================================
 * EP1 IN Callback (USB ISR context)
 * ================================================================= */
void HID_EP1_IN_Callback(void)
{
    g_hidTxBusy = 0U;
}

/* =================================================================
 * HID_Task (main loop context)
 *
 * Drains RX ring, feeds STK parser, calls stkPoll().
 * All STK command execution (including flash writes) happens here.
 * ================================================================= */
void HID_Task(void)
{
    while (g_hidRxHead != g_hidRxTail)
    {
        uint8_t c = g_hidRxBuf[g_hidRxTail];
        g_hidRxTail = (uint16_t)((g_hidRxTail + 1U) & (HID_RX_RING_SIZE - 1U));
        stkSetRxChar(c);
    }
    stkPoll();
}

/* =================================================================
 * HID_GetTxBuffer (internal)
 *
 * Builds a HID input report from STK txBuffer.
 * Report format: [reportId][pendingByteCount][stkData...]
 * ================================================================= */
static uint8_t *HID_GetTxBuffer(uint16_t *pOutLen)
{
    static uint8_t reportBuf[HID_REPORT_MAX_LOAD];
    uint16_t txCount, idx;
    int      c;

    if (pOutLen == NULL) return NULL;

    txCount = (uint16_t)stkGetTxCount();
    if (txCount == 0U || stkGetTxSource() != STK_DATA_SOURCE_USB_HID)
    {
        *pOutLen = 0;
        return NULL;
    }

    for (idx = 0; idx < HID_REPORT_MAX_LOAD; idx++) reportBuf[idx] = 0;

    {
        uint8_t reportId   = HID_ChooseReportId(txCount);
        uint8_t reportSize = (reportId == 1U) ? 15U : 31U;

        reportBuf[0] = reportId;
        reportBuf[1] = (uint8_t)((txCount > 0xFFU) ? 0xFFU : txCount);

        idx = 2U;
        while (idx < (uint16_t)reportSize && (c = stkGetTxByte()) >= 0)
            reportBuf[idx++] = (uint8_t)c;

        *pOutLen = (uint16_t)reportSize;
    }

    return reportBuf;
}

/* =================================================================
 * HID_Tx_Flush (main loop context)
 *
 * Pushes pending STK TX data to host via EP1 IN.
 * Modeled after stkWinUSBFlush(). Call from main loop after HID_Task().
 * ================================================================= */
void HID_Tx_Flush(void)
{
    uint16_t outLen;
    uint8_t *buf;

    if (stkGetTxCount() <= 0) return;
    if (stkGetTxSource() != STK_DATA_SOURCE_USB_HID) return;

    HID_PollTxDone();
    if (g_hidTxBusy != 0U) return;

    buf = HID_GetTxBuffer(&outLen);
    if (buf == NULL || outLen == 0U) return;

    UserToPMABufferCopy(buf, ENDP1_TXADDR, outLen);
    SetEPTxCount(ENDP1, outLen);
    g_hidTxBusy = 1U;
    SetEPTxStatus(ENDP1, EP_TX_VALID);
}


/* OLD API stubs for bootloader usb_prop.c compatibility */
void HID_BeginReportRequest(uint8_t reportId, RequestType_t requestType) { (void)reportId; (void)requestType; }
void HID_Rx_Store(uint8_t reportId, const uint8_t *data, uint8_t len) { (void)reportId; (void)data; (void)len; }
uint8_t *HID_GetReport_Buffer(uint8_t reportId, uint16_t requestedLen, uint16_t *pOutLen) { (void)reportId; (void)requestedLen; if(pOutLen) *pOutLen=0; return NULL; }
void HID_ResetRequestState(void) {}