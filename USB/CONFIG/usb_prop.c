/******************** (C) COPYRIGHT 2008 STMicroelectronics ********************
* File Name          : usb_prop.c
* ļ              : usb_prop.c
* Author             : MCD Application Team
*                : MCD ӦŶ
* Version            : V2.2.0
* Date               : 06/13/2008
* Description        : All processings related to UsbHidDev Mouse Demo
*                 : USB HID 豸ԴHID ʾ
********************************************************************************
* THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
* WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE TIME.
* AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
* INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
* CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
* INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
* ̼οּΪͻṩƷıϢԱʡʱ䡣
* STMicroelectronics ʹñ̼κֱӡӻ򸽴ʧеΡ
*******************************************************************************/

/* Includes ------------------------------------------------------------------*/
/* ͷļ */
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

/* Private typedef -----------------------------------------------------------*/
/* ˽Ͷ */
/* Private define ------------------------------------------------------------*/
/* ˽к궨 */
/* Private macro -------------------------------------------------------------*/
/* ˽к */
/* Private variables ---------------------------------------------------------*/
/* ˽б */
u32 ProtocolValue;  /* Эֵ */

#if DEBUG_HARDWARE_CONFIG
static void UsbDebugWriteHex8(u8 value)
{
  static const char hexTable[] = "0123456789ABCDEF";
  uart1_WriteByte((u8)hexTable[(value >> 4) & 0x0F]);
  uart1_WriteByte((u8)hexTable[value & 0x0F]);
}

static void UsbDebugWriteHex16(u16 value)
{
  UsbDebugWriteHex8((u8)(value >> 8));
  UsbDebugWriteHex8((u8)value);
}
#endif

/* -------------------------------------------------------------------------- */
/*  Structures initializations */
/* ṹʼ */
/* -------------------------------------------------------------------------- */

DEVICE Device_Table =
  {
    EP_NUM,
    1
  };

DEVICE_PROP Device_Property =
  {
    UsbHidDev_init,                 /* 豸ʼ */
    UsbHidDev_Reset,                /* 豸λ */
    UsbHidDev_Status_In,            /* ״̬ */
    UsbHidDev_Status_Out,           /* ״̬ */
    UsbHidDev_Data_Setup,           /*  */
    UsbHidDev_NoData_Setup,         /*  */
    UsbHidDev_Get_Interface_Setting,/* ȡӿ */
    UsbHidDev_GetDeviceDescriptor,  /* ȡ豸 */
    UsbHidDev_GetConfigDescriptor,  /* ȡ */
    UsbHidDev_GetStringDescriptor,  /* ȡַ */
    0,
    0x40 /*MAX PACKET SIZE*/       /* С 64 ֽ */
  };
USER_STANDARD_REQUESTS User_Standard_Requests =
  {
    UsbHidDev_GetConfiguration,     /* ȡ */
    UsbHidDev_SetConfiguration,     /*  */
    UsbHidDev_GetInterface,         /* ȡӿ */
    UsbHidDev_SetInterface,         /* ýӿ */
    UsbHidDev_GetStatus,            /* ȡ״̬ */
    UsbHidDev_ClearFeature,         /*  */
    UsbHidDev_SetEndPointFeature,   /* ö˵ */
    UsbHidDev_SetDeviceFeature,     /* 豸 */
    UsbHidDev_SetDeviceAddress      /* 豸ַ */
  };

ONE_DESCRIPTOR Device_Descriptor =
  {
    (u8*)UsbHidDev_DeviceDescriptor,
    USB_HID_DEV_SIZ_DEVICE_DESC
  };

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
    {0, 0}                                      /* no serial */
                                                /* к */
  };

/* Extern variables ----------------------------------------------------------*/
/* ⲿ */
/* Private function prototypes -----------------------------------------------*/
/* ˽кԭ */
/* Extern function prototypes ------------------------------------------------*/
/* ⲿԭ */
/* Private functions ---------------------------------------------------------*/
/* ˽к */

/* BUFFERS FOR GET/SET REPORT */
/* GET/SET REPORT  */
static u8 g_hidReportBuf[128];  /* ݻ */
static u8 g_hidReportLen;       /* ݳ */
static u8 g_hidPendingSetReportId;
static u8 g_hidPendingSetReport;
static u8 g_cdcPendingSetLineCoding;

