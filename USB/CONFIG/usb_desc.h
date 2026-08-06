/*
 * USB????? - HID + CDC + WinUSB ???
 */

#ifndef __USB_DESC_H
#define __USB_DESC_H

#include "usb_type.h"

/* ============================================================================
 * USB 设备身份配置区 —— 后续修改 VID/PID/字符串等参数只改这里
 * ==========================================================================*/
#define USB_VID                        0x16C0
#define USB_PID                        0x05DF
#define USB_BCD_USB                    0x0200
#define USB_BCD_DEVICE                 0x0200

/* 字符串以 UTF-16LE 字符对展开；CHAR 计数用于自动计算描述符长度。 */
#define USB_STRING_VENDOR_CHARS        8
#define USB_STRING_VENDOR              'o',0,'b',0,'d',0,'e',0,'v',0,'.',0,'a',0,'t',0

#define USB_STRING_PRODUCT_CHARS       14
#define USB_STRING_PRODUCT             'D',0,'F',0,'M',0,' ',0,'P',0,'r',0,'o',0,'g',0,'r',0,'a',0,'m',0,'m',0,'e',0,'r',0

#define USB_STRING_SERIAL_CHARS        19
#define USB_STRING_SERIAL              'd',0,'f',0,'m',0,'I',0,'n',0,'L',0,'i',0,'n',0,'e',0,'P',0,'r',0,'o',0,'g',0,'r',0,'a',0,'m',0,'m',0,'e',0,'r',0

/* WinUSB / MS OS 描述符相关参数 */
#define USB_MS_VENDOR_CODE             0x07
#define USB_MSOS_GUID_UTF16 \
    '{',0,'9',0,'B',0,'0',0,'D',0,'1',0,'C',0,'A',0,'8',0, \
    '-',0,'2',0,'D',0,'6',0,'8',0,'-',0, \
    '4',0,'B',0,'A',0,'2',0,'-',0, \
    '9',0,'E',0,'7',0,'2',0,'-',0, \
    '4',0,'0',0,'1',0,'0',0,'5',0,'5',0,'5',0,'1',0,'0',0,'0',0,'0',0,'1',0, \
    '}',0,0,0
/* ==========================================================================*/

#define USB_DEVICE_DESCRIPTOR_TYPE                 0x01
#define USB_CONFIGURATION_DESCRIPTOR_TYPE          0x02
#define USB_STRING_DESCRIPTOR_TYPE                 0x03
#define USB_INTERFACE_DESCRIPTOR_TYPE              0x04
#define USB_ENDPOINT_DESCRIPTOR_TYPE               0x05
#define USB_INTERFACE_ASSOCIATION_DESCRIPTOR_TYPE  0x0B
#define USB_BOS_DESCRIPTOR_TYPE                    0x0F
#define USB_DEVICE_CAPABILITY_DESCRIPTOR_TYPE      0x10

#define HID_DESCRIPTOR_TYPE                        0x21
#define REPORT_DESCRIPTOR                          0x22

#define USB_HID_DEV_SIZ_DEVICE_DESC               18
#define USB_HID_DEV_SIZ_CONFIG_DESC               139
#define USB_HID_DEV_SIZ_REPORT_DESC               47
#define USB_HID_DEV_SIZ_HID_DESC                  9
#define USB_HID_DEV_SIZ_BOS_DESC                  33
#define USB_HID_DEV_SIZ_MSOS20_DESC               178
#define USB_HID_DEV_SIZ_MSOS10_COMPAT_DESC        40
#define USB_HID_DEV_SIZ_MSOS10_EXT_PROPS_DESC     142
#define USB_HID_DEV_OFF_HID_DESC                  18

#define USB_HID_DEV_SIZ_STRING_LANGID             4
#define USB_HID_DEV_SIZ_STRING_VENDOR             (2 + 2 * USB_STRING_VENDOR_CHARS)
#define USB_HID_DEV_SIZ_STRING_PRODUCT            (2 + 2 * USB_STRING_PRODUCT_CHARS)
#define USB_HID_DEV_SIZ_STRING_SERIAL             (2 + 2 * USB_STRING_SERIAL_CHARS)

#define STANDARD_ENDPOINT_DESC_SIZE               0x09
#define WINUSB_MS_VENDOR_CODE                     USB_MS_VENDOR_CODE
#define WINUSB_REQUEST_GET_DESCRIPTOR_SET         0x07

extern const u8 UsbHidDev_DeviceDescriptor[USB_HID_DEV_SIZ_DEVICE_DESC];
extern const u8 UsbHidDev_ConfigDescriptor[USB_HID_DEV_SIZ_CONFIG_DESC];
extern const u8 UsbHidDev_ReportDescriptor[USB_HID_DEV_SIZ_REPORT_DESC];
extern const u8 UsbHidDev_StringLangID[USB_HID_DEV_SIZ_STRING_LANGID];
extern const u8 UsbHidDev_StringVendor[USB_HID_DEV_SIZ_STRING_VENDOR];
extern const u8 UsbHidDev_StringProduct[USB_HID_DEV_SIZ_STRING_PRODUCT];
extern const u8 UsbHidDev_StringSerial[USB_HID_DEV_SIZ_STRING_SERIAL];
extern const u8 UsbHidDev_StringMSOS[18];
extern const u8 UsbHidDev_BOSDescriptor[USB_HID_DEV_SIZ_BOS_DESC];
extern const u8 UsbHidDev_MSOS20Descriptor[USB_HID_DEV_SIZ_MSOS20_DESC];
extern const u8 UsbHidDev_MSOS10CompatDescriptor[USB_HID_DEV_SIZ_MSOS10_COMPAT_DESC];
extern const u8 UsbHidDev_MSOS10ExtPropsDescriptor[USB_HID_DEV_SIZ_MSOS10_EXT_PROPS_DESC];

#endif
