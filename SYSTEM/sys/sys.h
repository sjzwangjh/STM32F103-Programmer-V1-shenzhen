#ifndef __SYS_H
#define __SYS_H	  
#include <stm32f10x.h>   
//ç³»ç»Ÿæ—¶é’Ÿåˆå§‹åŒ?	   

//-------- ä»¥ä¸‹ä¸ºå®å±•å¼€è¾…åŠ©ï¼Œæ— éœ€ä¿®æ”¹ --------
// ä»?"B,5" ä¸­æå–ç«¯å£å’Œå¼•è„š
// GET_PORT_FROM(HARDWARE_LED0)  â†?B
// GET_PIN_FROM(HARDWARE_LED0)   â†?5
#define GET_PORT_FROM(...)       GET_PORT_FROM_(__VA_ARGS__)
#define GET_PORT_FROM_(x, ...) x
#define GET_PIN_FROM(...)        GET_PIN_FROM_(__VA_ARGS__)
#define GET_PIN_FROM_(x, y, ...) y

// æ‹¼æŽ¥å‡?PBoutã€PEout ç­‰ä½å¸¦æ“ä½œå‡½æ•°å
#define ARM_PORT_OUT(port)      ARM_PORT_OUT_(port)
#define ARM_PORT_OUT_(port)     P##port##out

// è¯»å–ç«¯å£è¾“å…¥å€¼ï¼šARM_PORT_IN(B) â†?PBin
#define ARM_PORT_IN(port)       ARM_PORT_IN_(port)
#define ARM_PORT_IN_(port)      P##port##in

// ç«¯å£ç»“æž„ä½“æŒ‡é’ˆï¼šARM_PORT_GPIO(B) â†?GPIOB
#define ARM_PORT_GPIO(port)     ARM_PORT_GPIO_(port)
#define ARM_PORT_GPIO_(port)    GPIO##port

// CRL/CRH å¯„å­˜å™¨ï¼šARM_PORT_CRL(B) â†?GPIOB->CRL, ARM_PORT_CRH(B) â†?GPIOB->CRH
#define ARM_PORT_CRL(port)      (ARM_PORT_GPIO(port)->CRL)
#define ARM_PORT_CRH(port)      (ARM_PORT_GPIO(port)->CRH)

// RCC æ—¶é’Ÿä½¿èƒ½ï¼šARM_PORT_RCC_CLK(B) ä½¿èƒ½ GPIOB æ—¶é’Ÿ
#define ARM_PORT_RCC_CLK(port)  (RCC->APB2ENR |= (1 << (ARM_PORT_RCC_BIT(port))))
#define ARM_PORT_RCC_BIT(port)  ARM_PORT_RCC_BIT_(port)
#define ARM_PORT_RCC_BIT_(port) ARM_PORT_RCC_BIT_##port
#define ARM_PORT_RCC_BIT_A      2
#define ARM_PORT_RCC_BIT_B      3
#define ARM_PORT_RCC_BIT_C      4
#define ARM_PORT_RCC_BIT_D      5
#define ARM_PORT_RCC_BIT_E      6
#define ARM_PORT_RCC_BIT_F      7
#define ARM_PORT_RCC_BIT_G      8
// æœ€ç»ˆæ‰“å¼€ç«¯å£æ—¶é’Ÿä½¿ç”¨çš„å®å®šä¹‰
#define PORT_RCC_CLK(...)        ARM_PORT_RCC_CLK(GET_PORT_FROM(__VA_ARGS__))

