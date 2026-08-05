/*
 * USB configuration - HID + CDC + WinUSB Composite
 * PMA layout optimized for 6 endpoints in 512 bytes:
 *   BTABLE:   0x00-0x2F  (48B)
 *   EP0 RX:   0x30-0x6F  (64B)  EP0 TX:   0x70-0xAF  (64B)
 *   EP1 TX:   0xB0-0xCF  (32B)  EP1 RX:   0xD0-0xEF  (32B)
 *   EP2 TX:   0xF0-0xF7  (8B)
 *   EP3 TX:   0xF8-0x137 (64B)  EP3 RX:   0x138-0x177 (64B)
 *   EP4 TX:   0x178-0x1B7 (64B)  EP4 RX:   0x1B8-0x1F7 (64B)
 * Total: 504 bytes (fits in 512-byte PMA)
 */

#ifndef __USB_CONF_H
#define __USB_CONF_H

/* ---- Endpoint count (EP0-EP4) ---- */
#define EP_NUM     (5)

/* ---- Buffer Description Table ---- */
#define BTABLE_ADDRESS      (0x00)

/* EP0: Control */
#define ENDP0_RXADDR        (0x30)
#define ENDP0_TXADDR        (0x70)

/* EP1: HID Interrupt IN/OUT (32 bytes each) */
#define ENDP1_TXADDR        (0xB0)
#define ENDP1_RXADDR        (0xD0)

/* EP2: CDC Notification IN (8 bytes) */
#define ENDP2_TXADDR        (0xF0)

/* EP3: CDC Data IN/OUT */
#define ENDP3_TXADDR        (0xF8)
#define ENDP3_RXADDR        (0x138)

/* EP4: WinUSB Bulk IN/OUT */
#define ENDP4_TXADDR        (0x178)
#define ENDP4_RXADDR        (0x1B8)

/* ---- IMR mask ---- */
#define IMR_MSK (CNTR_CTRM  | CNTR_WKUPM | CNTR_SUSPM | CNTR_ERRM  | CNTR_SOFM \
                 | CNTR_ESOFM | CNTR_RESETM )

/* ---- CTR callbacks ---- */
/* HID EP1: real interrupt endpoint handlers (not NOP) */
#define  EP1_IN_Callback   HID_EP1_IN_Callback
#define  EP1_OUT_Callback  HID_EP1_OUT_Callback

/* CDC */
#define  EP2_IN_Callback   NOP_Process
#define  EP3_IN_Callback   CDC_DataIn_Callback
#define  EP3_OUT_Callback  CDC_DataOut_Callback

/* WinUSB */
#define  EP4_IN_Callback   WinUSB_IN_Callback
#define  EP4_OUT_Callback  WinUSB_OUT_Callback

/* Unused */
#define  EP5_IN_Callback   NOP_Process
#define  EP6_IN_Callback   NOP_Process
#define  EP7_IN_Callback   NOP_Process
#define  EP2_OUT_Callback  NOP_Process
#define  EP5_OUT_Callback  NOP_Process
#define  EP6_OUT_Callback  NOP_Process
#define  EP7_OUT_Callback  NOP_Process

#endif /*__USB_CONF_H__*/
