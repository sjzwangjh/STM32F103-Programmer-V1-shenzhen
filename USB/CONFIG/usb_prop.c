/*
 * USB Property Handler - HID + CDC + WinUSB Composite
 *
 * HID transport migrated from EP0 Feature Reports to EP1 Interrupt IN/OUT.
 * SET_REPORT/GET_REPORT handlers removed; data now flows through EP1.
 * Standard USB requests (GET_DESCRIPTOR etc.), MS OS, and CDC requests unchanged.
 */

#include "hw_USB_config.h"
#include "usb_lib.h"
#include "usb_conf.h"
#include "usb_prop.h"
#include "usb_desc.h"
#include "usb_pwr.h"
#include "usb_hid_user.h"
#include "usb_cdc_user.h"
#include "Hardware_Config.h"
#include "usart.h"

u32 ProtocolValue;

/* ---- Global USB structures required by STM32 USB library ---- */
DEVICE Device_Table = {
    EP_NUM,
    1
};

DEVICE_PROP Device_Property = {
    UsbHidDev_init,
    UsbHidDev_Reset,
    UsbHidDev_Status_In,
    UsbHidDev_Status_Out,
    UsbHidDev_Data_Setup,
    UsbHidDev_NoData_Setup,
    UsbHidDev_Get_Interface_Setting,
    UsbHidDev_GetDeviceDescriptor,
    UsbHidDev_GetConfigDescriptor,
    UsbHidDev_GetStringDescriptor,
    UsbHidDev_GetBOSDescriptor,
    0,
    0x40
};

USER_STANDARD_REQUESTS User_Standard_Requests = {
    UsbHidDev_GetConfiguration,
    UsbHidDev_SetConfiguration,
    UsbHidDev_GetInterface,
    UsbHidDev_SetInterface,
    UsbHidDev_GetStatus,
    UsbHidDev_ClearFeature,
    UsbHidDev_SetEndPointFeature,
    UsbHidDev_SetDeviceFeature,
    UsbHidDev_SetDeviceAddress
};

/* ---- Custom descriptor helper for MS OS / BOS ---- */
static ONE_DESCRIPTOR g_customDesc;
static u8 *GetCustomDescriptor(u16 Length)
{
    return Standard_GetDescriptorData(Length, &g_customDesc);
}

/* ---- Standard descriptor reference tables ---- */
ONE_DESCRIPTOR Device_Descriptor = {
    (u8 *)UsbHidDev_DeviceDescriptor,
    USB_HID_DEV_SIZ_DEVICE_DESC
};

ONE_DESCRIPTOR Config_Descriptor = {
    (u8 *)UsbHidDev_ConfigDescriptor,
    USB_HID_DEV_SIZ_CONFIG_DESC
};

ONE_DESCRIPTOR UsbHidDev_Report_Descriptor = {
    (u8 *)UsbHidDev_ReportDescriptor,
    USB_HID_DEV_SIZ_REPORT_DESC
};

ONE_DESCRIPTOR Mouse_Hid_Descriptor = {
    (u8 *)UsbHidDev_ConfigDescriptor + USB_HID_DEV_OFF_HID_DESC,
    USB_HID_DEV_SIZ_HID_DESC
};

ONE_DESCRIPTOR String_Descriptor[4] = {
    {(u8 *)UsbHidDev_StringLangID,  USB_HID_DEV_SIZ_STRING_LANGID},
    {(u8 *)UsbHidDev_StringVendor,  USB_HID_DEV_SIZ_STRING_VENDOR},
    {(u8 *)UsbHidDev_StringProduct, USB_HID_DEV_SIZ_STRING_PRODUCT},
    {0, 0}                          /* serial filled by Get_SerialNum() */
};

/* ---- CDC pending state ---- */
static u8 g_cdcPendingSetLineCoding;

/* ---- CDC helper routines ---- */
static u8 *CDC_GetLineCodingData(u16 Length)
{
    if (Length == 0) {
        CDC_FillLineCodingBuffer();
        pInformation->Ctrl_Info.Usb_wLength = 7;
        return NULL;
    }
    return CDC_GetLineCodingBuffer() + pInformation->Ctrl_Info.Usb_wOffset;
}