// GPIO æ–¹å‘/æ¨¡å¼è®¾ç½®è¾…åŠ©å®ï¼š
// cfg4bit ä¸?STM32F103 CRL/CRH ä¸­å¯¹åº”å¼•è„šçš„ 4bit é…ç½®å€?
// ä¾‹å¦‚ï¼?
//   0x3 = 50MHz é€šç”¨æŽ¨æŒ½è¾“å‡º
//   0x7 = 50MHz é€šç”¨å¼€æ¼è¾“å‡?
//   0xB = 50MHz å¤ç”¨æŽ¨æŒ½è¾“å‡º
//   0xF = 50MHz å¤ç”¨å¼€æ¼è¾“å‡?
//   0x4 = æµ®ç©ºè¾“å…¥
//   0x8 = ä¸Šæ‹‰/ä¸‹æ‹‰è¾“å…¥
//   0x0 = æ¨¡æ‹Ÿè¾“å…¥
#define ARM_PORT_SET_CFG_(port, pin, cfg4bit) \
    do { \
        if ((pin) < 8U) \
            ARM_PORT_CRL(port) = (ARM_PORT_CRL(port) & ~((u32)0x0FU << (((pin) & 0x07U) << 2))) | ((u32)(cfg4bit) << (((pin) & 0x07U) << 2)); \
        else \
            ARM_PORT_CRH(port) = (ARM_PORT_CRH(port) & ~((u32)0x0FU << (((pin) & 0x07U) << 2))) | ((u32)(cfg4bit) << (((pin) & 0x07U) << 2)); \
    } while (0)

// 50MHz é€šç”¨æŽ¨æŒ½è¾“å‡ºï¼šCNF=00 MODE=11 => 0x3
#define PORT_SET_DIR_PP(...) \
    ARM_PORT_SET_CFG_(GET_PORT_FROM(__VA_ARGS__), GET_PIN_FROM(__VA_ARGS__), 0x03U)

// 50MHz é€šç”¨å¼€æ¼è¾“å‡ºï¼šCNF=01 MODE=11 => 0x7
#define PORT_SET_DIR_OUT_OC(...) \
    ARM_PORT_SET_CFG_(GET_PORT_FROM(__VA_ARGS__), GET_PIN_FROM(__VA_ARGS__), 0x07U)

// 50MHz å¤ç”¨æŽ¨æŒ½è¾“å‡ºï¼šCNF=10 MODE=11 => 0xB
#define PORT_SET_DIR_OUT_MUX_PP(...) \
    ARM_PORT_SET_CFG_(GET_PORT_FROM(__VA_ARGS__), GET_PIN_FROM(__VA_ARGS__), 0x0BU)

// 50MHz å¤ç”¨å¼€æ¼è¾“å‡ºï¼šCNF=11 MODE=11 => 0xF
#define PORT_SET_DIR_OUT_MUX_OC(...) \
    ARM_PORT_SET_CFG_(GET_PORT_FROM(__VA_ARGS__), GET_PIN_FROM(__VA_ARGS__), 0x0FU)

// æµ®ç©ºè¾“å…¥ï¼šCNF=01 MODE=00 => 0x4
#define PORT_SET_DIR_IN_FLOAT(...) \
    ARM_PORT_SET_CFG_(GET_PORT_FROM(__VA_ARGS__), GET_PIN_FROM(__VA_ARGS__), 0x04U)

// æ¨¡æ‹Ÿè¾“å…¥ï¼šCNF=00 MODE=00 => 0x0
#define PORT_SET_DIR_AIN(...) \
    ARM_PORT_SET_CFG_(GET_PORT_FROM(__VA_ARGS__), GET_PIN_FROM(__VA_ARGS__), 0x00U)

// ä¸Šæ‹‰è¾“å…¥ï¼šCNF=10 MODE=00 => 0x8ï¼Œä¸” ODR å¯¹åº”ä½å†™ 1
#define PORT_SET_DIR_IN_PU(...) \
    do { \
        ARM_PORT_SET_CFG_(GET_PORT_FROM(__VA_ARGS__), GET_PIN_FROM(__VA_ARGS__), 0x08U); \
        PORT_OUT(__VA_ARGS__) = 1; \
    } while (0)

// ä¸‹æ‹‰è¾“å…¥ï¼šCNF=10 MODE=00 => 0x8ï¼Œä¸” ODR å¯¹åº”ä½å†™ 0
#define PORT_SET_DIR_IN_PD(...) \
    do { \
        ARM_PORT_SET_CFG_(GET_PORT_FROM(__VA_ARGS__), GET_PIN_FROM(__VA_ARGS__), 0x08U); \
        PORT_OUT(__VA_ARGS__) = 0; \
    } while (0)

// å…¼å®¹æ—§åå­?
#define PORT_SET_DIR_IN_UPLOAD(...) PORT_SET_DIR_IN_PU(__VA_ARGS__)