static u8 *GetReport_CopyRoutine(u16 Length)
{
    if (Length == 0)
    {
        pInformation->Ctrl_Info.Usb_wLength = g_hidReportLen;
        return NULL;
    }
    return g_hidReportBuf + pInformation->Ctrl_Info.Usb_wOffset;
}

static u8 *UsbHidDev_SetReportData(u16 Length)
{
    if (Length == 0)
    {
        pInformation->Ctrl_Info.Usb_rLength = pInformation->USBwLength;
        return NULL;
    }
    return g_hidReportBuf + pInformation->Ctrl_Info.Usb_rOffset;
}


static void UsbHidDev_ProcessPendingSetReport(void)
{
  if (g_hidPendingSetReport != 0U)
  {
    HID_Rx_Store(g_hidPendingSetReportId, g_hidReportBuf, g_hidReportLen);
    g_hidPendingSetReport = 0U;
    g_hidPendingSetReportId = 0U;
    g_hidReportLen = 0U;
    HID_ResetRequestState();
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
/*******************************************************************************
* Function Name  : UsbHidDev_init.
*           : UsbHidDev_init
* Description    : UsbHidDev Mouse init routine.
*             : USB HID 豸ʼ
* Input          : None.
*             : 
* Output         : None.
*             : 
* Return         : None.
* ֵ          : 
*******************************************************************************/
void UsbHidDev_init(void)
{

  /* Update the serial number string descriptor with the data from the unique
  ID*/
  /* ʹΨһ ID кַ */
  Get_SerialNum();

  pInformation->Current_Configuration = 0;
  /* Connect the device */
  /* 豸 */
  PowerOn();
  /* USB interrupts initialization */
  /* USB жϳʼ */
  _SetISTR(0);               /* clear pending interrupts */
                              /* ж */
  wInterrupt_Mask = IMR_MSK;
  _SetCNTR(wInterrupt_Mask); /* set interrupts mask */
                              /* ж */
  bDeviceState = UNCONNECTED;
}

/*******************************************************************************
* Function Name  : UsbHidDev_Reset.
*           : UsbHidDev_Reset
* Description    : UsbHidDev Mouse reset routine.
*             : USB HID 豸λ
* Input          : None.
*             : 
* Output         : None.
*             : 
* Return         : None.
* ֵ          : 
*******************************************************************************/
void UsbHidDev_Reset(void)
{
  /* Set UsbHidDev_DEVICE as not configured */
  /* 豸Ϊδ״̬ */
  pInformation->Current_Configuration = 0;
  pInformation->Current_Interface = 0;/*the default Interface*/
                                      /* ĬϽӿ */

  /* Current Feature initialization */
  /* ǰԳʼ */
  pInformation->Current_Feature = UsbHidDev_ConfigDescriptor[7];

  SetBTABLE(BTABLE_ADDRESS);

  /* Initialize Endpoint 0 */
  /* ʼ˵ 0 */
  SetEPType(ENDP0, EP_CONTROL);
  SetEPTxStatus(ENDP0, EP_TX_STALL);
  SetEPRxAddr(ENDP0, ENDP0_RXADDR);
  SetEPTxAddr(ENDP0, ENDP0_TXADDR);
  Clear_Status_Out(ENDP0);
  SetEPRxCount(ENDP0, Device_Property.MaxPacketSize);
  SetEPRxValid(ENDP0);

  /* Initialize Endpoint 1 */
  /* ʼ˵ 1 */
  SetEPType(ENDP1, EP_INTERRUPT);
  SetEPTxAddr(ENDP1, ENDP1_TXADDR);
#if HW_USB_HID_SPEED_FULL
  SetEPTxCount(ENDP1, 64);
#else
  SetEPTxCount(ENDP1, 8);
#endif
  SetEPRxStatus(ENDP1, EP_RX_DIS);
  SetEPTxStatus(ENDP1, EP_TX_NAK);

  /* Initialize Endpoint 2: CDC notification IN */
  SetEPType(ENDP2, EP_INTERRUPT);
  SetEPTxAddr(ENDP2, ENDP2_TXADDR);
  SetEPTxCount(ENDP2, 0);
  SetEPRxStatus(ENDP2, EP_RX_DIS);
  SetEPTxStatus(ENDP2, EP_TX_NAK);

  /* Initialize Endpoint 3: CDC data IN/OUT */
  SetEPType(ENDP3, EP_BULK);
  SetEPTxAddr(ENDP3, ENDP3_TXADDR);
  SetEPRxAddr(ENDP3, ENDP3_RXADDR);
  SetEPTxCount(ENDP3, 0);
  SetEPRxCount(ENDP3, 64);
  SetEPTxStatus(ENDP3, EP_TX_NAK);
  SetEPRxStatus(ENDP3, EP_RX_VALID);
  CDC_Init();
  bDeviceState = ATTACHED;

  /* Set this device to response on default address */
  /* ô豸ӦĬϵַ 0 */
  SetDeviceAddress(0);
}
/*******************************************************************************
* Function Name  : UsbHidDev_SetConfiguration.
*           : UsbHidDev_SetConfiguration
* Description    : Udpade the device state to configured.
*             : 豸״̬Ϊ
* Input          : None.
*             : 
* Output         : None.
*             : 
* Return         : None.
* ֵ          : 
*******************************************************************************/
void UsbHidDev_SetConfiguration(void)
{
  DEVICE_INFO *pInfo = &Device_Info;

  if (pInfo->Current_Configuration != 0)
  {
    /* Device configured */
    /* 豸 */
    bDeviceState = CONFIGURED;
  }
}
/*******************************************************************************
* Function Name  : UsbHidDev_SetDeviceAddress
*           : UsbHidDev_SetDeviceAddress
* Description    : Udpade the device state to addressed.
*             : 豸״̬Ϊѱַ
* Input          : None.
*             : 
* Output         : None.
*             : 
* Return         : None.
* ֵ          : 
*******************************************************************************/
void UsbHidDev_SetDeviceAddress (void)
{
  bDeviceState = ADDRESSED;
}
/*******************************************************************************
* Function Name  : UsbHidDev_Status_In.
*           : UsbHidDev_Status_In
* Description    : UsbHidDev status IN routine.
*             : USB HID 豸״̬
* Input          : None.
*             : 
* Output         : None.
*             : 
* Return         : None.
* ֵ          : 
*******************************************************************************/
void UsbHidDev_Status_In(void)
{
  /*
   * HID SET_REPORT Ѿ g_hidReportBuf
   *  Status IN ׶ STK500 ƶ˵״̬׶ʱ
   * Э鴦͵Դӡռùãŵ GET_REPORT żʧܡ
   * ʵʽӺ GET_REPORT ǰִС
   */

  if (g_cdcPendingSetLineCoding != 0U)
  {
    CDC_SetLineCodingFromBuffer();
    g_cdcPendingSetLineCoding = 0U;
  }
}

/*******************************************************************************
* Function Name  : UsbHidDev_Status_Out
*           : UsbHidDev_Status_Out
* Description    : UsbHidDev status OUT routine.
*             : USB HID 豸״̬
* Input          : None.
*             : 
* Output         : None.
*             : 
* Return         : None.
* ֵ          : 
*******************************************************************************/
void UsbHidDev_Status_Out (void)
{
}

/*******************************************************************************
* Function Name  : UsbHidDev_Data_Setup
*           : UsbHidDev_Data_Setup
* Description    : Handle the data class specific requests.
*             : ضGET_DESCRIPTORGET_REPORT ȣ
* Input          : Request Nb.
*             : RequestNo = 
* Output         : None.
*             : 
* Return         : USB_UNSUPPORT or USB_SUCCESS.
* ֵ          : USB_UNSUPPORT֧֣ USB_SUCCESSɹ
*******************************************************************************/
RESULT UsbHidDev_Data_Setup(u8 RequestNo)
{
  u8 *(*CopyRoutine)(u16);

#if DEBUG_HARDWARE_CONFIG
  uart1_WriteString("USB_DATA_SETUP req=");
  UsbDebugWriteHex8(RequestNo);
  uart1_WriteString(" type=");
  UsbDebugWriteHex8(Type_Recipient);
  uart1_WriteString(" val=");
  UsbDebugWriteHex8(pInformation->USBwValue1);
  UsbDebugWriteHex8(pInformation->USBwValue0);
  uart1_WriteString(" idx=");
  UsbDebugWriteHex8(pInformation->USBwIndex1);
  UsbDebugWriteHex8(pInformation->USBwIndex0);
  uart1_WriteString(" len=");
  UsbDebugWriteHex16(pInformation->USBwLength);
  uart1_WriteString("\r\n");
#endif
  CopyRoutine = NULL;
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

  } /* End of GET_DESCRIPTOR */
    /* GET_DESCRIPTOR  */

  /*** GET_PROTOCOL ***/
  /*** ȡЭ ***/
  else if ((Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT))
           && RequestNo == GET_PROTOCOL)
  {
    CopyRoutine = UsbHidDev_GetProtocolValue;
  }

  /*** GET_REPORT: Host reads data from device ***/
  /*** ȡ棺豸ȡ ***/
  else if ((Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT))
           && RequestNo == GET_REPORT)
  {
    u16 outLen;
    u8 *buf;
    HID_BeginReportRequest((u8)pInformation->USBwValue0, REQUEST_TYPE_HID_FIRST);
    UsbHidDev_ProcessPendingSetReport();
    buf = HID_GetReport_Buffer((u8)pInformation->USBwValue0,
                               pInformation->USBwLength,
                               &outLen);
    if (buf && outLen)
    {
      g_hidReportLen = outLen;
      {
        u8 i; for (i=0; i<outLen; i++) g_hidReportBuf[i]=buf[i];
      }
      CopyRoutine = GetReport_CopyRoutine;
    }
    else
    {
      g_hidReportLen = 0;
      CopyRoutine = GetReport_CopyRoutine;
    }
  }
  /*** SET_REPORT: Host writes data to device ***/
  /*** ñ棺豸д ***/
  /*** CDC GET_LINE_CODING: Host reads UART format ***/
  else if ((Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT))
           && RequestNo == CDC_GET_LINE_CODING
           && pInformation->USBwIndex0 == 1U)
  {
    CopyRoutine = CDC_GetLineCodingData;
  }
  /*** CDC SET_LINE_CODING: Host writes UART format ***/
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
           && RequestNo == SET_REPORT)
  {
    if (pInformation->USBwLength == 0U || pInformation->USBwLength > sizeof(g_hidReportBuf))
    {
      return USB_UNSUPPORT;
    }

    HID_BeginReportRequest((u8)pInformation->USBwValue0, REQUEST_TYPE_HID_FIRST);
    g_hidPendingSetReport = 1U;
    g_hidPendingSetReportId = (u8)pInformation->USBwValue0;
    g_hidReportLen = (u8)pInformation->USBwLength;
    pInformation->Ctrl_Info.Usb_rOffset = 0;
    pInformation->Ctrl_Info.Usb_rLength = pInformation->USBwLength;
    pInformation->Ctrl_Info.CopyData = UsbHidDev_SetReportData;
    return USB_SUCCESS;
  }

  if (CopyRoutine == NULL)
  {
    return USB_UNSUPPORT;
  }

  pInformation->Ctrl_Info.CopyData = CopyRoutine;
  pInformation->Ctrl_Info.Usb_wOffset = 0;
  (*CopyRoutine)(0);
  return USB_SUCCESS;
}

