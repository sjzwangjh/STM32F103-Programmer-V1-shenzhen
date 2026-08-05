/*
 * ????: usb_prop.c
 * ????: USB ?? / USB ????
 * ????: ???
 * ????: ??????? Bootloader ??????????????
 * ????: ????????????????????????????? GB2312/CP936 ?????
 */
/******************** (C) COPYRIGHT 2008 STMicroelectronics ********************
* File Name          : usb_prop.c
* ??»¶?             : usb_prop.c
* Author             : MCD Application Team
* ä½?              : MCD åº???¢é?
* Version            : V2.2.0
* Date               : 06/13/2008
* Description        : All processings related to UsbHidDev Mouse Demo
* ??¿°                : USB HID è®¾å?å±??§å????HID é¼??æ¼?¤ºç¨???********************************************************************************
*******************************************************************************/

#include "sys.h"
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
#include "led.h"

static ONE_DESCRIPTOR g_customDesc;
static u8 *GetCustomDescriptor(u16 Length)
{
  return Standard_GetDescriptorData(Length, &g_customDesc);
}

/* WCID / MS-OS request counters (incremented in the USB ISR, printed by main) */
volatile u32 g_wcidReqCnt[8] = {0};
volatile u32 g_descReqCnt[4] = {0};   /* 0=device, 1=config, 2=string, 3=BOS */
volatile u32 g_eeReqCnt = 0;          /* 0xEE MS OS string descriptor */

u32 ProtocolValue;

DEVICE Device_Table =
{
  EP_NUM,
  1
};

