#include "sys.h"
#include "Hardware_Config.h"
//系统时钟初始化

//设置向量表偏移地址
//NVIC_VectTab:基址
//Offset:偏移量			 
void AppEarlyMark(void)
{
    u32 timeout;

    if ((RCC->APB2ENR & RCC_APB2ENR_USART1EN) == 0U) {
        return;
    }
    if ((USART1->CR1 & USART_CR1_UE) == 0U) {
        return;
    }

    timeout = 0x0003FFFFU;
    while ((USART1->SR & USART_SR_TXE) == 0U) {
        if (timeout-- == 0U) {
            return;
        }
    }

    USART1->DR = (u16)'A';

    timeout = 0x0003FFFFU;
    while ((USART1->SR & USART_SR_TC) == 0U) {
        if (timeout-- == 0U) {
            return;
        }
    }
}

void AppEarlyBeep(void)
{
    volatile u32 delayCount;

    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    GPIOB->CRH &= ~(0xFUL << 4);
    GPIOB->CRH |=  (0x3UL << 4);

    GPIOB->BSRR = (1U << 9);
    for (delayCount = 0U; delayCount < 180000U; ++delayCount) {
        __NOP();
    }
    GPIOB->BRR = (1U << 9);
}

void AppProgrammer_EarlyTrace(void)
{
    *(volatile u32 *)APP_TRACE_MARK_ADDR = APP_TRACE_MARK_APPS;
    AppEarlyBeep();
}

void MY_NVIC_SetVectorTable(u32 NVIC_VectTab, u32 Offset)	 
{ 	   	 
	SCB->VTOR = NVIC_VectTab|(Offset & (u32)0x1FFFFF80);//设置NVIC的向量表偏移寄存器
	//用于标识向量表是在CODE区还是在RAM区
}
//设置NVIC分组
//NVIC_Group:NVIC分组 0~4 总共5组 		   
void MY_NVIC_PriorityGroupConfig(u8 NVIC_Group)	 
{ 
	u32 temp,temp1;	  
	temp1=(~NVIC_Group)&0x07;//取后三位
	temp1<<=8;
	temp=SCB->AIRCR;  //读取先前的设置
	temp&=0X0000F8FF; //清空先前分组
	temp|=0X05FA0000; //写入钥匙
	temp|=temp1;	   
	SCB->AIRCR=temp;  //设置分组	    	  				   
}
//设置NVIC 
//NVIC_PreemptionPriority:抢占优先级
//NVIC_SubPriority       :响应优先级
//NVIC_Channel           :中断编号
//NVIC_Group             :中断分组 0~4
//注意优先级不能超过设定的组的范围!否则会有意想不到的错误
//组划分:
//组0:0位抢占优先级,4位响应优先级
//组1:1位抢占优先级,3位响应优先级
//组2:2位抢占优先级,2位响应优先级
//组3:3位抢占优先级,1位响应优先级
//组4:4位抢占优先级,0位响应优先级
//NVIC_SubPriority和NVIC_PreemptionPriority的原则是,数值越小,越优先	   
void MY_NVIC_Init(u8 NVIC_PreemptionPriority,u8 NVIC_SubPriority,u8 NVIC_Channel,u8 NVIC_Group)	 
{ 
	u32 temp;	
	MY_NVIC_PriorityGroupConfig(NVIC_Group);//设置分组
	temp=NVIC_PreemptionPriority<<(4-NVIC_Group);	  
	temp|=NVIC_SubPriority&(0x0f>>NVIC_Group);
	temp&=0xf;//取低四位  
	if(NVIC_Channel<32)NVIC->ISER[0]|=1<<NVIC_Channel;//使能中断位(要清除的话,相反操作就OK)
	else NVIC->ISER[1]|=1<<(NVIC_Channel-32);    
	NVIC->IP[NVIC_Channel]|=temp<<4;//设置响应优先级和抢断优先级   	    	  				   
}

