/*
 * USB配置头文件 - USB端点/缓冲区/传输配置
 */

/******************** (C) COPYRIGHT 2008 STMicroelectronics ********************
* File Name          : usb_conf.h
* Author             : MCD Application Team
* Version            : V2.2.0
* Date               : 06/13/2008
* Description        : UsbHidDev Mouse demo configuration file
********************************************************************************
* THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
* WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE TIME.
* AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
* INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
* CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
* INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
*******************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USB_CONF_H
#define __USB_CONF_H

/* Includes ------------------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
/* External variables --------------------------------------------------------*/
/*-------------------------------------------------------------*/
/* EP_NUM */
/* defines how many endpoints are used by the device */
/*-------------------------------------------------------------*/
#define EP_NUM     (5)

/*-------------------------------------------------------------*/
/* --------------   Buffer Description Table  -----------------*/
/*-------------------------------------------------------------*/
/* buffer table base address */
/* buffer table base address */
#define BTABLE_ADDRESS      (0x00)

/* PMA layout optimized for 6 endpoints in 512 bytes (504B used):
 *   BTABLE:   0x00-0x2F  (48B)
 *   EP0 RX:   0x30-0x4F  (32B)  EP0 TX:   0x50-0x6F  (32B)
 *   EP1 TX:   0x70-0xAF  (64B)  EP1 RX:   0xB0-0xEF  (64B)
 *   EP2 TX:   0xF0-0xF7  (8B)
 *   EP3 TX:   0xF8-0x137 (64B)  EP3 RX:   0x138-0x177 (64B)
 *   EP4 TX:   0x178-0x1B7 (64B) EP4 RX:   0x1B8-0x1F7 (64B)
 * Total: 504 bytes (fits in 512-byte PMA), all addresses 4-byte aligned
 */

/* EP0: Control (32-byte MPS) */
#define ENDP0_RXADDR        (0x30)
#define ENDP0_TXADDR        (0x50)

/* EP1: HID Interrupt IN/OUT (64 bytes each) */
#define ENDP1_TXADDR        (0x70)
#define ENDP1_RXADDR        (0xB0)

/* EP2: CDC Notification IN (8 bytes) */
#define ENDP2_TXADDR        (0xF0)

/* EP3: CDC Data IN/OUT */
#define ENDP3_TXADDR        (0xF8)
#define ENDP3_RXADDR        (0x138)

/* EP4: WinUSB Bulk IN/OUT */
#define ENDP4_TXADDR        (0x178)
#define ENDP4_RXADDR        (0x1B8)


/*-------------------------------------------------------------*/
/* -------------------   ISTR events  -------------------------*/
/*-------------------------------------------------------------*/
/* IMR_MSK */
/* mask defining which events has to be handled */
/* by the device application software */
#define IMR_MSK (CNTR_CTRM  | CNTR_WKUPM | CNTR_SUSPM | CNTR_ERRM  | CNTR_SOFM \
                 | CNTR_ESOFM | CNTR_RESETM )

/* CTR service routines */
/* associated to defined endpoints */
#define  EP1_IN_Callback   HID_EP1_IN_Callback
#define  EP2_IN_Callback   NOP_Process
#define  EP3_IN_Callback   CDC_DataIn_Callback
#define  EP4_IN_Callback   WinUSB_IN_Callback
#define  EP5_IN_Callback   NOP_Process
#define  EP6_IN_Callback   NOP_Process
#define  EP7_IN_Callback   NOP_Process

#define  EP1_OUT_Callback  HID_EP1_OUT_Callback
#define  EP2_OUT_Callback   NOP_Process
#define  EP3_OUT_Callback   CDC_DataOut_Callback
#define  EP4_OUT_Callback  WinUSB_OUT_Callback
#define  EP5_OUT_Callback   NOP_Process
#define  EP6_OUT_Callback   NOP_Process
#define  EP7_OUT_Callback   NOP_Process

#endif /*__USB_CONF_H*/

/******************* (C) COPYRIGHT 2008 STMicroelectronics *****END OF FILE****/


