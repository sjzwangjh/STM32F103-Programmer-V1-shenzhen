/*
 * 串口驱动头文件 - USART1初始化/发送/接收/中断处理（含环形缓冲区接口）
 */

#ifndef __USART_H
#define __USART_H
#include "sys.h"
#include "stdio.h"	 

/* 定义printf执行方式 */
# define    PRINTF_USE_BUFF     1   // 打印函数调用缓冲区发送 0=实时发送；1=缓冲区发送

/* ========== 旧接口（向后兼容） ========== */
#define USART_REC_LEN  			200  	//定义最大接收字节数 200
#define EN_USART1_RX 			1		//使能（1）/禁止（0）串口1接收
	  	
extern u8  USART_RX_BUF[USART_REC_LEN]; //接收缓冲,最大USART_REC_LEN个字节.末字节为换行符 
extern u16 USART_RX_STA;         		//接收状态标记	

/* ========== 新接口：环形缓冲区（256字节） ========== */
#define UART1_RB_SIZE         256          // 环形缓冲区大小（必须是2的幂）
#define UART1_RB_MASK         (UART1_RB_SIZE - 1)

// 写环形缓冲区（发送），如果缓冲区满则等待
void uart1_WriteByte(u8 data);
void uart1_WriteString(const char *str);

// 读环形缓冲区（接收），返回1表示成功读到数据，0表示缓冲区空
u8   uart1_ReadByte(u8 *data);

// 从接收环形缓冲区提取一条完整命令（以\r\n结尾），
// buf：输出缓冲区，bufSize：输出缓冲区大小
// 返回值：命令字符串长度（不含\r\n和结尾\0），若无完整命令则返回0
u16  uart1_ReadLine(u8 *buf, u16 bufSize);

// 查询接收环形缓冲区中可用字节数
u16  uart1_RxAvailable(void);

// 查询发送环形缓冲区中剩余空间字节数
u16  uart1_TxFree(void);

// 串口初始化
void uart_init(u32 pclk2,u32 bound);

#endif


