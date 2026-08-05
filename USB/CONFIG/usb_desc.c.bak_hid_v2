#include "sys.h"
#include "usb_lib.h"
#include "Hardware_Config.h"
#include "usb_desc.h"

/* USB Standard Device Descriptor
 * VID/PID 缁х画 AVR-Doper HID ; 璁剧�?涓鸿锟?IAD)? */
const u8 UsbHidDev_DeviceDescriptor[USB_HID_DEV_SIZ_DEVICE_DESC] =
{
    0x12,
    USB_DEVICE_DESCRIPTOR_TYPE,
    (u8)(USB_BCD_USB & 0xFF), (u8)(USB_BCD_USB >> 8),
    0xEF,
    0x02,
    0x01,
    0x40,
    (u8)(USB_VID & 0xFF), (u8)(USB_VID >> 8),
    (u8)(USB_PID & 0xFF), (u8)(USB_PID >> 8),
    (u8)(USB_BCD_DEVICE & 0xFF), (u8)(USB_BCD_DEVICE >> 8),
    1,
    2,
    3,
    0x01
};

const u8 UsbHidDev_ConfigDescriptor[USB_HID_DEV_SIZ_CONFIG_DESC] =
{
    0x09,
    USB_CONFIGURATION_DESCRIPTOR_TYPE,
    USB_HID_DEV_SIZ_CONFIG_DESC, 0x00,
    0x04,
    0x01,
    0x00,
    0x80,
    0xFA,

    0x09,
    USB_INTERFACE_DESCRIPTOR_TYPE,
    0x00,
    0x00,
    0x01,
    0x03,
    0x00,
    0x00,
    0x00,

    0x09,
    HID_DESCRIPTOR_TYPE,
    0x01, 0x01,
    0x00,
    0x01,
    REPORT_DESCRIPTOR,
    USB_HID_DEV_SIZ_REPORT_DESC, 0x00,

    0x07,
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    0x81,
    0x03,
#if HW_USB_HID_SPEED_FULL
    0x38, 0x00,
    0x01,
#else
    0x08, 0x00,
    0x0A,
#endif

    0x08,
    USB_INTERFACE_ASSOCIATION_DESCRIPTOR_TYPE,
    0x01,
    0x02,
    0x02,
    0x02,
    0x01,
    0x00,

    0x09,
    USB_INTERFACE_DESCRIPTOR_TYPE,
    0x01,
    0x00,
    0x01,
    0x02,
    0x02,
    0x01,
    0x00,

    0x05,
    0x24,
    0x00,
    0x10, 0x01,

    0x05,
    0x24,
    0x01,
    0x00,
    0x02,

    0x05,
    0x24,
    0x02,
    0x01,
    0x02,

    0x05,
    0x24,
    0x06,
    0x01,
    0x02,

    0x07,
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    0x82,
    0x03,
    0x08, 0x00,
    0xFF,

    0x09,
    USB_INTERFACE_DESCRIPTOR_TYPE,
    0x02,
    0x00,
    0x02,
    0x0A,
    0x00,
    0x00,
    0x00,

    0x07,
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    0x03,
    0x02,
    0x40, 0x00,
    0x00,

    0x07,
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    0x83,
    0x02,
    0x40, 0x00,
    0x00,

    0x08,
    USB_INTERFACE_ASSOCIATION_DESCRIPTOR_TYPE,
    0x03,
    0x01,
    0xFF,
    0x00,
    0x00,
    0x00,

    0x09,
    USB_INTERFACE_DESCRIPTOR_TYPE,
    0x03,
    0x00,
    0x02,
    0xFF,
    0x00,
    0x00,
    0x00,

    0x07,
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    0x04,
    0x02,
    0x40, 0x00,
    0x00,

    0x07,
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    0x84,
    0x02,
    0x40, 0x00,
    0x00
};

const u8 UsbHidDev_ReportDescriptor[USB_HID_DEV_SIZ_REPORT_DESC] =
{
    0x06, 0x00, 0xff,
    0x09, 0x01,
    0xa1, 0x01,
    0x15, 0x00,
    0x26, 0xff, 0x00,
    0x75, 0x08,

    0x85, 0x01,
    0x95, 0x0e,
    0x09, 0x00,
    0xb2, 0x02, 0x01,

    0x85, 0x02,
    0x95, 0x1e,
    0x09, 0x00,
    0xb2, 0x02, 0x01,

    0x85, 0x03,
    0x95, 0x3e,
    0x09, 0x00,
    0xb2, 0x02, 0x01,

    0x85, 0x04,
    0x95, 0x7e,
    0x09, 0x00,
    0xb2, 0x02, 0x01,

    0xc0
};

const u8 UsbHidDev_StringLangID[USB_HID_DEV_SIZ_STRING_LANGID] =
{
    USB_HID_DEV_SIZ_STRING_LANGID,
    USB_STRING_DESCRIPTOR_TYPE,
    0x09, 0x04
};

const u8 UsbHidDev_StringVendor[USB_HID_DEV_SIZ_STRING_VENDOR] =
{
    USB_HID_DEV_SIZ_STRING_VENDOR,
    USB_STRING_DESCRIPTOR_TYPE,
    USB_STRING_VENDOR
};

const u8 UsbHidDev_StringProduct[USB_HID_DEV_SIZ_STRING_PRODUCT] =
{
    USB_HID_DEV_SIZ_STRING_PRODUCT,
    USB_STRING_DESCRIPTOR_TYPE,
    USB_STRING_PRODUCT
};