static u8 *CDC_SetLineCodingData(u16 Length)
{
    if (Length == 0) {
        pInformation->Ctrl_Info.Usb_rLength = 7;
        return NULL;
    }
    return CDC_GetLineCodingBuffer() + pInformation->Ctrl_Info.Usb_rOffset;
}

/* =================================================================
 * Status handlers (called at end of control transfer status phase)
 * ================================================================= */
void UsbHidDev_Status_In(void)
{
    if (g_cdcPendingSetLineCoding != 0U) {
        CDC_SetLineCodingFromBuffer();
        g_cdcPendingSetLineCoding = 0U;
    }
}

void UsbHidDev_Status_Out(void)
{
}

void UsbHidDev_SetConfiguration(void)
{
    DEVICE_INFO *pInfo = &Device_Info;
    if (pInfo->Current_Configuration != 0)
        bDeviceState = CONFIGURED;
}

void UsbHidDev_SetDeviceAddress(void)
{
    bDeviceState = ADDRESSED;
}

/* =================================================================
 * BOS Descriptor getter (for WinUSB MS OS 2.0)
 * ================================================================= */
u8 *UsbHidDev_GetBOSDescriptor(u16 Length)
{
    g_customDesc.Descriptor     = (u8 *)UsbHidDev_BOSDescriptor;
    g_customDesc.Descriptor_Size = sizeof(UsbHidDev_BOSDescriptor);
    return GetCustomDescriptor(Length);
}

/* =================================================================
 * UsbHidDev_Data_Setup
 *
 * Handles EP0 control transfer SETUP stage.
 * HID class: GET_PROTOCOL, SET_PROTOCOL only (GET_REPORT/SET_REPORT removed).
 * CDC class: GET_LINE_CODING, SET_LINE_CODING.
 * MS OS / BOS: uses GetCustomDescriptor helper.
 * ================================================================= */
