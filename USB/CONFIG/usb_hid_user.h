/*
 * USB HID User Header - Interrupt Endpoint Transport (CMSIS-DAP Model)
 *
 * HID data flows through EP1 interrupt IN/OUT endpoints, NOT EP0 feature reports.
 * USB ISR only copies data between PMA and ring buffers; all STK command
 * processing happens in the main loop via HID_Task().
 */

#ifndef __USB_HID_USER_H__
#define __USB_HID_USER_H__

#include <stdint.h>
#include <stddef.h>

/* Max report buffer size (matches 32-byte EP1 wMaxPacketSize) */
#define HID_REPORT_MAX_LOAD   32
#define HID_EP_BUF_SIZE       32

/* EP1 OUT ring buffer: stores bytes from host for main-loop processing */
#define HID_RX_RING_SIZE      1024U

/* ---- Called from USB ISR (EP1 CTR callbacks) ---- */
void HID_EP1_IN_Callback(void);
void HID_EP1_OUT_Callback(void);

/* ---- Called from main loop ---- */

/* Drain RX ring buffer, feed STK parser, call stkPoll() */
void HID_Task(void);

/* Push STK txBuffer data to host via EP1 IN */
void HID_Tx_Flush(void);

#endif
