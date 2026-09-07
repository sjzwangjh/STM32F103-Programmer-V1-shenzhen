/*
 * USB硬件配置实现 - USB外设初始化与配置
 */

/******************** (C) COPYRIGHT 2008 STMicroelectronics ********************
* File Name          : hw_config.c
* Author             : MCD Application Team
* Version            : V2.2.0
* Date               : 06/13/2008
* Description        : Hardware Configuration & Setup
********************************************************************************
* THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
* WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE TIME.
* AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
* INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
* CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
* INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
*******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "sys.h"
#include "hw_USB_config.h"
#include "usb_lib.h"
#include "usb_desc.h"
#include "platform_config.h"
#include "usb_pwr.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
 
 
/* Extern variables ----------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
			  

//配置USB时钟,USBclk=48Mhz
void Set_USBClock(void)
{
 	RCC->CFGR&=~(1<<22); //USBclk=PLLclk/1.5=48Mhz	    
	RCC->APB1ENR|=1<<23; //USB时钟使能					 
}
 
/*******************************************************************************
* Function Name  : Enter_LowPowerMode.
* Description    : Power-off system clocks and power while entering suspend mode.
* Input          : None.
* Output         : None.
* Return         : None.
*******************************************************************************/
void Enter_LowPowerMode(void)
{
  /* Set the device state to suspend */
  bDeviceState = SUSPENDED;	  
  /* Request to enter STOP mode with regulator in low power mode */
  //PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI);
}			  

/*******************************************************************************
* Function Name  : Leave_LowPowerMode.
* Description    : Restores system clocks and power while exiting suspend mode.
* Input          : None.
* Output         : None.
* Return         : None.
*******************************************************************************/
void Leave_LowPowerMode(void)
{
  DEVICE_INFO *pInfo = &Device_Info;
								   
													   
  /* Set the device state to the correct state */
  if (pInfo->Current_Configuration != 0)
  {
    /* Device configured */
    bDeviceState = CONFIGURED;
  }
  else
  {
    bDeviceState = ATTACHED;
  }
}

//USB中断配置
void USB_Interrupts_Config(void)
{
  
	EXTI->IMR|=1<<18;//  开启线18上的中断
 	EXTI->RTSR|=1<<18;//line 18上事件上升降沿触发	 
	MY_NVIC_Init(1,0,USB_LP_CAN1_RX0_IRQn,2);//组2，优先级次之 
	MY_NVIC_Init(0,0,USBWakeUp_IRQn,2);     //组2，优先级最高	 	 
}				  
/*******************************************************************************
* Function Name  : Get_SerialNum.
* Description    : Create the serial number string descriptor.
* Input          : None.
* Output         : None.
* Return         : None.
*******************************************************************************/
static void UsbUidWordToString(u32 value, u8 char_offset)
{
  static const u8 hex[] = "0123456789ABCDEF";
  u8 nibble;
  u8 index;

  for (nibble = 0; nibble < 8; nibble++)
  {
    index = (u8)(char_offset + nibble);
    UsbHidDev_StringSerial[2 + 2 * index] =
      hex[(value >> (28 - 4 * nibble)) & 0x0FU];
    UsbHidDev_StringSerial[3 + 2 * index] = 0;
  }
}

static void UsbSerialPrefix(void)
{
  static const u8 prefix[] = "DFM-";
  u8 index;

  for (index = 0; index < 4; index++)
  {
    UsbHidDev_StringSerial[2 + 2 * index] = prefix[index];
    UsbHidDev_StringSerial[3 + 2 * index] = 0;
  }
}

void Get_SerialNum(void)
{
  /* Serial is "DFM-" followed by UID0, UID1, UID2; each word is MSB first. */
  UsbSerialPrefix();
  UsbUidWordToString(*((volatile u32 *)0x1FFFF7E8), 4);
  UsbUidWordToString(*((volatile u32 *)0x1FFFF7EC), 12);
  UsbUidWordToString(*((volatile u32 *)0x1FFFF7F0), 20);
}

/******************* (C) COPYRIGHT 2008 STMicroelectronics *****END OF FILE****/