/*******************************************************************************
* Function Name  : UsbHidDev_NoData_Setup
*           : UsbHidDev_NoData_Setup
* Description    : handle the no data class specific requests
*             : ضSET_PROTOCOL ȣ
* Input          : Request Nb.
*             : RequestNo = 
* Output         : None.
*             : 
* Return         : USB_UNSUPPORT or USB_SUCCESS.
* ֵ          : USB_UNSUPPORT֧֣ USB_SUCCESSɹ
*******************************************************************************/
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
/*******************************************************************************
* Function Name  : UsbHidDev_GetDeviceDescriptor.
*           : UsbHidDev_GetDeviceDescriptor
* Description    : Gets the device descriptor.
*             : ȡ豸
* Input          : Length
*             : Length = 󳤶
* Output         : None.
*             : 
* Return         : The address of the device descriptor.
* ֵ          : 豸ַ
*******************************************************************************/
u8 *UsbHidDev_GetDeviceDescriptor(u16 Length)
{
  return Standard_GetDescriptorData(Length, &Device_Descriptor);
}

/*******************************************************************************
* Function Name  : UsbHidDev_GetConfigDescriptor.
*           : UsbHidDev_GetConfigDescriptor
* Description    : Gets the configuration descriptor.
*             : ȡ
* Input          : Length
*             : Length = 󳤶
* Output         : None.
*             : 
* Return         : The address of the configuration descriptor.
* ֵ          : ַ
*******************************************************************************/
u8 *UsbHidDev_GetConfigDescriptor(u16 Length)
{
  return Standard_GetDescriptorData(Length, &Config_Descriptor);
}

