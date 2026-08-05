#include "usart.h"	  
#include <string.h>

//如果使用ucos,则包括下面的头文件即可.
#if SYSTEM_SUPPORT_UCOS
#include "includes.h"					//ucos 使用	  
#endif

/* ========== 旧接收接口（向后兼容） ========== */
#if EN_USART1_RX
u8 USART_RX_BUF[USART_REC_LEN];
u16 USART_RX_STA=0;
#endif

/* ========== 环形缓冲区 ========== */

/* ---- 接收环形缓冲区 ---- */
static u8      rxRingBuf[UART1_RB_SIZE];
static volatile u16 rxRingWr;       // 写入索引（中断中修改）
static volatile u16 rxRingRd;       // 读取索引（主循环中修改）

/* ---- 发送环形缓冲区 ---- */
static u8      txRingBuf[UART1_RB_SIZE];
static volatile u16 txRingWr;       // 写入索引（主循环中修改）
static volatile u16 txRingRd;       // 读取索引（中断中修改）

/* ========== 工具：判断环形缓冲区空/满 ========== */
#define RING_EMPTY(wr, rd)  ((wr) == (rd))
#define RING_FULL(wr, rd)   (((wr) + 1) & UART1_RB_MASK) == (rd)
#define RING_COUNT(wr, rd)  ((u16)(((wr) - (rd)) & UART1_RB_MASK))
#define RING_FREE(wr, rd)   ((u16)(((rd) - (wr) - 1) & UART1_RB_MASK))



/* ========== printf 重定向支持 ========== */
#if 1
#pragma import(__use_no_semihosting)             
struct __FILE 
{ 
	int handle; 
}; 
FILE __stdout;       
void _sys_exit(int x) 
{ 
	x = x; 
} 

int fputc(int ch, FILE *f)
{      
#if PRINTF_USE_BUFF==0
	while((USART1->SR&0X40)==0);
	USART1->DR = (u8) ch;      
#else
    /* 把数据压入发送环形缓冲区（不等待，直接用 if 判断）*/
    u16 wr, rd;
    
    __disable_irq();
    wr = txRingWr;
    rd = txRingRd;
    
    if (!RING_FULL(wr, rd))
    {
        txRingBuf[wr] = (u8)ch;
        txRingWr = (wr + 1) & UART1_RB_MASK;
        __enable_irq();
        USART1->CR1 |= (1 << 7);   // TXEIE = 1
    }
    else
    {
        __enable_irq();
        /* 缓冲区满时，退化为轮询发送（不会死锁）*/
        while((USART1->SR & 0x40) == 0);
        USART1->DR = (u8)ch;
    }
#endif
	return ch;
}
#endif 

/* ========== 公开接口实现 ========== */
/* ---- 接收接口 ---- */
u16 uart1_RxAvailable(void)
{
    u16 rd, wr;
    __disable_irq();
    rd = rxRingRd;
    wr = rxRingWr;
    __enable_irq();
    return RING_COUNT(wr, rd);
}

u8 uart1_ReadByte(u8 *data)
{
    u16 rd, wr;
    __disable_irq();
    rd = rxRingRd;
    wr = rxRingWr;
    if (RING_EMPTY(wr, rd))
    {
        __enable_irq();
        return 0;
    }
    *data = rxRingBuf[rd];
    rxRingRd = (rd + 1) & UART1_RB_MASK;
    __enable_irq();
    return 1;
}

u16 uart1_ReadLine(u8 *buf, u16 bufSize)
{
    u16 rd, wr;
    u16 len = 0;
    u16 idx;
    u8  found = 0;

    if (bufSize == 0) return 0;

    __disable_irq();
    rd = rxRingRd;
    wr = rxRingWr;

    /* 在环形缓冲区中查找 \r\n 序列 */
    idx = rd;
    while (idx != wr)
    {
        if (rxRingBuf[idx] == '\r')
        {
            u16 next = (idx + 1) & UART1_RB_MASK;
            if (next != wr && rxRingBuf[next] == '\n')
            {
                found = 1;
                break;
            }
        }
        idx = (idx + 1) & UART1_RB_MASK;
    }

    if (!found)
    {
        __enable_irq();
        return 0;
    }

    /* 计算命令长度（不含 \r\n） */
    len = (idx >= rd) ? (idx - rd) : (UART1_RB_SIZE - rd + idx);
    if (len >= bufSize) len = bufSize - 1;

    /* 拷贝命令内容到输出缓冲区 */
    for (idx = 0; idx < len; idx++)
    {
        buf[idx] = rxRingBuf[rd];
        rd = (rd + 1) & UART1_RB_MASK;
    }
    buf[len] = '\0';

    /* 跳过 \r\n */
    rd = (rd + 2) & UART1_RB_MASK;
    rxRingRd = rd;

    __enable_irq();
    return len;
}

/* ---- 发送接口 ---- */