RESULT UsbHidDev_Data_Setup(u8 RequestNo)
{
    u8 *(*CopyRoutine)(u16) = NULL;

    /* ---- GET_DESCRIPTOR (Standard, Interface recipient) ---- */
    if ((RequestNo == GET_DESCRIPTOR)
        && (Type_Recipient == (STANDARD_REQUEST | INTERFACE_RECIPIENT))
        && (pInformation->USBwIndex0 == 0U))
    {
        if (pInformation->USBwValue1 == REPORT_DESCRIPTOR)
            CopyRoutine = UsbHidDev_GetReportDescriptor;
        else if (pInformation->USBwValue1 == HID_DESCRIPTOR_TYPE)
            CopyRoutine = UsbHidDev_GetHIDDescriptor;
    }

    /* ---- MS OS 2.0 Descriptor (vendor request) ---- */
    if ((pInformation->USBbmRequestType == 0xC0U)
        && (RequestNo == WINUSB_MS_VENDOR_CODE)
        && ((pInformation->USBwIndex == WINUSB_REQUEST_GET_DESCRIPTOR_SET)
            || (pInformation->USBwIndex == 0x0007U)))
    {
        g_customDesc.Descriptor     = (u8 *)UsbHidDev_MSOS20Descriptor;
        g_customDesc.Descriptor_Size = sizeof(UsbHidDev_MSOS20Descriptor);
        pInformation->Ctrl_Info.CopyData = GetCustomDescriptor;
        pInformation->Ctrl_Info.Usb_wOffset = 0;
        GetCustomDescriptor(0);
        return USB_SUCCESS;
    }

    /* ---- MS OS 1.0 Compatible ID ---- */
    if ((pInformation->USBbmRequestType == 0xC0U)
        && (RequestNo == WINUSB_MS_VENDOR_CODE)
        && (pInformation->USBwIndex == 0x0004U))
    {
        g_customDesc.Descriptor     = (u8 *)UsbHidDev_MSOS10CompatDescriptor;
        g_customDesc.Descriptor_Size = sizeof(UsbHidDev_MSOS10CompatDescriptor);
        pInformation->Ctrl_Info.CopyData = GetCustomDescriptor;
        pInformation->Ctrl_Info.Usb_wOffset = 0;
        GetCustomDescriptor(0);
        return USB_SUCCESS;
    }

    /* ---- MS OS 1.0 Extended Properties ---- */
    if ((pInformation->USBbmRequestType == 0xC0U)
        && (RequestNo == WINUSB_MS_VENDOR_CODE)
        && (pInformation->USBwIndex == 0x0005U))
    {
        if (pInformation->USBwValue0 != 3U)
            return USB_UNSUPPORT;
        g_customDesc.Descriptor     = (u8 *)UsbHidDev_MSOS10ExtPropsDescriptor;
        g_customDesc.Descriptor_Size = sizeof(UsbHidDev_MSOS10ExtPropsDescriptor);
        pInformation->Ctrl_Info.CopyData = GetCustomDescriptor;
        pInformation->Ctrl_Info.Usb_wOffset = 0;
        GetCustomDescriptor(0);
        return USB_SUCCESS;
    }

    /*** GET_PROTOCOL ***/
    if ((Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT))
        && RequestNo == GET_PROTOCOL)
    {
        CopyRoutine = UsbHidDev_GetProtocolValue;
    }

    /*** CDC GET_LINE_CODING ***/
    if ((Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT))
        && RequestNo == CDC_GET_LINE_CODING
        && pInformation->USBwIndex0 == 1U)
    {
        CopyRoutine = CDC_GetLineCodingData;
    }

    /*** CDC SET_LINE_CODING ***/
    if ((Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT))
        && RequestNo == CDC_SET_LINE_CODING
        && pInformation->USBwIndex0 == 1U)
    {
        if (pInformation->USBwLength != 7U)
            return USB_UNSUPPORT;
        g_cdcPendingSetLineCoding = 1U;
        pInformation->Ctrl_Info.Usb_rOffset = 0;
        pInformation->Ctrl_Info.Usb_rLength = 7;
        pInformation->Ctrl_Info.CopyData = CDC_SetLineCodingData;
        return USB_SUCCESS;
    }

    if (CopyRoutine == NULL)
        return USB_UNSUPPORT;

    pInformation->Ctrl_Info.CopyData = CopyRoutine;
    pInformation->Ctrl_Info.Usb_wOffset = 0;
    (*CopyRoutine)(0);
    return USB_SUCCESS;
}

/* =================================================================
 * UsbHidDev_NoData_Setup
 * ================================================================= */
RESULT UsbHidDev_NoData_Setup(u8 RequestNo)
{
    if ((Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT))
        && (RequestNo == SET_PROTOCOL)
        && (pInformation->USBwIndex0 == 0U))
    {
        return UsbHidDev_SetProtocol();
    }
    else if ((Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT))
             && (RequestNo == CDC_SET_CONTROL_LINE_STATE)
             && (pInformation->USBwIndex0 == 1U))
    {
        uint16_t state;
        state = (uint16_t)pInformation->USBwValue0 |
                ((uint16_t)pInformation->USBwValue1 << 8);
        CDC_SetControlLineState(state);
        return USB_SUCCESS;
    }
    else
    {
        return USB_UNSUPPORT;
    }
}

/* =================================================================
 * Standard descriptor getters
 * ================================================================= */
u8 *UsbHidDev_GetDeviceDescriptor(u16 Length)
{
    return Standard_GetDescriptorData(Length, &Device_Descriptor);
}

u8 *UsbHidDev_GetConfigDescriptor(u16 Length)
{
    return Standard_GetDescriptorData(Length, &Config_Descriptor);
}

u8 *UsbHidDev_GetStringDescriptor(u16 Length)
{
    u8 wValue0 = pInformation->USBwValue0;

    if (wValue0 == 0xEE) {
        g_customDesc.Descriptor = (u8 *)UsbHidDev_StringMSOS;
        g_customDesc.Descriptor_Size = sizeof(UsbHidDev_StringMSOS);
        return Standard_GetDescriptorData(Length, &g_customDesc);
    }
    if (wValue0 >= 4)
        return NULL;
    else
        return Standard_GetDescriptorData(Length, &String_Descriptor[wValue0]);
}