/*******************************************************************************
* Function Name  : UsbHidDev_GetStringDescriptor
*           : UsbHidDev_GetStringDescriptor
* Description    : Gets the string descriptors according to the needed index
*             : ȡַ
* Input          : Length
*             : Length = 󳤶
* Output         : None.
*             : 
* Return         : The address of the string descriptors.
* ֵ          : ַַ
*******************************************************************************/
u8 *UsbHidDev_GetStringDescriptor(u16 Length)
{
  u8 wValue0 = pInformation->USBwValue0;
  if (wValue0 >= 4)
  {
    return NULL;
  }
  else
  {
    return Standard_GetDescriptorData(Length, &String_Descriptor[wValue0]);
  }
}

/*******************************************************************************
* Function Name  : UsbHidDev_GetReportDescriptor.
*           : UsbHidDev_GetReportDescriptor
* Description    : Gets the HID report descriptor.
*             : ȡ HID 
* Input          : Length
*             : Length = 󳤶
* Output         : None.
*             : 
* Return         : The address of the configuration descriptor.
* ֵ          : HID ַ
*******************************************************************************/
u8 *UsbHidDev_GetReportDescriptor(u16 Length)
{
  return Standard_GetDescriptorData(Length, &UsbHidDev_Report_Descriptor);
}