//外部中断配置函数
//只针对GPIOA~G;不包括PVD,RTC和USB唤醒这三个
//参数:
//GPIOx:0~6,代表GPIOA~G
//BITx:需要使能的位;
//TRIM:触发模式,1,下升沿;2,上降沿;3，任意电平触发
//该函数一次只能配置1个IO口,多个IO口,需多次调用
//该函数会自动开启对应中断,以及屏蔽线   	    
void Ex_NVIC_Config(u8 GPIOx,u8 BITx,u8 TRIM) 
{
	u8 EXTADDR;
	u8 EXTOFFSET;
	EXTADDR=BITx/4;//得到中断寄存器组的编号
	EXTOFFSET=(BITx%4)*4; 
	RCC->APB2ENR|=0x01;//使能io复用时钟			 
	AFIO->EXTICR[EXTADDR]&=~(0x000F<<EXTOFFSET);//清除原来设置！！！
	AFIO->EXTICR[EXTADDR]|=GPIOx<<EXTOFFSET;//EXTI.BITx映射到GPIOx.BITx 
	//自动设置
	EXTI->IMR|=1<<BITx;//  开启line BITx上的中断
	//EXTI->EMR|=1<<BITx;//不屏蔽line BITx上的事件 (如果不屏蔽这句,在硬件上是可以的,但是在软件仿真的时候无法进入中断!)
 	if(TRIM&0x01)EXTI->FTSR|=1<<BITx;//line BITx上事件下降沿触发
	if(TRIM&0x02)EXTI->RTSR|=1<<BITx;//line BITx上事件上升降沿触发
} 	  
//不能在这里执行所有外设复位!否则至少引起串口不工作.		    
void MYRCC_DeInit(void)
{
    /* Keep the clock-tree rebuild, but avoid touching SysTick/NVIC here.
     * After a bootloader handoff these accesses can destabilize startup
     * before the app reaches uart_init()/main diagnostics. */
    __disable_irq();

    RCC->APB1RSTR = 0x00000000; // 复位结束
    RCC->APB2RSTR = 0x00000000;

    RCC->AHBENR = 0x00000014;   // 睡眠模式闪存和SRAM时钟使能, 其他关闭
    RCC->APB2ENR = 0x00000000;  // 外设时钟关闭
    RCC->APB1ENR = 0x00000000;

    RCC->CR |= RCC_CR_HSION;
    while((RCC->CR & RCC_CR_HSIRDY) == 0);

    RCC->CFGR &= 0xF8FF0000;    // SW/HPRE/PPRE/ADCPRE/MCO复位
    while((RCC->CFGR & RCC_CFGR_SWS) != 0);

    RCC->CR &= 0xFEF6FFFF;      // 复位HSEON,CSSON,PLLON
    RCC->CR &= 0xFFFBFFFF;      // 复位HSEBYP
    RCC->CFGR &= 0xFF80FFFF;    // 复位PLLSRC, PLLXTPRE, PLLMUL and USBPRE
    RCC->CIR = 0x00000000;      // 关闭所有RCC中断

#ifdef  VECT_TAB_RAM
    MY_NVIC_SetVectorTable(0x20000000, 0x0);
#else
    MY_NVIC_SetVectorTable(APP_START_ADDER, 0x0);
#endif
}
//THUMB指令不支持汇编内联
//采用如下方法实现执行汇编指令WFI  
__asm void WFI_SET(void)
{
	WFI;		  
}
//关闭所有中断
__asm void INTX_DISABLE(void)
{
	CPSID I;		  
}
//开启所有中断
__asm void INTX_ENABLE(void)
{
	CPSIE I;		  
}
//设置栈顶地址
//addr:栈顶地址
__asm void MSR_MSP(u32 addr) 
{
    MSR MSP, r0 			//set Main Stack value
    BX r14
}

//进入待机模式	  
void Sys_Standby(void)
{
	SCB->SCR|=1<<2;//使能SLEEPDEEP位 (SYS->CTRL)	   
  	RCC->APB1ENR|=1<<28;     //使能电源时钟	    
 	PWR->CSR|=1<<8;          //设置WKUP用于唤醒
	PWR->CR|=1<<2;           //清除Wake-up 标志
	PWR->CR|=1<<1;           //PDDS置位		  
	WFI_SET();				 //执行WFI指令		 
}	     
//系统软复位   
void Sys_Soft_Reset(void)
{   
	/*
	 * 标准 NVIC_SystemReset 流程：
	 *   1) 先执行数据同步屏障，确保之前所有内存/外设访问完成；
	 *   2) 写入 SYSRESETREQ 到 AIRCR；
	 *   3) 再次执行数据同步屏障，确保复位请求送达系统总线；
	 *   4) 死循环等待复位实际发生。
	 * 缺少这些步骤可能导致复位请求被未完成的 UART 发送中断等延迟或阻止。
	 */
	__DSB();
	SCB->AIRCR = 0x05FA0000 | (u32)0x04;
	__DSB();
	while(1);	  
}

//JTAG模式设置,用于设置JTAG的模式
//mode:jtag,swd模式设置;00,全使能;01,使能SWD;10,全关闭;	   
//#define JTAG_SWD_DISABLE   0X02
//#define SWD_ENABLE         0X01
//#define JTAG_SWD_ENABLE    0X00		  
void Disable_JTAG_Keep_SWD(void)
{
	RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;		// 开启辅助时钟
	AFIO->MAPR &= ~AFIO_MAPR_SWJ_CFG; 		// 清除MAPR的[26:24]
	AFIO->MAPR |=  AFIO_MAPR_SWJ_CFG_1;		// 设置“SW使能-JTAG关闭”模式
} 

//系统时钟初始化函数
//pll:选择的倍频数，从2开始，最大值为16		 
u8 Stm32_Clock_Init(u8 PLL)
{
    unsigned char temp = 0;
    u32 timeout;

    MYRCC_DeInit();             // 复位并配置向量表

    RCC->CR |= RCC_CR_HSEON;
    timeout = 0x000FFFFF;
    while((RCC->CR & RCC_CR_HSERDY) == 0)
    {
        if(timeout-- == 0)
        {
            __enable_irq();
            return 8;
        }
    }

    RCC->CFGR = 0x00000400;     // APB1=DIV2; APB2=DIV1; AHB=DIV1
    PLL -= 2;                   // PLLMUL编码比实际倍频小2
    RCC->CFGR |= PLL << 18;
    RCC->CFGR |= RCC_CFGR_PLLSRC;
    FLASH->ACR |= 0x32;         // Flash 2 wait states + prefetch

    RCC->CR |= RCC_CR_PLLON;
    timeout = 0x000FFFFF;
    while((RCC->CR & RCC_CR_PLLRDY) == 0)
    {
        if(timeout-- == 0)
        {
            __enable_irq();
            return 8;
        }
    }

    RCC->CFGR |= RCC_CFGR_SW_PLL;
    timeout = 0x000FFFFF;
    while(temp != 0x02)
    {
        temp = (u8)((RCC->CFGR >> 2) & 0x03);
        if(timeout-- == 0)
        {
            __enable_irq();
            return 8;
        }
    }

    __enable_irq();
    return (u8)(12U * (PLL + 2U));
}