u8 *UsbHidDev_GetReportDescriptor(u16 Length)
{
    return Standard_GetDescriptorData(Length, &UsbHidDev_Report_Descriptor);
}

u8 *UsbHidDev_GetHIDDescriptor(u16 Length)
{
    return Standard_GetDescriptorData(Length, &Mouse_Hid_Descriptor);
}

RESULT UsbHidDev_Get_Interface_Setting(u8 Interface, u8 AlternateSetting)
{
    if (AlternateSetting > 0)
        return USB_UNSUPPORT;
    else if (Interface > 2)
        return USB_UNSUPPORT;
    return USB_SUCCESS;
}

RESULT UsbHidDev_SetProtocol(void)
{
    u8 wValue0 = pInformation->USBwValue0;
    ProtocolValue = wValue0;
    return USB_SUCCESS;
}

u8 *UsbHidDev_GetProtocolValue(u16 Length)
{
    if (Length == 0) {
        pInformation->Ctrl_Info.Usb_wLength = 1;
        return NULL;
    } else {
        return (u8 *)(&ProtocolValue);
    }
}

/* =================================================================
 * Init / Reset
 * ================================================================= */
void UsbHidDev_init(void)
{
    Get_SerialNum();
    pInformation->Current_Configuration = 0;
    PowerOn();
    _SetISTR(0);
    wInterrupt_Mask = IMR_MSK;
    _SetCNTR(wInterrupt_Mask);
    bDeviceState = UNCONNECTED;
}

void UsbHidDev_Reset(void)
{
    pInformation->Current_Configuration = 0;
    pInformation->Current_Interface = 0;
    _SetBTABLE(BTABLE_ADDRESS);
    _SetEPType(ENDP0, EP_CONTROL);
    _SetEPRxAddr(ENDP0, ENDP0_RXADDR);
    _SetEPTxAddr(ENDP0, ENDP0_TXADDR);
    SetEPRxCount(ENDP0, Device_Property.MaxPacketSize);
    SetEPRxValid(ENDP0);

    /* EP1: HID Interrupt IN/OUT */
    _SetEPType(ENDP1, EP_INTERRUPT);
    SetEPTxAddr(ENDP1, ENDP1_TXADDR);
    SetEPTxCount(ENDP1, HID_EP_BUF_SIZE);
    SetEPTxStatus(ENDP1, EP_TX_NAK);
    SetEPRxAddr(ENDP1, ENDP1_RXADDR);
    SetEPRxCount(ENDP1, HID_EP_BUF_SIZE);
    SetEPRxStatus(ENDP1, EP_RX_VALID);

    /* EP2: CDC Notification IN */
    _SetEPType(ENDP2, EP_INTERRUPT);
    SetEPTxAddr(ENDP2, ENDP2_TXADDR);
    SetEPTxStatus(ENDP2, EP_TX_NAK);

    /* EP3: CDC Data */
    _SetEPType(ENDP3, EP_BULK);
    SetEPRxAddr(ENDP3, ENDP3_RXADDR);
    SetEPRxStatus(ENDP3, EP_RX_VALID);
    SetEPTxAddr(ENDP3, ENDP3_TXADDR);
    SetEPTxCount(ENDP3, 64);
    SetEPTxStatus(ENDP3, EP_TX_NAK);

    /* EP4: WinUSB */
    _SetEPType(ENDP4, EP_BULK);
    SetEPRxAddr(ENDP4, ENDP4_RXADDR);
    SetEPRxStatus(ENDP4, EP_RX_VALID);
    SetEPTxAddr(ENDP4, ENDP4_TXADDR);
    SetEPTxCount(ENDP4, 64);
    SetEPTxStatus(ENDP4, EP_TX_NAK);

    bDeviceState = ATTACHED;
}
