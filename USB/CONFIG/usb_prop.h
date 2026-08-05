/*
 * USB属性处理头文件 - 属性请求处理接口
 */

/******************** (C) COPYRIGHT 2008 STMicroelectronics ********************
* File Name          : usb_prop.h
* Author             : MCD Application Team
* Version            : V2.2.0
* Date               : 06/13/2008
* Description        : All processings related to UsbHidDev Mouse demo
********************************************************************************
* THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
* WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE TIME.
* AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
* INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
* CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
* INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
*******************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USB_PROP_H
#define __USB_PROP_H

/* Includes ------------------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/
typedef enum _HID_REQUESTS
{
  GET_REPORT = 1,
  GET_IDLE,
  GET_PROTOCOL,

  SET_REPORT = 9,
  SET_IDLE,
  SET_PROTOCOL
} HID_REQUESTS;

#define CDC_SET_LINE_CODING             0x20
#define CDC_GET_LINE_CODING             0x21
#define CDC_SET_CONTROL_LINE_STATE      0x22
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
void UsbHidDev_init(void);
void UsbHidDev_Reset(void);
void UsbHidDev_SetConfiguration(void);
void UsbHidDev_SetDeviceAddress (void);
void UsbHidDev_Status_In (void);
void UsbHidDev_Status_Out (void);
RESULT UsbHidDev_Data_Setup(u8);
RESULT UsbHidDev_NoData_Setup(u8);
RESULT UsbHidDev_Get_Interface_Setting(u8 Interface, u8 AlternateSetting);
u8 *UsbHidDev_GetDeviceDescriptor(u16 );
u8 *UsbHidDev_GetConfigDescriptor(u16);
u8 *UsbHidDev_GetStringDescriptor(u16);
u8 *UsbHidDev_GetBOSDescriptor(u16 Length);
RESULT UsbHidDev_SetProtocol(void);
u8 *UsbHidDev_GetProtocolValue(u16 Length);
RESULT UsbHidDev_SetProtocol(void);
u8 *UsbHidDev_GetReportDescriptor(u16 Length);
u8 *UsbHidDev_GetHIDDescriptor(u16 Length);
u8 *UsbHidDev_SetReportData(u16 Length);

/* Exported define -----------------------------------------------------------*/
#define UsbHidDev_GetConfiguration          NOP_Process
//#define UsbHidDev_SetConfiguration          NOP_Process
#define UsbHidDev_GetInterface              NOP_Process
#define UsbHidDev_SetInterface              NOP_Process
#define UsbHidDev_GetStatus                 NOP_Process
#define UsbHidDev_ClearFeature              NOP_Process
#define UsbHidDev_SetEndPointFeature        NOP_Process
#define UsbHidDev_SetDeviceFeature          NOP_Process
//#define UsbHidDev_SetDeviceAddress          NOP_Process

#define REPORT_DESCRIPTOR                  0x22

#endif /* __USB_PROP_H */

/******************* (C) COPYRIGHT 2008 STMicroelectronics *****END OF FILE****/

