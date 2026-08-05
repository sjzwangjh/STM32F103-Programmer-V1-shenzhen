/*
 * USB Descriptors - HID + CDC + WinUSB Composite Device
 * HID: EP1 Interrupt IN/OUT (Report IDs 1-2, 15/31 bytes)
 * CDC: EP2 IN (notification) + EP3 IN/OUT (data)
 * WinUSB: EP4 IN/OUT (Bulk)
 */

#include "sys.h"
#include "usb_lib.h"
#include "Hardware_Config.h"
#include "usb_desc.h"

/* ---- Device Descriptor ---- */
const u8 UsbHidDev_DeviceDescriptor[USB_HID_DEV_SIZ_DEVICE_DESC] =
{
    0x12,                       /* bLength */
    USB_DEVICE_DESCRIPTOR_TYPE, /* bDescriptorType */
    (u8)(USB_BCD_USB & 0xFF), (u8)(USB_BCD_USB >> 8),  /* bcdUSB = 2.00 */
    0xEF,                       /* bDeviceClass: Misc */
    0x02,                       /* bDeviceSubClass */
    0x01,                       /* bDeviceProtocol: IAD */
    0x40,                       /* bMaxPacketSize0 = 64 */
    (u8)(USB_VID & 0xFF), (u8)(USB_VID >> 8),    /* idVendor = 0x16C0 */
    (u8)(USB_PID & 0xFF), (u8)(USB_PID >> 8),    /* idProduct = 0x05DF */
    (u8)(USB_BCD_DEVICE & 0xFF), (u8)(USB_BCD_DEVICE >> 8), /* bcdDevice */
    1,                          /* iManufacturer */
    2,                          /* iProduct */
    3,                          /* iSerialNumber */
    0x01                        /* bNumConfigurations */
};

