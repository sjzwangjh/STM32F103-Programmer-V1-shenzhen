/******************** (C) COPYRIGHT 2008 STMicroelectronics ********************
* File Name          : usb_prop.c
* 文件名              : usb_prop.c
* Author             : MCD Application Team
* 作者               : MCD 应用团队
* Version            : V2.2.0
* Date               : 06/13/2008
* Description        : All processings related to UsbHidDev Mouse Demo
* 描述                : USB HID 设备属性处理（HID 鼠标演示程序）
********************************************************************************
* THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
* WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE TIME.
* AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
* INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
* CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
* INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
* 本固件仅供参考，旨在为客户提供其产品的编程信息以便节省时间。
* STMicroelectronics 不对因使用本固件而产生的任何直接、间接或附带损失承担责任。
*******************************************************************************/

/* Includes ------------------------------------------------------------------*/
/* 包含头文件 */
#include "hw_USB_config.h"
#include "usb_lib.h"
#include "usb_conf.h"
#include "usb_prop.h"
#include "usb_desc.h"
#include "usb_pwr.h"
#include "usb_hid_user.h"
#include "usb_cdc_user.h"

/* Private typedef -----------------------------------------------------------*/
/* 私有类型定义 */
/* Private define ------------------------------------------------------------*/
/* 私有宏定义 */
/* Private macro -------------------------------------------------------------*/
/* 私有宏 */
/* Private variables ---------------------------------------------------------*/
/* 私有变量 */
u32 ProtocolValue;  /* 协议值 */

/* -------------------------------------------------------------------------- */
/*  Structures initializations */
/* 结构体初始化 */
/* -------------------------------------------------------------------------- */

DEVICE Device_Table =
  {
    EP_NUM,
    1
  };

DEVICE_PROP Device_Property =
  {
    UsbHidDev_init,                 /* 设备初始化 */
    UsbHidDev_Reset,                /* 设备复位 */
    UsbHidDev_Status_In,            /* 状态输入 */
    UsbHidDev_Status_Out,           /* 状态输出 */
    UsbHidDev_Data_Setup,           /* 数据类请求处理 */
    UsbHidDev_NoData_Setup,         /* 无数据类请求处理 */
    UsbHidDev_Get_Interface_Setting,/* 获取接口设置 */
    UsbHidDev_GetDeviceDescriptor,  /* 获取设备描述符 */
    UsbHidDev_GetConfigDescriptor,  /* 获取配置描述符 */
    UsbHidDev_GetStringDescriptor,  /* 获取字符串描述符 */
    0,
    0x40 /*MAX PACKET SIZE*/       /* 最大包大小 64 字节 */
  };
USER_STANDARD_REQUESTS User_Standard_Requests =
  {
    UsbHidDev_GetConfiguration,     /* 获取配置 */
    UsbHidDev_SetConfiguration,     /* 设置配置 */
    UsbHidDev_GetInterface,         /* 获取接口 */
    UsbHidDev_SetInterface,         /* 设置接口 */
    UsbHidDev_GetStatus,            /* 获取状态 */
    UsbHidDev_ClearFeature,         /* 清除特性 */
    UsbHidDev_SetEndPointFeature,   /* 设置端点特性 */
    UsbHidDev_SetDeviceFeature,     /* 设置设备特性 */
    UsbHidDev_SetDeviceAddress      /* 设置设备地址 */
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
                                                /* 无序列号 */
  };

/* Extern variables ----------------------------------------------------------*/
/* 外部变量 */
/* Private function prototypes -----------------------------------------------*/
/* 私有函数原型 */
/* Extern function prototypes ------------------------------------------------*/
/* 外部函数原型 */
/* Private functions ---------------------------------------------------------*/
/* 私有函数 */