//-------- ç»Ÿä¸€çš„ç«¯å?å¼•è„šè®¿é—®å®?--------
#define PORT_OUT(...)            ARM_PORT_OUT(GET_PORT_FROM(__VA_ARGS__))(GET_PIN_FROM(__VA_ARGS__))
#define PORT_IN(...)             ARM_PORT_IN(GET_PORT_FROM(__VA_ARGS__))(GET_PIN_FROM(__VA_ARGS__))
#define PIN_MASK(...)            (1U << GET_PIN_FROM(__VA_ARGS__))

/* -------- µÚ¶þÌ× GPIO ºê£ºÖ±½ÓÊ¹ÓÃ stm32f10x.h ÖÐµÄÎ»¶¨Òå --------
 * 1. Hardware_Config.h ÖÐ¼ÌÐø±£³Ö "B,5" ÕâÖÖÐ´·¨²»±ä¡£
 * 2. ÕâÒ»×éºê¸ÄÓÃ CRL/CRH¡¢IDR¡¢BSRR¡¢BRR¡¢ODR Ö±½Ó·ÃÎÊ¼Ä´æÆ÷¡£
 * 3. ÕâÑùÐ´³öÀ´µÄÐ§¹û¸ü½Ó½ü£º
 *      GPIOE->CRL &= ~(GPIO_CRL_CNF4 | GPIO_CRL_MODE4);
 *      GPIOE->CRL |= GPIO_CRL_MODE4_0;
 *      GPIOE->BSRR = GPIO_BSRR_BS4;
 *      GPIOE->BRR  = GPIO_BRR_BR4;
 *
 * ËµÃ÷£º
 * "B,5" ÕâÀà²ÎÊýÏÈ»á±»²ð³É port=B¡¢pin=5¡£
 * ÓÉÓÚ pin ºóÐø»¹Òª²ÎÓëºêÃûÆ´½Ó£¬ÀýÈçÆ´³É STM_IO_SET_SEL_5_FN£¬
 * Ô¤´¦ÀíÆ÷²»»á×Ô¶¯°Ñ GET_PIN_FROM(...) µÄ½á¹ûÏÈÕ¹¿ªÔÙÆ´½Ó¡£
 * ËùÒÔÕâÀï²ÉÓÃÁ½¼¶Õ¹¿ª£º
 *   STM_IO_SET_(...) -> STM_IO_SET_X(...) -> STM_IO_SET_Y(...)
 * Ä¿µÄÊÇÈ·±£ pin ÏÈ±ä³É¾ßÌåÊý×Ö£¬ÔÙ²ÎÓë ## Æ´½Ó¡£
 * Èç¹ûÉÙÕâÒ»²ã£¬±àÒëÆ÷¾Í»á¿´µ½ÀàËÆ STM_IO_SET_SEL_GET_PIN_FROM(...)
 * ÕâÑùµÄ´íÎóÃû×Ö£¬´Ó¶ø±¨ ¡°identifier E/B/D is undefined¡± Ò»Àà´íÎó¡£
 */
#define STM_IO_GPIO(port)          GPIO##port
#define STM_IO_SET_DIR_PP(...)     STM_IO_SET_DIR_PP_(GET_PORT_FROM(__VA_ARGS__), GET_PIN_FROM(__VA_ARGS__))
#define STM_IO_SET_DIR_IN_PD(...)  STM_IO_SET_DIR_IN_PD_(GET_PORT_FROM(__VA_ARGS__), GET_PIN_FROM(__VA_ARGS__))
#define STM_IO_SET_DIR_IN_PU(...)  STM_IO_SET_DIR_IN_PU_(GET_PORT_FROM(__VA_ARGS__), GET_PIN_FROM(__VA_ARGS__))
#define STM_IO_READ(...)           STM_IO_READ_(GET_PORT_FROM(__VA_ARGS__), GET_PIN_FROM(__VA_ARGS__))
#define STM_IO_SET(...)            STM_IO_SET_(GET_PORT_FROM(__VA_ARGS__), GET_PIN_FROM(__VA_ARGS__))
#define STM_IO_CLR(...)            STM_IO_CLR_(GET_PORT_FROM(__VA_ARGS__), GET_PIN_FROM(__VA_ARGS__))
#define STM_IO_TOGGLE(...)         STM_IO_TOGGLE_(GET_PORT_FROM(__VA_ARGS__), GET_PIN_FROM(__VA_ARGS__))

