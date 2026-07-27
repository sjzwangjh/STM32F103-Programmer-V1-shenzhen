#include "usb_lib.h"
#include "usb_desc.h"

/* USB Standard Device Descriptor
 * VID/PID继续保持AVR-Doper HID兼容; 设备类改为复合设备(IAD)。
 */
const u8 UsbHidDev_DeviceDescriptor[USB_HID_DEV_SIZ_DEVICE_DESC] =
{
    0x12,
    USB_DEVICE_DESCRIPTOR_TYPE,
#if HW_USB_HID_SPEED_FULL
    0x00, 0x02,
#else
    0x10, 0x01,
#endif
    0xEF,                       /* Miscellaneous Device Class */
    0x02,                       /* Common Class */
    0x01,                       /* Interface Association Descriptor */
    0x40,
    0xC0, 0x16,                 /* idVendor = 0x16C0 */
    0xDF, 0x05,                 /* idProduct = 0x05DF */
    0x00, 0x01,
    1,
    2,
    0,
    0x01
};

/* USB Configuration Descriptor: Interface0=HID, Interface1/2=CDC ACM */
const u8 UsbHidDev_ConfigDescriptor[USB_HID_DEV_SIZ_CONFIG_DESC] =
{
    /* Configuration Descriptor */
    0x09,
    USB_CONFIGURATION_DESCRIPTOR_TYPE,
    USB_HID_DEV_SIZ_CONFIG_DESC, 0x00,
    0x03,                       /* bNumInterfaces = HID + CDC Control + CDC Data */
    0x01,
    0x00,
    0x80,                       /* bus powered */
    0xFA,

    /* Interface 0: HID */
    0x09,
    USB_INTERFACE_DESCRIPTOR_TYPE,
    0x00,
    0x00,
    0x01,
    0x03,
    0x00,
    0x00,
    0x00,

    /* HID Descriptor */
    0x09,
    HID_DESCRIPTOR_TYPE,
    0x01, 0x01,
    0x00,
    0x01,
    REPORT_DESCRIPTOR,
    USB_HID_DEV_SIZ_REPORT_DESC, 0x00,

    /* HID Interrupt IN EP1 */
    0x07,
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    0x81,
    0x03,
#if HW_USB_HID_SPEED_FULL
    0x40, 0x00,
    0x01,
#else
    0x08, 0x00,
    0x0A,
#endif

    /* IAD: CDC function, Interface 1 and 2 */
    0x08,
    USB_INTERFACE_ASSOCIATION_DESCRIPTOR_TYPE,
    0x01,
    0x02,
    0x02,
    0x02,
    0x01,
    0x00,

    /* Interface 1: CDC Communication Class */
    0x09,
    USB_INTERFACE_DESCRIPTOR_TYPE,
    0x01,
    0x00,
    0x01,
    0x02,
    0x02,
    0x01,
    0x00,

    /* Header Functional Descriptor */
    0x05,
    0x24,
    0x00,
    0x10, 0x01,

    /* Call Management Functional Descriptor */
    0x05,
    0x24,
    0x01,
    0x00,
    0x02,

    /* Abstract Control Management Functional Descriptor */
    0x04,
    0x24,
    0x02,
    0x02,

    /* Union Functional Descriptor */
    0x05,
    0x24,
    0x06,
    0x01,
    0x02,

    /* CDC Notification IN EP2 */
    0x07,
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    0x82,
    0x03,
    0x08, 0x00,
    0xFF,

    /* Interface 2: CDC Data Class */
    0x09,
    USB_INTERFACE_DESCRIPTOR_TYPE,
    0x02,
    0x00,
    0x02,
    0x0A,
    0x00,
    0x00,
    0x00,

    /* CDC Data OUT EP3 */
    0x07,
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    0x03,
    0x02,
    0x40, 0x00,
    0x00,

    /* CDC Data IN EP3 */
    0x07,
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    0x83,
    0x02,
    0x40, 0x00,
    0x00
};

/* HID Report Descriptor: 保持AVR-Doper Feature Report格式 */
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
    'o', 0, 'b', 0, 'd', 0, 'e', 0, 'v', 0, '.', 0, 'a', 0, 't', 0
};

const u8 UsbHidDev_StringProduct[USB_HID_DEV_SIZ_STRING_PRODUCT] =
{
    USB_HID_DEV_SIZ_STRING_PRODUCT,
    USB_STRING_DESCRIPTOR_TYPE,
    'D',0,'F',0,'M',0,' ',0,'H',0,'I',0,'D',0,'+',0,'C',0,'D',0,'C',0,' ',0,
    'P',0,'r',0,'o',0,'g',0,'r',0,'a',0,'m',0,'m',0,'e',0,'r',0
};
