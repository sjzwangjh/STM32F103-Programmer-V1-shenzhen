/*
 * USB Property Handler - HID + CDC + WinUSB Composite
 *
 * HID transport migrated from EP0 Feature Reports to EP1 Interrupt IN/OUT.
 * SET_REPORT/GET_REPORT handlers removed; data now flows through EP1.
 * Standard USB requests (GET_DESCRIPTOR etc.) and CDC requests unchanged.
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
    {(u8 *)UsbHidDev_StringSerial,  USB_HID_DEV_SIZ_STRING_SERIAL}
};

ONE_DESCRIPTOR g_customDesc;

/* =================================================================
 * UsbHidDev_Data_Setup
 *
 * Handles EP0 control transfer SETUP stage.
 * Standard requests forwarded to STM32 USB library.
 * HID class requests: GET_PROTOCOL, SET_PROTOCOL only.
 * GET_REPORT/SET_REPORT removed - HID data uses EP1 interrupt endpoints.
 * CDC class requests: GET_LINE_CODING, SET_LINE_CODING, SET_CONTROL_LINE_STATE.
 * ================================================================= */
RESULT UsbHidDev_Data_Setup(u8 RequestNo)
{
    u8 *(*CopyRoutine)(u16) = NULL;

    /* ---- Standard Requests ---- */
    if (Type_Recipient == (STANDARD_REQUEST | INTERFACE_RECIPIENT)
        && RequestNo == GET_DESCRIPTOR)
    {
        if (pInformation->USBwValue1 == REPORT_DESCRIPTOR)
        {
            CopyRoutine = UsbHidDev_GetReportDescriptor;
        }
        else if (pInformation->USBwValue1 == HID_DESCRIPTOR_TYPE)
        {
            CopyRoutine = UsbHidDev_GetHIDDescriptor;
        }
    }
    else if ((Type_Recipient == (STANDARD_REQUEST | INTERFACE_RECIPIENT))
             && pInformation->USBwValue1 == HID_DESCRIPTOR_TYPE)
    {
        /* Standard GET_DESCRIPTOR for HID descriptor */
        CopyRoutine = UsbHidDev_GetHIDDescriptor;
    }

    /*** MS OS Descriptors ***/
    else if (pInformation->USBbmRequestType == 0xC0U
             && RequestNo == WINUSB_REQUEST_GET_DESCRIPTOR_SET
             && pInformation->USBwIndex1 == WINUSB_MS_VENDOR_CODE)
    {
        g_customDesc.Descriptor     = (u8 *)UsbHidDev_MSOS10CompatDescriptor;
        g_customDesc.Descriptor_Size = USB_HID_DEV_SIZ_MSOS10_COMPAT_DESC;
        CopyRoutine = WinUSB_MSOS_GetDescriptorSet;
    }

    /*** BOS Descriptor ***/
    else if (pInformation->USBbmRequestType == 0x80U
             && RequestNo == GET_DESCRIPTOR
             && pInformation->USBwValue1 == USB_BOS_DESCRIPTOR_TYPE)
    {
        g_customDesc.Descriptor     = (u8 *)UsbHidDev_BOSDescriptor;
        g_customDesc.Descriptor_Size = USB_HID_DEV_SIZ_BOS_DESC;
        CopyRoutine = WinUSB_MSOS_GetDescriptorSet;
    }

    /*** MS OS 2.0 Descriptor ***/
    else if (pInformation->USBbmRequestType == 0xC1U
             && RequestNo == WINUSB_REQUEST_GET_DESCRIPTOR_SET
             && pInformation->USBwIndex1 == WINUSB_MS_VENDOR_CODE)
    {
        g_customDesc.Descriptor     = (u8 *)UsbHidDev_MSOS20Descriptor;
        g_customDesc.Descriptor_Size = USB_HID_DEV_SIZ_MSOS20_DESC;
        CopyRoutine = WinUSB_MSOS_GetDescriptorSet;
    }

    /*** GET_PROTOCOL ***/
    else if ((Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT))
             && RequestNo == GET_PROTOCOL)
    {
        CopyRoutine = UsbHidDev_GetProtocolValue;
    }

    /*** CDC GET_LINE_CODING ***/
    else if ((Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT))
             && RequestNo == CDC_GET_LINE_CODING
             && pInformation->USBwIndex0 == 1U)
    {
        CopyRoutine = CDC_GetLineCodingData;
    }

    /*** CDC SET_LINE_CODING ***/
    else if ((Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT))
             && RequestNo == CDC_SET_LINE_CODING
             && pInformation->USBwIndex0 == 1U)
    {
        if (pInformation->USBwLength != 7U)
        {
            return USB_UNSUPPORT;
        }
        g_cdcPendingSetLineCoding = 1U;
        pInformation->Ctrl_Info.Usb_rOffset = 0;
        pInformation->Ctrl_Info.Usb_rLength = 7;
        pInformation->Ctrl_Info.CopyData = CDC_SetLineCodingData;
        return USB_SUCCESS;
    }

    /*
     * GET_REPORT / SET_REPORT removed.
     * HID data transport now uses EP1 Interrupt IN/OUT endpoints.
     * Windows HID driver sends Output reports via EP1 OUT,
     * and reads Input reports via EP1 IN.
     */

    if (CopyRoutine == NULL)
    {
        return USB_UNSUPPORT;
    }

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

    if (wValue0 == 0xEE)
    {
        g_customDesc.Descriptor = (u8 *)UsbHidDev_StringMSOS;
        g_customDesc.Descriptor_Size = sizeof(UsbHidDev_StringMSOS);
        return Standard_GetDescriptorData(Length, &g_customDesc);
    }
    if (wValue0 >= 4)
    {
        return NULL;
    }
    else
    {
        return Standard_GetDescriptorData(Length, &String_Descriptor[wValue0]);
    }
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
    {
        return USB_UNSUPPORT;
    }
    else if (Interface > 3)
    {
        return USB_UNSUPPORT;
    }
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
    if (Length == 0)
    {
        pInformation->Ctrl_Info.Usb_wLength = 1;
        return NULL;
    }
    else
    {
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
    _SetCNTR(CNTR_FRES);
    wInterrupt_Mask = CNTR_RESETM | CNTR_SUSPM | CNTR_WKUPM;
    _SetCNTR(IMR_MSK);
}

void UsbHidDev_Reset(void)
{
    pInformation->Current_Configuration = 0;
    pInformation->Current_Interface = 0;
    _SetBTABLE(BTABLE_ADDRESS);
    _SetEP0(ENDP0_RXADDR, ENDP0_TXADDR);
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