/*******************************************************************************
* Function Name  : UsbHidDev_GetHIDDescriptor.
*           : UsbHidDev_GetHIDDescriptor
* Description    : Gets the HID descriptor.
*             : ȡ HID 
* Input          : Length
*             : Length = 󳤶
* Output         : None.
*             : 
* Return         : The address of the configuration descriptor.
* ֵ          : HID ַ
*******************************************************************************/
u8 *UsbHidDev_GetHIDDescriptor(u16 Length)
{
  return Standard_GetDescriptorData(Length, &Mouse_Hid_Descriptor);
}

/*******************************************************************************
* Function Name  : UsbHidDev_Get_Interface_Setting.
*           : UsbHidDev_Get_Interface_Setting
* Description    : tests the interface and the alternate setting according to the
*                  supported one.
*             : ԽӿںͱǷֵ֧ƥ
* Input          : - Interface : interface number.
*                  - AlternateSetting : Alternate Setting number.
*             : Interface = ӿںţAlternateSetting = ú
* Output         : None.
*             : 
* Return         : USB_SUCCESS or USB_UNSUPPORT.
* ֵ          : USB_SUCCESSɹ USB_UNSUPPORT֧֣
*******************************************************************************/
RESULT UsbHidDev_Get_Interface_Setting(u8 Interface, u8 AlternateSetting)
{
  if (AlternateSetting > 0)
  {
    return USB_UNSUPPORT;
  }
  else if (Interface > 2)
  {
    return USB_UNSUPPORT;
  }
  return USB_SUCCESS;
}

/*******************************************************************************
* Function Name  : UsbHidDev_SetProtocol
*           : UsbHidDev_SetProtocol
* Description    : UsbHidDev Set Protocol request routine.
*             : USB HID Э
* Input          : None.
*             : 
* Output         : None.
*             : 
* Return         : USB SUCCESS.
* ֵ          : USB_SUCCESSɹ
*******************************************************************************/
RESULT UsbHidDev_SetProtocol(void)
{
  u8 wValue0 = pInformation->USBwValue0;
  ProtocolValue = wValue0;
  return USB_SUCCESS;
}

/*******************************************************************************
* Function Name  : UsbHidDev_GetProtocolValue
*           : UsbHidDev_GetProtocolValue
* Description    : get the protocol value
*             : ȡЭֵ
* Input          : Length.
*             : Length = 󳤶
* Output         : None.
*             : 
* Return         : address of the protcol value.
* ֵ          : Эֵĵַ
*******************************************************************************/
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