u16 uart1_TxFree(void)
{
    u16 rd, wr;
    __disable_irq();
    rd = txRingRd;
    wr = txRingWr;
    __enable_irq();
    return RING_FREE(wr, rd);
}

void uart1_WriteByte(u8 data)
{
    u16 wr, rd;

    /*
     * 先关中断检查+写入环形缓冲区。
     * 如果缓冲区满则退化为直接轮询硬件发送——保证永不阻塞/死锁。
     */
    __disable_irq();
    wr = txRingWr;
    rd = txRingRd;

    if (!RING_FULL(wr, rd))
    {
        /* 环形缓冲区有空位：写入缓冲区，由 ISR 异步发送 */
        txRingBuf[wr] = data;
        txRingWr = (wr + 1) & UART1_RB_MASK;
        __enable_irq();
        USART1->CR1 |= (1 << 7);   /* TXEIE = 1，触发中断发送 */
    }
    else
    {
        /* 环形缓冲区满：退化到轮询硬件发送（永不阻塞） */
        u32 pollTimeout = 0x0000FFFFU;
        __enable_irq();
        while ((USART1->SR & 0x40) == 0)
        {
            if (pollTimeout-- == 0U)
            {
                break;   /* drop the byte instead of hanging forever */
            }
        }
        if (pollTimeout != 0U)
        {
            USART1->DR = (u8)data;
        }
    }
}

void uart1_WriteString(const char *str)
{
    while (*str)
    {
        uart1_WriteByte((u8)*str);
        str++;
    }
}

/* ========== 串口1中断服务程序 ========== */

void USART1_IRQHandler(void)
{
    u8 res;
    u16 wr, rd;

#ifdef OS_CRITICAL_METHOD
    OSIntEnter();
#endif

    /* ---- 接收中断（RXNE） ---- */
    if (USART1->SR & (1 << 5))
    {
        res = (u8)USART1->DR;

        /* --- 旧接口（向后兼容） --- */
        if ((USART_RX_STA & 0x8000) == 0)
        {
            if (USART_RX_STA & 0x4000)
            {
                if (res != 0x0a) USART_RX_STA = 0;
                else USART_RX_STA |= 0x8000;
            }
            else
            {
                if (res == 0x0d) USART_RX_STA |= 0x4000;
                else
                {
                    USART_RX_BUF[USART_RX_STA & 0x3FFF] = res;
                    USART_RX_STA++;
                    if (USART_RX_STA > (USART_REC_LEN - 1)) USART_RX_STA = 0;
                }
            }
        }

        /* --- 新接口：压入接收环形缓冲区（满则覆盖最旧数据） --- */
        wr = rxRingWr;
        rd = rxRingRd;

        rxRingBuf[wr] = res;
        wr = (wr + 1) & UART1_RB_MASK;

        /* 如果缓冲区满，丢弃最旧的数据（覆盖） */
        if (wr == rd)
        {
            rd = (rd + 1) & UART1_RB_MASK;  // 丢弃最早的一个字节
        }

        rxRingWr = wr;
        rxRingRd = rd;
    }

    /* ---- 发送中断（TXE） ---- */
    if (USART1->SR & (1 << 7))
    {
        wr = txRingWr;
        rd = txRingRd;

        if (!RING_EMPTY(wr, rd))
        {
            USART1->DR = txRingBuf[rd];
            txRingRd = (rd + 1) & UART1_RB_MASK;
        }
        else
        {
            /* 没有数据要发送，关闭发送中断 */
            USART1->CR1 &= ~(1 << 7);  // TXEIE = 0
        }
    }

#ifdef OS_CRITICAL_METHOD
    OSIntExit();
#endif
}

/* ========== 串口1初始化 ========== */

void uart_init(u32 pclk2,u32 bound)
{  	 
	float temp;
	u16 mantissa;
	u16 fraction;	   
	temp=(float)(pclk2*1000000)/(bound*16);
	mantissa=temp;
	fraction=(temp-mantissa)*16; 
    mantissa<<=4;
	mantissa+=fraction; 
	RCC->APB2ENR|=1<<2;   //使能PORTA口时钟  
	RCC->APB2ENR|=1<<14;  //使能串口时钟 
	GPIOA->CRH&=0XFFFFF00F;
	GPIOA->CRH|=0X000008B0;
		  
	RCC->APB2RSTR|=1<<14;   //复位串口1
	RCC->APB2RSTR&=~(1<<14);
	//波特率设置
 	USART1->BRR=mantissa; 
	USART1->CR1|=0X200C;  //1位停止,无校验位.

    /* 清空环形缓冲区索引 */
    rxRingWr = 0;
    rxRingRd = 0;
    txRingWr = 0;
    txRingRd = 0;

#if EN_USART1_RX
	//使能接收中断
	USART1->CR1|=1<<8;    //PE中断使能
	USART1->CR1|=1<<5;    //RXNEIE（接收缓冲区非空中断使能）
	MY_NVIC_Init(3,3,USART1_IRQn,2);//组2，最低优先级 
#endif
}



