/*
 * USB WinUSB Bulk user-layer header
 */

#ifndef __USB_WINUSB_USER_H
#define __USB_WINUSB_USER_H

#include "sys.h"

/* Bulk transfer API */
u8   WinUSB_Bulk_Send(const u8 *buf, u16 len);
u16  WinUSB_Bulk_Recv(u8 *buf, u16 maxLen);
u16  WinUSB_Bulk_Available(void);
u8   WinUSB_Bulk_TxBusy(void);

/* EP4 callbacks (bound via usb_conf.h macros) */
void WinUSB_IN_Callback(void);
void WinUSB_OUT_Callback(void);

#endif