/* ---- Configuration Descriptor ---- */
const u8 UsbHidDev_ConfigDescriptor[USB_HID_DEV_SIZ_CONFIG_DESC] =
{
    /********** Configuration Descriptor (9 bytes) **********/
    0x09,                       /* bLength */
    USB_CONFIGURATION_DESCRIPTOR_TYPE,
    USB_HID_DEV_SIZ_CONFIG_DESC, 0x00,  /* wTotalLength */
    0x04,                       /* bNumInterfaces = 4 (HID+CDC+IAD+WinUSB) */
    0x01,                       /* bConfigurationValue */
    0x00,                       /* iConfiguration */
    0x80,                       /* bmAttributes: Bus-powered */
    0xFA,                       /* bMaxPower: 500mA */

    /********** Interface 0: HID (9 bytes) **********/
    0x09,                       /* bLength */
    USB_INTERFACE_DESCRIPTOR_TYPE,
    0x00,                       /* bInterfaceNumber = 0 */
    0x00,                       /* bAlternateSetting */
    0x02,                       /* bNumEndpoints = 2 (EP1 IN + EP1 OUT) */
    0x03,                       /* bInterfaceClass: HID */
    0x00,                       /* bInterfaceSubClass */
    0x00,                       /* bInterfaceProtocol */
    0x00,                       /* iInterface */

    /********** HID Descriptor (9 bytes) **********/
    0x09,                       /* bLength */
    HID_DESCRIPTOR_TYPE,
    0x01, 0x01,                 /* bcdHID = 1.01 */
    0x00,                       /* bCountryCode */
    0x01,                       /* bNumDescriptors */
    REPORT_DESCRIPTOR,          /* bDescriptorType: Report */
    USB_HID_DEV_SIZ_REPORT_DESC, 0x00,  /* wDescriptorLength */

    /********** EP1 IN: HID Interrupt IN (7 bytes) **********/
    0x07,                       /* bLength */
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    0x81,                       /* bEndpointAddress: EP1 IN */
    0x03,                       /* bmAttributes: Interrupt */
    0x20, 0x00,                 /* wMaxPacketSize: 32 bytes */
    0x01,                       /* bInterval: 1 ms */

    /********** EP1 OUT: HID Interrupt OUT (7 bytes) **********/
    0x07,                       /* bLength */
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    0x01,                       /* bEndpointAddress: EP1 OUT */
    0x03,                       /* bmAttributes: Interrupt */
    0x20, 0x00,                 /* wMaxPacketSize: 32 bytes */
    0x01,                       /* bInterval: 1 ms */

    /********** IAD: CDC (8 bytes) **********/
    0x08,
    USB_INTERFACE_ASSOCIATION_DESCRIPTOR_TYPE,
    0x01,                       /* bFirstInterface = 1 */
    0x02,                       /* bInterfaceCount = 2 */
    0x02,                       /* bFunctionClass: CDC */
    0x02,                       /* bFunctionSubClass: ACM */
    0x01,                       /* bFunctionProtocol: AT */
    0x00,                       /* iFunction */

    /********** Interface 1: CDC Control (9 bytes) **********/
    0x09,
    USB_INTERFACE_DESCRIPTOR_TYPE,
    0x01,                       /* bInterfaceNumber = 1 */
    0x00,
    0x01,                       /* bNumEndpoints = 1 */
    0x02,                       /* bInterfaceClass: CDC */
    0x02,                       /* bInterfaceSubClass */
    0x01,                       /* bInterfaceProtocol */
    0x00,

    /********** CDC Header Functional (5 bytes) **********/
    0x05, 0x24, 0x00, 0x10, 0x01,

    /********** CDC ACM Functional (5 bytes) **********/
    0x05, 0x24, 0x01, 0x00, 0x02,

    /********** CDC Union Functional (5 bytes) **********/
    0x05, 0x24, 0x02, 0x01, 0x02,

    /********** CDC Call Management Functional (5 bytes) **********/
    0x05, 0x24, 0x06, 0x01, 0x02,

    /********** EP2 IN: CDC Notification (7 bytes) **********/
    0x07,
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    0x82,                       /* EP2 IN */
    0x03,                       /* Interrupt */
    0x08, 0x00,                 /* wMaxPacketSize: 8 bytes */
    0xFF,                       /* bInterval */

    /********** Interface 2: CDC Data (9 bytes) **********/
    0x09,
    USB_INTERFACE_DESCRIPTOR_TYPE,
    0x02,                       /* bInterfaceNumber = 2 */
    0x00,
    0x02,                       /* bNumEndpoints = 2 */
    0x0A,                       /* bInterfaceClass: CDC Data */
    0x00, 0x00,
    0x00,

    /********** EP3 OUT: CDC Bulk OUT (7 bytes) **********/
    0x07,
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    0x03,                       /* EP3 OUT */
    0x02,                       /* Bulk */
    0x40, 0x00,                 /* wMaxPacketSize: 64 bytes */
    0x00,

    /********** EP3 IN: CDC Bulk IN (7 bytes) **********/
    0x07,
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    0x83,                       /* EP3 IN */
    0x02,                       /* Bulk */
    0x40, 0x00,                 /* wMaxPacketSize: 64 bytes */
    0x00,

    /********** IAD: WinUSB (8 bytes) **********/
    0x08,
    USB_INTERFACE_ASSOCIATION_DESCRIPTOR_TYPE,
    0x03,                       /* bFirstInterface = 3 */
    0x01,                       /* bInterfaceCount = 1 */
    0xFF,                       /* bFunctionClass: Vendor */
    0x00, 0x00, 0x00,

    /********** Interface 3: WinUSB (9 bytes) **********/
    0x09,
    USB_INTERFACE_DESCRIPTOR_TYPE,
    0x03,                       /* bInterfaceNumber = 3 */
    0x00,
    0x02,                       /* bNumEndpoints = 2 */
    0xFF,                       /* bInterfaceClass: Vendor */
    0x00, 0x00, 0x00,

    /********** EP4 OUT: WinUSB Bulk OUT (7 bytes) **********/
    0x07,
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    0x04,                       /* EP4 OUT */
    0x02,                       /* Bulk */
    0x40, 0x00,                 /* wMaxPacketSize: 64 bytes */
    0x00,

    /********** EP4 IN: WinUSB Bulk IN (7 bytes) **********/
    0x07,
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    0x84,                       /* EP4 IN */
    0x02,                       /* Bulk */
    0x40, 0x00,                 /* wMaxPacketSize: 64 bytes */
    0x00
};

/* ---- HID Report Descriptor ---- */
const u8 UsbHidDev_ReportDescriptor[USB_HID_DEV_SIZ_REPORT_DESC] =
{
    0x06, 0x00, 0xff,       /* Usage Page (Vendor Defined) */
    0x09, 0x01,             /* Usage */
    0xa1, 0x01,             /* Collection (Application) */
    0x15, 0x00,             /* Logical Minimum (0) */
    0x26, 0xff, 0x00,       /* Logical Maximum (255) */
    0x75, 0x08,             /* Report Size (8 bits) */

    /* Input Report 1 (device->host): 15 bytes */
    0x85, 0x01,             /*   Report ID = 1 */
    0x95, 0x0f,             /*   Report Count = 15 */
    0x09, 0x00,             /*   Usage */
    0x81, 0x02,             /*   Input (Data, Variable, Absolute) */

    /* Input Report 2 (device->host): 31 bytes */
    0x85, 0x02,             /*   Report ID = 2 */
    0x95, 0x1f,             /*   Report Count = 31 */
    0x09, 0x00,             /*   Usage */
    0x81, 0x02,             /*   Input (Data, Variable, Absolute) */

    /* Output Report 1 (host->device): 15 bytes */
    0x85, 0x01,             /*   Report ID = 1 */
    0x95, 0x0f,             /*   Report Count = 15 */
    0x09, 0x00,             /*   Usage */
    0x91, 0x02,             /*   Output (Data, Variable, Absolute) */

    /* Output Report 2 (host->device): 31 bytes */
    0x85, 0x02,             /*   Report ID = 2 */
    0x95, 0x1f,             /*   Report Count = 31 */
    0x09, 0x00,             /*   Usage */
    0x91, 0x02,             /*   Output (Data, Variable, Absolute) */

    0xc0                    /* End Collection */
};

