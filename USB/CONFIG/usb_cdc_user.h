/*
 * USB CDC用户层接口 - CDC ACM虚拟串口收发
 */

#ifndef __USB_CDC_USER_H__
#define __USB_CDC_USER_H__

#include <stdint.h>
#include <stddef.h>

#define CDC_RX_PACKET_SIZE      64U
#define CDC_TX_PACKET_SIZE      64U
#define CDC_STK_FRAME_SIZE      281U
#define CDC_RX_RING_SIZE        512U

typedef struct
{
    uint32_t bitrate;
    uint8_t  format;
    uint8_t  paritytype;
    uint8_t  datatype;
} usb_cdc_line_coding_t;

void CDC_Init(void);
void CDC_Task(void);
void CDC_DataOut_Callback(void);
void CDC_DataIn_Callback(void);
uint8_t CDC_SendData(const uint8_t *data, uint16_t len);
uint8_t CDC_IsTxBusy(void);
uint8_t *CDC_GetLineCodingBuffer(void);
void CDC_SetLineCodingFromBuffer(void);
void CDC_FillLineCodingBuffer(void);
void CDC_SetControlLineState(uint16_t state);
uint16_t CDC_GetControlLineState(void);

#endif





