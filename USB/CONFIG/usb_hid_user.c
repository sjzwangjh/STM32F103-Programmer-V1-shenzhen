/*
 * USB HID User Implementation - Interrupt Endpoint Transport
 *
 * HID data flows through EP1 interrupt IN/OUT endpoints, NOT EP0 feature
 * reports. The USB ISR only copies data between PMA and the ring buffer;
 * all STK command processing happens in the main loop via HID_Task().
 *
 * Report format (preserved from the previous feature-report transport):
 *   byte 0 = Report ID (1-2)
 *   byte 1 = payload length in THIS report
 *   byte 2.. = STK message bytes
 */

#include "sys.h"
#include "usb_hid_user.h"
#include "usb_lib.h"
#include "usb_conf.h"
#include "Stk500Protocol.h"
#include "Hardware_Config.h"
#include "usart.h"

/* ---- EP1 OUT: ring buffer for bytes from host ---- */
static uint8_t  g_hidRxBuf[HID_RX_RING_SIZE];
static uint16_t g_hidRxHead;
static uint16_t g_hidRxTail;
static volatile uint8_t  g_hidRxStalled;   /* EP1 OUT held in NAK: ring was full */
static volatile uint16_t g_hidRxDrop;      /* overflow counter (debug) */

/* ---- EP1 IN: TX state ---- */
static uint8_t  g_hidTxBusy;

#if DEBUG_HARDWARE_CONFIG
static void HidDebugWriteDec(uint16_t value)
{
    char buf[6];
    uint8_t i = 0U;
    if (value == 0U)
    {
        uart1_WriteByte('0');
        return;
    }
    while (value > 0U && i < sizeof(buf))
    {
        buf[i++] = (char)('0' + (value % 10U));
        value /= 10U;
    }
    while (i > 0U)
    {
        uart1_WriteByte((uint8_t)buf[--i]);
    }
}
#endif

/* ---- Helper: choose report ID based on payload size ---- */
static uint8_t HID_ChooseReportId(uint16_t payloadLen)
{
    if (payloadLen <= 29U) return 1U;   /* Report 1: 31 bytes total */
    return 2U;                           /* Report 2: 63 bytes total */
}

/* ---- Helper: free space in RX ring (one slot reserved) ---- */
static uint16_t HID_RingSpace(void)
{
    uint16_t used = (uint16_t)((g_hidRxHead - g_hidRxTail) & (HID_RX_RING_SIZE - 1U));
    return (uint16_t)(HID_RX_RING_SIZE - 1U - used);
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
    uint16_t i, rx_count, payloadLen;

    rx_count = GetEPRxCount(ENDP1);
    if (rx_count == 0U || rx_count > HID_EP_BUF_SIZE)
    {
        SetEPRxStatus(ENDP1, EP_RX_VALID);
        return;
    }

    PMAToUserBufferCopy(buf, ENDP1_RXADDR, rx_count);

    /* Report format: [Report ID][payloadLen][STK bytes...] */
    if (buf[0] != 1U && buf[0] != 2U)
    {
        SetEPRxStatus(ENDP1, EP_RX_VALID);
        return;
    }

    payloadLen = buf[1];
    if (payloadLen > (uint16_t)(rx_count - 2U))
        payloadLen = (uint16_t)(rx_count - 2U);
    if (buf[0] == 1U && payloadLen > 29U)
        payloadLen = 29U;
    else if (buf[0] == 2U && payloadLen > 61U)
        payloadLen = 61U;
    if (payloadLen == 0U)
    {
        SetEPRxStatus(ENDP1, EP_RX_VALID);
        return;
    }

    if (payloadLen > HID_RingSpace())
    {
        /* Ring full: keep EP1 OUT in NAK so the host retries later. */
        g_hidRxDrop++;
        g_hidRxStalled = 1U;
        return;
    }

    for (i = 0U; i < payloadLen; i++)
    {
        uint16_t next = (uint16_t)((g_hidRxHead + 1U) & (HID_RX_RING_SIZE - 1U));
        g_hidRxBuf[g_hidRxHead] = buf[2U + i];
        g_hidRxHead = next;
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
 * Single main-loop entry for the HID interface: drains the EP1 OUT ring
 * and feeds the STK parser, re-arms EP1 OUT after a ring-full stall, then
 * flushes pending STK responses via EP1 IN.
 * ================================================================= */
static void HID_TxFlush(void);

void HID_Task(void)
{
    while (g_hidRxHead != g_hidRxTail)
    {
        uint8_t c = g_hidRxBuf[g_hidRxTail];
        g_hidRxTail = (uint16_t)((g_hidRxTail + 1U) & (HID_RX_RING_SIZE - 1U));
        stkSetRxCharEx(STK_DATA_SOURCE_USB_HID, c);
    }

    if (g_hidRxStalled != 0U)
    {
        g_hidRxStalled = 0U;
        SetEPRxStatus(ENDP1, EP_RX_VALID);
    }

    /* Process any complete STK frame, then push pending response bytes */
    stkPoll();
    HID_TxFlush();

#if DEBUG_HARDWARE_CONFIG
    if (g_hidRxDrop != 0U)
    {
        uart1_WriteString("HID RX drop=");
        HidDebugWriteDec(g_hidRxDrop);
        uart1_WriteString("\r\n");
        g_hidRxDrop = 0U;
    }
#endif
}

/* =================================================================
 * HID_GetTxBuffer (internal)
 *
 * Builds a HID input report from the STK TX buffer.
 * Report format: [reportId][payloadLen][stkData...]
 * ================================================================= */
static uint8_t *HID_GetTxBuffer(uint16_t *pOutLen)
{
    static uint8_t reportBuf[HID_REPORT_MAX_LOAD];
    uint16_t txCount, idx;
    uint8_t  reportId, reportSize;
    int      c;

    if (pOutLen == NULL) return NULL;

    txCount = (uint16_t)stkGetTxCount();
    if (txCount == 0U)
    {
        *pOutLen = 0U;
        return NULL;
    }
    if (stkGetTxSource() != STK_DATA_SOURCE_USB_HID)
    {
        *pOutLen = 0U;
        return NULL;
    }

    for (idx = 0U; idx < HID_REPORT_MAX_LOAD; idx++) reportBuf[idx] = 0;

    reportId   = HID_ChooseReportId(txCount);
    reportSize = (reportId == 1U) ? 31U : 63U;

    reportBuf[0] = reportId;
    reportBuf[1] = 0U;

    idx = 2U;
    while (idx < (uint16_t)reportSize && (c = stkGetTxByte()) >= 0)
        reportBuf[idx++] = (uint8_t)c;

    reportBuf[1] = (uint8_t)(idx - 2U);
    *pOutLen = (uint16_t)reportSize;

    return reportBuf;
}

/* =================================================================
 * HID_TxFlush (main loop context)
 *
 * Pushes pending STK TX data to the host via EP1 IN. One report per
 * call; long replies are split over successive main-loop iterations.
 * ================================================================= */
static void HID_TxFlush(void)
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