/* BUFFERS FOR GET/SET REPORT */
/* GET/SET REPORT 缓冲区 */
static u8 g_hidReportBuf[128];  /* 报告数据缓冲区 */
static u8 g_hidReportLen;       /* 报告数据长度 */
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
* 函数名          : UsbHidDev_init
* Description    : UsbHidDev Mouse init routine.
* 描述            : USB HID 设备初始化例程
* Input          : None.
* 输入            : 无
* Output         : None.
* 输出            : 无
* Return         : None.
* 返回值          : 无
*******************************************************************************/
void UsbHidDev_init(void)
{

  /* Update the serial number string descriptor with the data from the unique
  ID*/
  /* 使用唯一 ID 更新序列号字符串描述符 */
  Get_SerialNum();

  pInformation->Current_Configuration = 0;
  /* Connect the device */
  /* 连接设备 */
  PowerOn();
  /* USB interrupts initialization */
  /* USB 中断初始化 */
  _SetISTR(0);               /* clear pending interrupts */
                              /* 清除挂起的中断 */
  wInterrupt_Mask = IMR_MSK;
  _SetCNTR(wInterrupt_Mask); /* set interrupts mask */
                              /* 设置中断屏蔽 */
  bDeviceState = UNCONNECTED;
}

/*******************************************************************************
* Function Name  : UsbHidDev_Reset.
* 函数名          : UsbHidDev_Reset
* Description    : UsbHidDev Mouse reset routine.
* 描述            : USB HID 设备复位例程
* Input          : None.
* 输入            : 无
* Output         : None.
* 输出            : 无
* Return         : None.
* 返回值          : 无
*******************************************************************************/
void UsbHidDev_Reset(void)
{
  /* Set UsbHidDev_DEVICE as not configured */
  /* 将设备设置为未配置状态 */
  pInformation->Current_Configuration = 0;
  pInformation->Current_Interface = 0;/*the default Interface*/
                                      /* 默认接口 */

  /* Current Feature initialization */
  /* 当前特性初始化 */
  pInformation->Current_Feature = UsbHidDev_ConfigDescriptor[7];

  SetBTABLE(BTABLE_ADDRESS);

  /* Initialize Endpoint 0 */
  /* 初始化端点 0 */
  SetEPType(ENDP0, EP_CONTROL);
  SetEPTxStatus(ENDP0, EP_TX_STALL);
  SetEPRxAddr(ENDP0, ENDP0_RXADDR);
  SetEPTxAddr(ENDP0, ENDP0_TXADDR);
  Clear_Status_Out(ENDP0);
  SetEPRxCount(ENDP0, Device_Property.MaxPacketSize);
  SetEPRxValid(ENDP0);

  /* Initialize Endpoint 1 */
  /* 初始化端点 1 */
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
  /* 设置此设备响应默认地址 0 */
  SetDeviceAddress(0);
}
/*******************************************************************************
* Function Name  : UsbHidDev_SetConfiguration.
* 函数名          : UsbHidDev_SetConfiguration
* Description    : Udpade the device state to configured.
* 描述            : 更新设备状态为已配置
* Input          : None.
* 输入            : 无
* Output         : None.
* 输出            : 无
* Return         : None.
* 返回值          : 无
*******************************************************************************/
void UsbHidDev_SetConfiguration(void)
{
  DEVICE_INFO *pInfo = &Device_Info;

  if (pInfo->Current_Configuration != 0)
  {
    /* Device configured */
    /* 设备已配置 */
    bDeviceState = CONFIGURED;
  }
}
/*******************************************************************************
* Function Name  : UsbHidDev_SetDeviceAddress
* 函数名          : UsbHidDev_SetDeviceAddress
* Description    : Udpade the device state to addressed.
* 描述            : 更新设备状态为已编址
* Input          : None.
* 输入            : 无
* Output         : None.
* 输出            : 无
* Return         : None.
* 返回值          : 无
*******************************************************************************/
void UsbHidDev_SetDeviceAddress (void)
{
  bDeviceState = ADDRESSED;
}
/*******************************************************************************
* Function Name  : UsbHidDev_Status_In.
* 函数名          : UsbHidDev_Status_In
* Description    : UsbHidDev status IN routine.
* 描述            : USB HID 设备状态输入例程
* Input          : None.
* 输入            : 无
* Output         : None.
* 输出            : 无
* Return         : None.
* 返回值          : 无
*******************************************************************************/
void UsbHidDev_Status_In(void)
{
  /*
   * SET_REPORT is a Control Write transaction: host sends data in Data Stage,
   * device replies with zero-length Status IN.
   * The data was copied into g_hidReportBuf by the CopyRoutine callback earlier
   * (UsbHidDev_SetReportData). Now is the right time to deliver it to the
   * STK500 protocol parser.
   *
   * SET_REPORT 是控制写事务：主机在数据阶段发送数据，设备回复零长度状态IN。
   * 数据已在 CopyRoutine 回调中复制到 g_hidReportBuf，现在交付给 STK 解析器。
   */
  if (g_hidPendingSetReport != 0U)
  {
    HID_Rx_Store(g_hidPendingSetReportId, g_hidReportBuf, g_hidReportLen);
    g_hidPendingSetReport = 0U;
    g_hidPendingSetReportId = 0U;
    g_hidReportLen = 0U;
    HID_ResetRequestState();
  }

  if (g_cdcPendingSetLineCoding != 0U)
  {
    CDC_SetLineCodingFromBuffer();
    g_cdcPendingSetLineCoding = 0U;
  }
}