#define STM_IO_SET_DIR_PP_(port, pin)      STM_IO_SET_DIR_PP_X(port, pin)
#define STM_IO_SET_DIR_IN_PD_(port, pin)   STM_IO_SET_DIR_IN_PD_X(port, pin)
#define STM_IO_SET_DIR_IN_PU_(port, pin)   STM_IO_SET_DIR_IN_PU_X(port, pin)
#define STM_IO_READ_(port, pin)            STM_IO_READ_X(port, pin)
#define STM_IO_SET_(port, pin)             STM_IO_SET_X(port, pin)
#define STM_IO_CLR_(port, pin)             STM_IO_CLR_X(port, pin)
#define STM_IO_TOGGLE_(port, pin)          STM_IO_TOGGLE_X(port, pin)

/* X/Y Á½¼¶Õ¹¿ª½öÓÃÓÚ¡°ÈÃ pin ÏÈÕ¹¿ª³ÉÊý×Ö£¬ÔÙ²ÎÓë ## Æ´½Ó¡±¡£ */
#define STM_IO_SET_DIR_PP_X(port, pin)     STM_IO_SET_DIR_PP_Y(port, pin)
#define STM_IO_SET_DIR_IN_PD_X(port, pin)  STM_IO_SET_DIR_IN_PD_Y(port, pin)
#define STM_IO_SET_DIR_IN_PU_X(port, pin)  STM_IO_SET_DIR_IN_PU_Y(port, pin)
#define STM_IO_READ_X(port, pin)           STM_IO_READ_Y(port, pin)
#define STM_IO_SET_X(port, pin)            STM_IO_SET_Y(port, pin)
#define STM_IO_CLR_X(port, pin)            STM_IO_CLR_Y(port, pin)
#define STM_IO_TOGGLE_X(port, pin)         STM_IO_TOGGLE_Y(port, pin)

#define STM_IO_SET_DIR_PP_Y(port, pin)     STM_IO_SET_DIR_PP_SEL_##pin##_FN(STM_IO_GPIO(port))
#define STM_IO_SET_DIR_IN_PD_Y(port, pin)  STM_IO_SET_DIR_IN_PD_SEL_##pin##_FN(STM_IO_GPIO(port))
#define STM_IO_SET_DIR_IN_PU_Y(port, pin)  STM_IO_SET_DIR_IN_PU_SEL_##pin##_FN(STM_IO_GPIO(port))
#define STM_IO_READ_Y(port, pin)           STM_IO_READ_SEL_##pin##_FN(STM_IO_GPIO(port))
#define STM_IO_SET_Y(port, pin)            STM_IO_SET_SEL_##pin##_FN(STM_IO_GPIO(port))
#define STM_IO_CLR_Y(port, pin)            STM_IO_CLR_SEL_##pin##_FN(STM_IO_GPIO(port))
#define STM_IO_TOGGLE_Y(port, pin)         STM_IO_TOGGLE_SEL_##pin##_FN(STM_IO_GPIO(port))

