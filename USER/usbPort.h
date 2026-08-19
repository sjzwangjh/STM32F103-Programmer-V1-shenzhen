#ifndef __USB_PORT_H_INCLUDED__
#define __USB_PORT_H_INCLUDED__

#include <stdint.h>

/*
 * USB D+ 连接/断开控制，Boot 与 App 两个工程共用同一份声明
 * （本文件在两个工程中各存一份，修改时需同步）。
 * 实现分别放在各工程 main.c（硬件相关，使用各自 Hardware_Config 引脚定义）。
 *
 * enable = 0：断开 USB（D+ 拉低 + 收发器掉电），用于跳转/复位前；
 * enable = 1：使能 USB 枚举。
 */
void usb_port_set(uint8_t enable);

#endif /* __USB_PORT_H_INCLUDED__ */