const u8 UsbHidDev_StringSerial[USB_HID_DEV_SIZ_STRING_SERIAL] =
{
    USB_HID_DEV_SIZ_STRING_SERIAL,
    USB_STRING_DESCRIPTOR_TYPE,
    USB_STRING_SERIAL
};

const u8 UsbHidDev_BOSDescriptor[USB_HID_DEV_SIZ_BOS_DESC] =
{
    /* BOS Descriptor Header (5 bytes) */
    0x05, USB_BOS_DESCRIPTOR_TYPE,        /* bLength, bDescriptorType             */
    USB_HID_DEV_SIZ_BOS_DESC, 0x00,       /* wTotalLength = 33                    */
    0x01,                                 /* bNumDeviceCaps = 1                   */

    /* Platform Capability Descriptor (28 bytes)                                  */
    /* MS OS 2.0 Platform Compatibility UUID                                      */
    0x1C, 0x10, 0x05, 0x00,              /* bLength=28, type=0x10, cap=0x05, res=0 */
    0xDF,0x60,0xDD,0xD8, 0x89,0x45,      /* Platform UUID: D8DD60DF-4589-4CC7-    */
    0xC7,0x4C, 0x9C,0xD2,0x65,0x9D,      /* 9CD2-659D-9E64-8A9F                   */
    0x9E,0x64,0x8A,0x9F,
    0x00,0x00,0x03,0x06,                 /* dwWindowsVersion = 0x06030000 */
    0xB2,0x00,                            /* wMSOSDescriptorSetTotalLength = 178 */
    0x07,                                 /* bMS_VendorCode */
    0x00                                  /* bAltEnumCode */
};

const u8 UsbHidDev_MSOS20Descriptor[USB_HID_DEV_SIZ_MSOS20_DESC] =
{
    /* MS OS 2.0 Descriptor Set Header (10 bytes) */
    0x0A,0x00,
    0x00,0x00,
    0x00,0x00,0x03,0x06,
    0xB2,0x00,                    /* Total length = 178 bytes */

    /* Configuration Subset Header (8 bytes) */
    0x08,0x00,
    0x01,0x00,
    0x01,0x00,
    0xA8,0x00,

    /* Function Subset Header Interface 3 (8 bytes) */
    0x08,0x00,
    0x02,0x00,
    0x03,0x00,
    0xA0,0x00,

    /* Compatible ID Descriptor (20 bytes) */
    0x14,0x00,
    0x03,0x00,
    'W','I','N','U','S','B',0,0,
    0,0,0,0,0,0,0,0,

    /* Registry Property Descriptor - DeviceInterfaceGUID */
    0x84,0x00,
    0x04,0x00,
    0x07,0x00,
    0x2A,0x00,
    'D',0,'e',0,'v',0,'i',0,'c',0,'e',0,'I',0,'n',0,'t',0,'e',0,'r',0,'f',0,'a',0,'c',0,'e',0,'G',0,'U',0,'I',0,'D',0,'s',0,0,0,
    0x50,0x00,
    USB_MSOS_GUID_UTF16, 0, 0
};

/* MS OS 1.0 Compatible ID Descriptor (WinUSB for interface 3) */
const u8 UsbHidDev_MSOS10CompatDescriptor[USB_HID_DEV_SIZ_MSOS10_COMPAT_DESC] =
{
    0x28,0x00,0x00,0x00,  /* dwLength = 40 */
    0x00,0x01,            /* bcdVersion = 0x0100 */
    0x04,0x00,            /* wIndex = 0x0004 */
    0x01,                 /* bCount = 1 */
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* Reserved[7] */
    0x03,                 /* bFirstInterfaceNumber = 3 (WinUSB interface) */
    0x01,                 /* Reserved1 (must be 0x01) */
    'W','I','N','U','S','B',0,0,  /* CompatibleID = "WINUSB" */
    0,0,0,0,0,0,0,0,     /* SubCompatibleID = empty */
    0,0,0,0,0,0           /* Reserved2[6] */
};

/* MS OS 1.0 Extended Properties Descriptor (WinUSB DeviceInterfaceGUID for interface 3)
 * Ported from TeenyUSB sample/composite COMP_IF3_WCIDProperties (MS OS 1.0 / WCID).
 */
const u8 UsbHidDev_MSOS10ExtPropsDescriptor[USB_HID_DEV_SIZ_MSOS10_EXT_PROPS_DESC] =
{
    0x8E,0x00,0x00,0x00,          /* dwLength = 142 */
    0x00,0x01,                    /* bcdVersion = 0x0100 */
    0x05,0x00,                    /* wIndex = 0x0005 */
    0x01,0x00,                    /* wCount = 1 */

    0x84,0x00,0x00,0x00,          /* dwSize = 132 */
    0x01,0x00,0x00,0x00,          /* dwPropertyDataType = REG_SZ (0x0001) */
    0x28,0x00,                    /* wPropertyNameLength = 40 */
    'D',0,'e',0,'v',0,'i',0,'c',0,'e',0,
    'I',0,'n',0,'t',0,'e',0,'r',0,'f',0,
    'a',0,'c',0,'e',0,'G',0,'U',0,'I',0,
    'D',0,0,0,                    /* "DeviceInterfaceGUID" */
    0x4E,0x00,0x00,0x00,          /* dwPropertyDataLength = 78 */
    USB_MSOS_GUID_UTF16
};

/* MS OS 1.0 String Descriptor (index 0xEE) */
const u8 UsbHidDev_StringMSOS[18] =
{
    18,
    USB_STRING_DESCRIPTOR_TYPE,
    'M',0,'S',0,'F',0,'T',0,'1',0,'0',0,'0',0,WINUSB_MS_VENDOR_CODE,0
};