DEVICE_PROP Device_Property =
{
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

USER_STANDARD_REQUESTS User_Standard_Requests =
{
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

ONE_DESCRIPTOR Device_Descriptor =
{
  (u8*)UsbHidDev_DeviceDescriptor,
  USB_HID_DEV_SIZ_DEVICE_DESC
};

/* FIX13: WinUSB single vendor interface - remove unnecessary IAD descriptor. */
ONE_DESCRIPTOR Config_Descriptor =
{
  (u8*)UsbHidDev_ConfigDescriptor,
  USB_HID_DEV_SIZ_CONFIG_DESC
};

ONE_DESCRIPTOR UsbHidDev_Report_Descriptor =
{
  (u8 *)UsbHidDev_ReportDescriptor,
  USB_HID_DEV_SIZ_REPORT_DESC
};

ONE_DESCRIPTOR Mouse_Hid_Descriptor =
{
  (u8*)UsbHidDev_ConfigDescriptor + USB_HID_DEV_OFF_HID_DESC,
  USB_HID_DEV_SIZ_HID_DESC
};

ONE_DESCRIPTOR String_Descriptor[4] =
{
  {(u8*)UsbHidDev_StringLangID, USB_HID_DEV_SIZ_STRING_LANGID},
  {(u8*)UsbHidDev_StringVendor, USB_HID_DEV_SIZ_STRING_VENDOR},
  {(u8*)UsbHidDev_StringProduct, USB_HID_DEV_SIZ_STRING_PRODUCT},
  {(u8*)UsbHidDev_StringSerial, USB_HID_DEV_SIZ_STRING_SERIAL}
};

static u8 g_cdcPendingSetLineCoding;

{
  if (Length == 0)
  {
    return NULL;
  }
  return g_hidReportBuf + pInformation->Ctrl_Info.Usb_wOffset;
}


{
  {
  }
}

static u8 *CDC_GetLineCodingData(u16 Length)
{
  if (Length == 0)
  {
    CDC_FillLineCodingBuffer();
    pInformation->Ctrl_Info.Usb_wLength = 7;
    return NULL;
  }
  return CDC_GetLineCodingBuffer() + pInformation->Ctrl_Info.Usb_wOffset;
}

static u8 *CDC_SetLineCodingData(u16 Length)
{
  if (Length == 0)
  {
    pInformation->Ctrl_Info.Usb_rLength = 7;
    return NULL;
  }
  return CDC_GetLineCodingBuffer() + pInformation->Ctrl_Info.Usb_rOffset;
}

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
  pInformation->Current_Feature = UsbHidDev_ConfigDescriptor[7];

  SetBTABLE(BTABLE_ADDRESS);

  SetEPType(ENDP0, EP_CONTROL);
  SetEPTxStatus(ENDP0, EP_TX_STALL);
  SetEPRxAddr(ENDP0, ENDP0_RXADDR);
  SetEPTxAddr(ENDP0, ENDP0_TXADDR);
  Clear_Status_Out(ENDP0);
  SetEPRxCount(ENDP0, Device_Property.MaxPacketSize);
  SetEPRxValid(ENDP0);

  SetEPType(ENDP1, EP_INTERRUPT);
  SetEPTxAddr(ENDP1, ENDP1_TXADDR);
  SetEPRxAddr(ENDP1, ENDP1_RXADDR);
  SetEPRxCount(ENDP1, HID_EP_BUF_SIZE);
  SetEPRxStatus(ENDP1, EP_RX_VALID);
#if HW_USB_HID_SPEED_FULL
  SetEPTxCount(ENDP1, 56);
#else
  SetEPTxCount(ENDP1, 8);
#endif
  SetEPRxStatus(ENDP1, EP_RX_DIS);
  SetEPTxStatus(ENDP1, EP_TX_NAK);

  SetEPType(ENDP2, EP_INTERRUPT);
  SetEPTxAddr(ENDP2, ENDP2_TXADDR);
  SetEPTxCount(ENDP2, 0);
  SetEPRxStatus(ENDP2, EP_RX_DIS);
  SetEPTxStatus(ENDP2, EP_TX_NAK);

  SetEPType(ENDP3, EP_BULK);
  SetEPTxAddr(ENDP3, ENDP3_TXADDR);
  SetEPRxAddr(ENDP3, ENDP3_RXADDR);
  SetEPTxCount(ENDP3, 0);
  SetEPRxCount(ENDP3, 64);
  SetEPTxStatus(ENDP3, EP_TX_NAK);
  SetEPRxStatus(ENDP3, EP_RX_VALID);

  SetEPType(ENDP4, EP_BULK);
  SetEPTxAddr(ENDP4, ENDP4_TXADDR);
  SetEPRxAddr(ENDP4, ENDP4_RXADDR);
  SetEPTxCount(ENDP4, 0);
  SetEPRxCount(ENDP4, 64);
  SetEPTxStatus(ENDP4, EP_TX_NAK);
  SetEPRxStatus(ENDP4, EP_RX_VALID);

  CDC_Init();
  bDeviceState = ATTACHED;
  SetDeviceAddress(0);
}

void UsbHidDev_SetConfiguration(void)
{
  DEVICE_INFO *pInfo = &Device_Info;
  if (pInfo->Current_Configuration != 0)
  {
    bDeviceState = CONFIGURED;
  }
}

void UsbHidDev_SetDeviceAddress(void)
{
  bDeviceState = ADDRESSED;
}

void UsbHidDev_Status_In(void)
{
  if (g_cdcPendingSetLineCoding != 0U)
  {
    CDC_SetLineCodingFromBuffer();
    g_cdcPendingSetLineCoding = 0U;
  }
}

void UsbHidDev_Status_Out(void)
{
}


/* Return BOS Descriptor for USB 2.0 BOS request (WinUSB) */
u8 *UsbHidDev_GetBOSDescriptor(u16 Length)
{
  g_descReqCnt[3]++;
	g_customDesc.Descriptor = (u8 *)UsbHidDev_BOSDescriptor;
  g_customDesc.Descriptor_Size = sizeof(UsbHidDev_BOSDescriptor);
  return Standard_GetDescriptorData(Length, &g_customDesc);
}

RESULT UsbHidDev_Data_Setup(u8 RequestNo)
{
  u8 *(*CopyRoutine)(u16);

  CopyRoutine = NULL;
  LED_ACTIVE = !LED_ACTIVE;  /* toggle on each Data_Setup call */

  if ((Type_Recipient == (STANDARD_REQUEST | DEVICE_RECIPIENT))
      && (RequestNo == GET_DESCRIPTOR)
      && (pInformation->USBwValue1 == USB_BOS_DESCRIPTOR_TYPE))
  {
    LED_ACTIVE = 0;  /* LED ON if BOS requested */
    g_customDesc.Descriptor = (u8 *)UsbHidDev_BOSDescriptor;
    g_customDesc.Descriptor_Size = sizeof(UsbHidDev_BOSDescriptor);
    pInformation->Ctrl_Info.CopyData = GetCustomDescriptor;
    pInformation->Ctrl_Info.Usb_wOffset = 0;
    GetCustomDescriptor(0);
    return USB_SUCCESS;
  }

  if ((pInformation->USBbmRequestType == 0xC0)
      && (RequestNo == WINUSB_MS_VENDOR_CODE)
      && ((pInformation->USBwIndex == WINUSB_REQUEST_GET_DESCRIPTOR_SET) || (pInformation->USBwIndex == 0x0007)))
  {
    g_wcidReqCnt[7]++;
    g_customDesc.Descriptor = (u8 *)UsbHidDev_MSOS20Descriptor;
    g_customDesc.Descriptor_Size = sizeof(UsbHidDev_MSOS20Descriptor);
    pInformation->Ctrl_Info.CopyData = GetCustomDescriptor;
    pInformation->Ctrl_Info.Usb_wOffset = 0;
    GetCustomDescriptor(0);
    return USB_SUCCESS;
  }

  /* Microsoft OS 1.0 Compatible ID request (vendor code from string 0xEE) */
  if ((pInformation->USBbmRequestType == 0xC0)
      && (RequestNo == WINUSB_MS_VENDOR_CODE)
      && (pInformation->USBwIndex == 0x0004))
  {
    g_wcidReqCnt[4]++;
    g_customDesc.Descriptor = (u8 *)UsbHidDev_MSOS10CompatDescriptor;
    g_customDesc.Descriptor_Size = sizeof(UsbHidDev_MSOS10CompatDescriptor);
    pInformation->Ctrl_Info.CopyData = GetCustomDescriptor;
    pInformation->Ctrl_Info.Usb_wOffset = 0;
    GetCustomDescriptor(0);
    return USB_SUCCESS;
  }

  /* Microsoft OS 1.0 Extended Properties request (WinUSB DeviceInterfaceGUID) */
  if ((pInformation->USBbmRequestType == 0xC0)
      && (RequestNo == WINUSB_MS_VENDOR_CODE)
      && (pInformation->USBwIndex == 0x0005))
  {
    g_wcidReqCnt[5]++;
    if (pInformation->USBwValue0 != 3U)   /* interface 3 (WinUSB) only */
    {
      return USB_UNSUPPORT;
    }
    g_customDesc.Descriptor = (u8 *)UsbHidDev_MSOS10ExtPropsDescriptor;
    g_customDesc.Descriptor_Size = sizeof(UsbHidDev_MSOS10ExtPropsDescriptor);
    pInformation->Ctrl_Info.CopyData = GetCustomDescriptor;
    pInformation->Ctrl_Info.Usb_wOffset = 0;
    GetCustomDescriptor(0);
    return USB_SUCCESS;
  }

  if ((RequestNo == GET_DESCRIPTOR)
      && (Type_Recipient == (STANDARD_REQUEST | INTERFACE_RECIPIENT))
      && (pInformation->USBwIndex0 == 0))
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
  else if ((Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT))
           && RequestNo == GET_PROTOCOL)
  {
    CopyRoutine = UsbHidDev_GetProtocolValue;
  }
  else if ((Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT))
  else if ((Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT))
           && RequestNo == CDC_GET_LINE_CODING
           && pInformation->USBwIndex0 == 1U)
  {
    CopyRoutine = CDC_GetLineCodingData;
  }
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
  else if ((Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT))

  if (CopyRoutine == NULL)
  {
    return USB_UNSUPPORT;
  }

  pInformation->Ctrl_Info.CopyData = CopyRoutine;
  pInformation->Ctrl_Info.Usb_wOffset = 0;
  (*CopyRoutine)(0);
  return USB_SUCCESS;
}


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

u8 *UsbHidDev_GetDeviceDescriptor(u16 Length)
{
  g_descReqCnt[0]++;
  return Standard_GetDescriptorData(Length, &Device_Descriptor);
}

u8 *UsbHidDev_GetConfigDescriptor(u16 Length)
{
  g_descReqCnt[1]++;
  return Standard_GetDescriptorData(Length, &Config_Descriptor);
}

u8 *UsbHidDev_GetStringDescriptor(u16 Length)
{
  u8 wValue0 = pInformation->USBwValue0;
  if (wValue0 == 0xEE)
  {
    g_eeReqCnt++;
    g_customDesc.Descriptor = (u8 *)UsbHidDev_StringMSOS;
    g_customDesc.Descriptor_Size = sizeof(UsbHidDev_StringMSOS);
    return Standard_GetDescriptorData(Length, &g_customDesc);
  }
  g_descReqCnt[2]++;
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

