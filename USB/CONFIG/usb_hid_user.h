/*
 * USB HID用户层头文件 - HID用户接口函数和结构体定义
 */

#ifndef   __USB_HID_USER_H__
#define   __USB_HID_USER_H__

#include <stdint.h>
#include <stddef.h>

#define HID_REPORT_MAX_LOAD   128

typedef enum {
    REQUEST_TYPE_IDLE           = 0,
    REQUEST_TYPE_HID_FIRST      = 1,
    REQUEST_TYPE_HID_SUBSEQUENT = 2,
    REQUEST_TYPE_HID_DEBUGDATA  = 3
} RequestType_t;

extern uint8_t  g_HidReportId;
extern RequestType_t g_RequestType;

void HID_BeginReportRequest(uint8_t reportId, RequestType_t requestType);
void HID_Rx_Store(uint8_t reportId, const uint8_t *data, uint8_t len);
void HID_Task(void);
uint8_t HID_Rx_IsAvailable(void);
uint8_t HID_Rx_Read(uint8_t *buf, uint8_t maxLen);
uint8_t *HID_GetReport_Buffer(uint8_t reportId, uint16_t requestedLen, uint16_t *pOutLen);
void HID_ResetRequestState(void);

#endif