/* ---- String Descriptors ---- */
const u8 UsbHidDev_StringLangID[USB_HID_DEV_SIZ_STRING_LANGID] =
{
    USB_HID_DEV_SIZ_STRING_LANGID,
    USB_STRING_DESCRIPTOR_TYPE,
    0x09, 0x04          /* English (US) */
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

/* ---- BOS Descriptor ---- */
const u8 UsbHidDev_BOSDescriptor[USB_HID_DEV_SIZ_BOS_DESC] =
{
    0x05, USB_BOS_DESCRIPTOR_TYPE,
    USB_HID_DEV_SIZ_BOS_DESC, 0x00,
    0x01,                                       /* bNumDeviceCaps = 1 */

    /* Platform Capability Descriptor (28 bytes) */
    0x1C, 0x10, 0x05, 0x00,
    0xDF,0x60,0xDD,0xD8, 0x89,0x45,
    0xC7,0x4C, 0x9C,0xD2,0x65,0x9D,
    0x9E,0x64,0x8A,0x9F,
    0x00,0x00,0x03,0x06,
    0xB2,0x00,
    0x07,
    0x00
};

/* ---- MS OS 2.0 Descriptor ---- */
const u8 UsbHidDev_MSOS20Descriptor[USB_HID_DEV_SIZ_MSOS20_DESC] =
{
    0x0A,0x00, 0x00,0x00, 0x00,0x00,0x03,0x06, 0xB2,0x00,

    0x08,0x00, 0x01,0x00, 0x01,0x00, 0xA8,0x00,

    0x08,0x00, 0x02,0x00, 0x03,0x00, 0xA0,0x00,

    0x14,0x00, 0x03,0x00,
    'W','I','N','U','S','B',0,0,
    0,0,0,0,0,0,0,0,

    0x84,0x00, 0x04,0x00, 0x07,0x00, 0x2A,0x00,
    'D',0,'e',0,'v',0,'i',0,'c',0,'e',0,'I',0,'n',0,'t',0,'e',0,'r',0,'f',0,'a',0,'c',0,'e',0,'G',0,'U',0,'I',0,'D',0,'s',0,0,0,
    0x50,0x00,
    USB_MSOS_GUID_UTF16, 0, 0
};

/* ---- MS OS 1.0 Compatible ID (WinUSB interface 3) ---- */
const u8 UsbHidDev_MSOS10CompatDescriptor[USB_HID_DEV_SIZ_MSOS10_COMPAT_DESC] =
{
    0x28,0x00,0x00,0x00, 0x00,0x01, 0x04,0x00, 0x01,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x03, 0x01,
    'W','I','N','U','S','B',0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0
};

/* ---- MS OS 1.0 Extended Properties ---- */
const u8 UsbHidDev_MSOS10ExtPropsDescriptor[USB_HID_DEV_SIZ_MSOS10_EXT_PROPS_DESC] =
{
    0x8E,0x00,0x00,0x00, 0x00,0x01, 0x05,0x00, 0x01,0x00,
    0x84,0x00,0x00,0x00, 0x01,0x00,0x00,0x00,
    0x28,0x00,
    'D',0,'e',0,'v',0,'i',0,'c',0,'e',0,
    'I',0,'n',0,'t',0,'e',0,'r',0,'f',0,
    'a',0,'c',0,'e',0,'G',0,'U',0,'I',0,
    'D',0,0,0,
    0x4E,0x00,0x00,0x00,
    USB_MSOS_GUID_UTF16
};

/* ---- MS OS String Descriptor ---- */
const u8 UsbHidDev_StringMSOS[18] =
{
    18,
    USB_STRING_DESCRIPTOR_TYPE,
    'M',0,'S',0,'F',0,'T',0,'1',0,'0',0,'0',0,WINUSB_MS_VENDOR_CODE,0
};