/* pin=0~7 Ê¹ÓÃ CRL£»Ã¿¸ö pin Éú³ÉÒ»×é¹Ì¶¨º¯Êý¡£ */
#define STM_IO_DEF_CRL(pin) \
    static __inline void STM_IO_SET_DIR_PP_SEL_##pin##_FN(GPIO_TypeDef *gpio) { gpio->CRL &= ~(GPIO_CRL_CNF##pin | GPIO_CRL_MODE##pin); gpio->CRL |= (GPIO_CRL_MODE##pin##_0 | GPIO_CRL_MODE##pin##_1); } \
    static __inline void STM_IO_SET_DIR_IN_PD_SEL_##pin##_FN(GPIO_TypeDef *gpio) { gpio->CRL &= ~(GPIO_CRL_CNF##pin | GPIO_CRL_MODE##pin); gpio->CRL |= GPIO_CRL_CNF##pin##_1; gpio->BRR = GPIO_BRR_BR##pin; } \
    static __inline void STM_IO_SET_DIR_IN_PU_SEL_##pin##_FN(GPIO_TypeDef *gpio) { gpio->CRL &= ~(GPIO_CRL_CNF##pin | GPIO_CRL_MODE##pin); gpio->CRL |= GPIO_CRL_CNF##pin##_1; gpio->BSRR = GPIO_BSRR_BS##pin; } \
    static __inline u8 STM_IO_READ_SEL_##pin##_FN(GPIO_TypeDef *gpio) { return ((gpio->IDR & GPIO_IDR_IDR##pin) != 0U) ? 1U : 0U; } \
    static __inline void STM_IO_SET_SEL_##pin##_FN(GPIO_TypeDef *gpio) { gpio->BSRR = GPIO_BSRR_BS##pin; } \
    static __inline void STM_IO_CLR_SEL_##pin##_FN(GPIO_TypeDef *gpio) { gpio->BRR = GPIO_BRR_BR##pin; } \
    static __inline void STM_IO_TOGGLE_SEL_##pin##_FN(GPIO_TypeDef *gpio) { gpio->ODR ^= GPIO_ODR_ODR##pin; }

/* pin=8~15 Ê¹ÓÃ CRH£»ÓëÉÏÃæÍ¬Àí¡£ */
#define STM_IO_DEF_CRH(pin) \
    static __inline void STM_IO_SET_DIR_PP_SEL_##pin##_FN(GPIO_TypeDef *gpio) { gpio->CRH &= ~(GPIO_CRH_CNF##pin | GPIO_CRH_MODE##pin); gpio->CRH |= (GPIO_CRH_MODE##pin##_0 | GPIO_CRH_MODE##pin##_1); } \
    static __inline void STM_IO_SET_DIR_IN_PD_SEL_##pin##_FN(GPIO_TypeDef *gpio) { gpio->CRH &= ~(GPIO_CRH_CNF##pin | GPIO_CRH_MODE##pin); gpio->CRH |= GPIO_CRH_CNF##pin##_1; gpio->BRR = GPIO_BRR_BR##pin; } \
    static __inline void STM_IO_SET_DIR_IN_PU_SEL_##pin##_FN(GPIO_TypeDef *gpio) { gpio->CRH &= ~(GPIO_CRH_CNF##pin | GPIO_CRH_MODE##pin); gpio->CRH |= GPIO_CRH_CNF##pin##_1; gpio->BSRR = GPIO_BSRR_BS##pin; } \
    static __inline u8 STM_IO_READ_SEL_##pin##_FN(GPIO_TypeDef *gpio) { return ((gpio->IDR & GPIO_IDR_IDR##pin) != 0U) ? 1U : 0U; } \
    static __inline void STM_IO_SET_SEL_##pin##_FN(GPIO_TypeDef *gpio) { gpio->BSRR = GPIO_BSRR_BS##pin; } \
    static __inline void STM_IO_CLR_SEL_##pin##_FN(GPIO_TypeDef *gpio) { gpio->BRR = GPIO_BRR_BR##pin; } \
    static __inline void STM_IO_TOGGLE_SEL_##pin##_FN(GPIO_TypeDef *gpio) { gpio->ODR ^= GPIO_ODR_ODR##pin; }

STM_IO_DEF_CRL(0)
STM_IO_DEF_CRL(1)
STM_IO_DEF_CRL(2)
STM_IO_DEF_CRL(3)
STM_IO_DEF_CRL(4)
STM_IO_DEF_CRL(5)
STM_IO_DEF_CRL(6)
STM_IO_DEF_CRL(7)
STM_IO_DEF_CRH(8)
STM_IO_DEF_CRH(9)
STM_IO_DEF_CRH(10)
STM_IO_DEF_CRH(11)
STM_IO_DEF_CRH(12)
STM_IO_DEF_CRH(13)
STM_IO_DEF_CRH(14)
STM_IO_DEF_CRH(15)

#define SYSTEM_SUPPORT_UCOS		0