/*******************************************************************************
* Function Name  : UsbHidDev_Status_Out
* 函数名          : UsbHidDev_Status_Out
* Description    : UsbHidDev status OUT routine.
* 描述            : USB HID 设备状态输出例程
* Input          : None.
* 输入            : 无
* Output         : None.
* 输出            : 无
* Return         : None.
* 返回值          : 无
*******************************************************************************/
void UsbHidDev_Status_Out (void)
{
}

/*******************************************************************************
* Function Name  : UsbHidDev_Data_Setup
* 函数名          : UsbHidDev_Data_Setup
* Description    : Handle the data class specific requests.
* 描述            : 处理数据类特定请求（GET_DESCRIPTOR、GET_REPORT 等）
* Input          : Request Nb.
* 输入            : RequestNo = 请求号
* Output         : None.
* 输出            : 无
* Return         : USB_UNSUPPORT or USB_SUCCESS.
* 返回值          : USB_UNSUPPORT（不支持）或 USB_SUCCESS（成功）
*******************************************************************************/
RESULT UsbHidDev_Data_Setup(u8 RequestNo)
{
  u8 *(*CopyRoutine)(u16);

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
    /* GET_DESCRIPTOR 结束 */

  /*** GET_PROTOCOL ***/
  /*** 获取协议 ***/
  else if ((Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT))
           && RequestNo == GET_PROTOCOL)
  {
    CopyRoutine = UsbHidDev_GetProtocolValue;
  }

  /*** GET_REPORT: Host reads data from device ***/
  /*** 获取报告：主机从设备读取数据 ***/
  else if ((Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT))
           && RequestNo == GET_REPORT)
  {
    u16 outLen;
    u8 *buf;
    HID_BeginReportRequest((u8)pInformation->USBwValue0, REQUEST_TYPE_HID_FIRST);
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
  /*** 设置报告：主机向设备写入数据 ***/
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
* 函数名          : UsbHidDev_NoData_Setup
* Description    : handle the no data class specific requests
* 描述            : 处理无数据类特定请求（SET_PROTOCOL 等）
* Input          : Request Nb.
* 输入            : RequestNo = 请求号
* Output         : None.
* 输出            : 无
* Return         : USB_UNSUPPORT or USB_SUCCESS.
* 返回值          : USB_UNSUPPORT（不支持）或 USB_SUCCESS（成功）
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
* 函数名          : UsbHidDev_GetDeviceDescriptor
* Description    : Gets the device descriptor.
* 描述            : 获取设备描述符
* Input          : Length
* 输入            : Length = 请求长度
* Output         : None.
* 输出            : 无
* Return         : The address of the device descriptor.
* 返回值          : 设备描述符缓冲区地址
*******************************************************************************/
u8 *UsbHidDev_GetDeviceDescriptor(u16 Length)
{
  return Standard_GetDescriptorData(Length, &Device_Descriptor);
}

/*******************************************************************************
* Function Name  : UsbHidDev_GetConfigDescriptor.
* 函数名          : UsbHidDev_GetConfigDescriptor
* Description    : Gets the configuration descriptor.
* 描述            : 获取配置描述符
* Input          : Length
* 输入            : Length = 请求长度
* Output         : None.
* 输出            : 无
* Return         : The address of the configuration descriptor.
* 返回值          : 配置描述符缓冲区地址
*******************************************************************************/
u8 *UsbHidDev_GetConfigDescriptor(u16 Length)
{
  return Standard_GetDescriptorData(Length, &Config_Descriptor);
}

/*******************************************************************************
* Function Name  : UsbHidDev_GetStringDescriptor
* 函数名          : UsbHidDev_GetStringDescriptor
* Description    : Gets the string descriptors according to the needed index
* 描述            : 根据索引获取字符串描述符
* Input          : Length
* 输入            : Length = 请求长度
* Output         : None.
* 输出            : 无
* Return         : The address of the string descriptors.
* 返回值          : 字符串描述符缓冲区地址
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
* 函数名          : UsbHidDev_GetReportDescriptor
* Description    : Gets the HID report descriptor.
* 描述            : 获取 HID 报告描述符
* Input          : Length
* 输入            : Length = 请求长度
* Output         : None.
* 输出            : 无
* Return         : The address of the configuration descriptor.
* 返回值          : HID 报告描述符缓冲区地址
*******************************************************************************/
u8 *UsbHidDev_GetReportDescriptor(u16 Length)
{
  return Standard_GetDescriptorData(Length, &UsbHidDev_Report_Descriptor);
}

/*******************************************************************************
* Function Name  : UsbHidDev_GetHIDDescriptor.
* 函数名          : UsbHidDev_GetHIDDescriptor
* Description    : Gets the HID descriptor.
* 描述            : 获取 HID 描述符
* Input          : Length
* 输入            : Length = 请求长度
* Output         : None.
* 输出            : 无
* Return         : The address of the configuration descriptor.
* 返回值          : HID 描述符缓冲区地址
*******************************************************************************/
u8 *UsbHidDev_GetHIDDescriptor(u16 Length)
{
  return Standard_GetDescriptorData(Length, &Mouse_Hid_Descriptor);
}

/*******************************************************************************
* Function Name  : UsbHidDev_Get_Interface_Setting.
* 函数名          : UsbHidDev_Get_Interface_Setting
* Description    : tests the interface and the alternate setting according to the
*                  supported one.
* 描述            : 测试接口和备用设置是否与支持的匹配
* Input          : - Interface : interface number.
*                  - AlternateSetting : Alternate Setting number.
* 输入            : Interface = 接口号，AlternateSetting = 备用设置号
* Output         : None.
* 输出            : 无
* Return         : USB_SUCCESS or USB_UNSUPPORT.
* 返回值          : USB_SUCCESS（成功）或 USB_UNSUPPORT（不支持）
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
* 函数名          : UsbHidDev_SetProtocol
* Description    : UsbHidDev Set Protocol request routine.
* 描述            : USB HID 设置协议请求例程
* Input          : None.
* 输入            : 无
* Output         : None.
* 输出            : 无
* Return         : USB SUCCESS.
* 返回值          : USB_SUCCESS（成功）
*******************************************************************************/
RESULT UsbHidDev_SetProtocol(void)
{
  u8 wValue0 = pInformation->USBwValue0;
  ProtocolValue = wValue0;
  return USB_SUCCESS;
}

/*******************************************************************************
* Function Name  : UsbHidDev_GetProtocolValue
* 函数名          : UsbHidDev_GetProtocolValue
* Description    : get the protocol value
* 描述            : 获取协议值
* Input          : Length.
* 输入            : Length = 请求长度
* Output         : None.
* 输出            : 无
* Return         : address of the protcol value.
* 返回值          : 协议值的地址
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

