/*
 * USB HID User Header - Dual API (old EP0 Feature Report + new EP1 Interrupt)
 *
 * OLD API (from bootloader): GET_REPORT/SET_REPORT support (dead code, kept for
 *   compatibility with bootloader's usb_prop.c. Windows won't call these since
 *   the HID descriptor now uses Input/Output reports.)
 *
 * NEW API (EP1 Interrupt endpoints): HID_EP1_IN_Callback, HID_EP1_OUT_Callback,
 *   HID_Task, HID_Tx_Flush - actual HID data transport.
 */

#ifndef __USB_HID_USER_H__
#define __USB_HID_USER_H__

#include <stdint.h>
#include <stddef.h>

/* ---- NEW API: EP1 Interrupt endpoint transport ---- */
#define HID_REPORT_MAX_LOAD   32
#define HID_EP_BUF_SIZE       32
#define HID_RX_RING_SIZE      1024U

void HID_EP1_IN_Callback(void);
void HID_EP1_OUT_Callback(void);
void HID_Task(void);
void HID_Tx_Flush(void);

/* ---- OLD API: EP0 Feature Report transport (for bootloader compatibility) ---- */
#define HID_REPORT_BUF_SIZE   128

typedef enum {
    REQUEST_TYPE_IDLE           = 0,
    REQUEST_TYPE_HID_FIRST      = 1,
    REQUEST_TYPE_HID_SUBSEQUENT = 2,
    REQUEST_TYPE_HID_DEBUGDATA  = 3
} RequestType_t;

void HID_BeginReportRequest(uint8_t reportId, RequestType_t requestType);
void HID_Rx_Store(uint8_t reportId, const uint8_t *data, uint8_t len);
uint8_t *HID_GetReport_Buffer(uint8_t reportId, uint16_t requestedLen, uint16_t *pOutLen);
void HID_ResetRequestState(void);

#endif