//ä½å¸¦æ“ä½œ,å®žçŽ°51ç±»ä¼¼çš„GPIOæŽ§åˆ¶åŠŸèƒ½
#define BITBAND(addr, bitnum) ((addr & 0xF0000000)+0x2000000+((addr &0xFFFFF)<<5)+(bitnum<<2)) 
#define MEM_ADDR(addr)  *((volatile unsigned long  *)(addr)) 
#define BIT_ADDR(addr, bitnum)   MEM_ADDR(BITBAND(addr, bitnum)) 

//IOå£åœ°å€æ˜ å°„
#define GPIOA_ODR_Addr    (GPIOA_BASE+12)
#define GPIOB_ODR_Addr    (GPIOB_BASE+12)
#define GPIOC_ODR_Addr    (GPIOC_BASE+12)
#define GPIOD_ODR_Addr    (GPIOD_BASE+12)
#define GPIOE_ODR_Addr    (GPIOE_BASE+12)
#define GPIOF_ODR_Addr    (GPIOF_BASE+12)
#define GPIOG_ODR_Addr    (GPIOG_BASE+12)

#define GPIOA_IDR_Addr    (GPIOA_BASE+8)
#define GPIOB_IDR_Addr    (GPIOB_BASE+8)
#define GPIOC_IDR_Addr    (GPIOC_BASE+8)
#define GPIOD_IDR_Addr    (GPIOD_BASE+8)
#define GPIOE_IDR_Addr    (GPIOE_BASE+8)
#define GPIOF_IDR_Addr    (GPIOF_BASE+8)
#define GPIOG_IDR_Addr    (GPIOG_BASE+8)

//IOå£æ“ä½?åªå¯¹å•ä¸€çš„IOå?
#define PAout(n)   BIT_ADDR(GPIOA_ODR_Addr,n)
#define PAin(n)    BIT_ADDR(GPIOA_IDR_Addr,n)
#define PBout(n)   BIT_ADDR(GPIOB_ODR_Addr,n)
#define PBin(n)    BIT_ADDR(GPIOB_IDR_Addr,n)
#define PCout(n)   BIT_ADDR(GPIOC_ODR_Addr,n)
#define PCin(n)    BIT_ADDR(GPIOC_IDR_Addr,n)
#define PDout(n)   BIT_ADDR(GPIOD_ODR_Addr,n)
#define PDin(n)    BIT_ADDR(GPIOD_IDR_Addr,n)
#define PEout(n)   BIT_ADDR(GPIOE_ODR_Addr,n)
#define PEin(n)    BIT_ADDR(GPIOE_IDR_Addr,n)
#define PFout(n)   BIT_ADDR(GPIOF_ODR_Addr,n)
#define PFin(n)    BIT_ADDR(GPIOF_IDR_Addr,n)
#define PGout(n)   BIT_ADDR(GPIOG_ODR_Addr,n)
#define PGin(n)    BIT_ADDR(GPIOG_IDR_Addr,n)

//Ex_NVIC_Configä¸“ç”¨å®šä¹‰
#define GPIO_A 0
#define GPIO_B 1
#define GPIO_C 2
#define GPIO_D 3
#define GPIO_E 4
#define GPIO_F 5
#define GPIO_G 6 
#define FTIR   1
#define RTIR   2

void Stm32_Clock_Init(u8 PLL);
void Sys_Soft_Reset(void);
void Sys_Standby(void);
void MY_NVIC_SetVectorTable(u32 NVIC_VectTab, u32 Offset);
void MY_NVIC_PriorityGroupConfig(u8 NVIC_Group);
void MY_NVIC_Init(u8 NVIC_PreemptionPriority,u8 NVIC_SubPriority,u8 NVIC_Channel,u8 NVIC_Group);
void Ex_NVIC_Config(u8 GPIOx,u8 BITx,u8 TRIM);
void Disable_JTAG_Keep_SWD(void);
void WFI_SET(void);
void INTX_DISABLE(void);
void INTX_ENABLE(void);
void MSR_MSP(u32 addr);
/* NULL æŒ‡é’ˆå®šä¹‰ */
#ifndef NULL
#define NULL ((void *)0)
#endif



#endif

