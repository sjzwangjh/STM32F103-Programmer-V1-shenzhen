/*
 * Handler è‡?åŠ¨æœºæ¨¡å??â€?Handler AutoMate Interface
 *
 * åŠŸèƒ½:
 *   å®ç°å…¨è‡ªåŠ¨ç¼–ç¨‹å™¨ä¸æœºæ¢°æ‰‹ (Handler) ä¹‹é—´çš„æ ‡å‡†æ¡æ‰‹ä¿¡å?
 *     SOT (Start of Test)  â€?æœºæ?°æ??â†?ç¼–ç¨‹å™? é€šçŸ¥"èŠ?ç‰‡å·²å°±ä??
 *     EOT (End of Test)    â€?ç¼–ç¨‹å™?â†?æœºæ?°æ?? é€šçŸ¥"æµ‹è¯•/ç¼–ç¨‹å®Œæˆ"
 *     BUSY                  â€?ç¼–ç¨‹å™?â†?æœºæ?°æ?? é€šçŸ¥"æ­£åœ¨å¤„ç†, ç¦æ?¢å–æ”?"
 *     OK / NG               â€?ç¼–ç¨‹å™?â†?æœºæ?°æ?? é€šçŸ¥"è‰?å“?/ä¸è‰¯å“?åˆ†æ‹£
 *
 * ç«?å£æ˜ å°? (æ¥è‡ª Hardware_Config.h ç¬?4~79è¡?:
 *     HW_HANDLER_OK      C,6   (PA8   â€?BUSY)
 *     HW_HANDLER_NG      C,7   (PC7   â€?NG)
 *     HW_HANDLER_BUSY    A,8   (PC6   â€?OK)
 *     HW_HANDLER_START   D,1   (PD1   â€?SOTè¾“å…¥)
 *     HW_HANDLER_UD      D,0   (PD0   â€?SOT ä¸?ä¸‹æ‹‰æ§åˆ¶)
 *
 * NOTE: Hardware_Config.h ä¸?SOT=START=D1, UD=D0ï¼Œè??BUSY/OK/NG æ°å¥½
 *       ä¸åŸ DFM é¡¹ç›®ä¸?"OK=C6, NG=C7, BUSY=A8" çš„å®šä¹‰é¡ºåºä¸€è‡´ã??
 *       æ­¤å?„æ²¿ç”? Hardware_Config.h å·²å®šä¹‰çš„ç«?å£å®ï¼Œä¸åšé?å?–é‡å®šä¹‰ã€?
 *
 * ç§»æ?è?? DFMProgrammer/Product_VET6/.../Src/handler.c
 * åŸæ¨¡å—ä½¿ç”?STM32 HAL åº“ï¼Œç°æ”¹ä¸ºæœ¬é¡¹ç›®è£¸æœºå¯„å­˜å™¨é?æ??(PORT_OUT / PORT_IN)ã€?
 */

#ifndef __HANDLER_H__
#define __HANDLER_H__

#include "sys.h"
#include "Hardware_Config.h"

/* â”?â”? ç«?å£å®å®šä??â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”? */
/* æ‰?æœ‰å¼•è„šå·²åœ?Hardware_Config.h ä¸?é€šè¿‡ HW_HANDLER_xxx å®å®šä¹‰ã??
 * ä¸ºé¿å…é‡å¤å®šä¹‰ï¼Œä¸‹é¢ä»…å®šä¹‰è?»å†™å¿?æ·å®ã€?*/

/* è¯»å– SOT (Start of Test) ä¿¡å·: PD1 */
#define HANDLER_SOT_GET           PORT_IN(HW_HANDLER_START)

#define HANDLER_UD_INIT  do{ STM_IO_SET_DIR_PP(HW_HANDLER_UD); }while(0)
/* SOT ä¸Šæ‹‰ (PD0=0)ï¼šSOT ç©ºé—²æ—¶è??æ‹‰åˆ°ä½ç”µå¹?*/
#define HANDLER_SOT_SET_UP_LOAD   do{ PORT_OUT(HW_HANDLER_UD)=0; }while(0)
/* SOT ä¸‹æ‹‰ (PD0=1)ï¼šSOT ç©ºé—²æ—¶è??æ‹‰åˆ°é«˜ç”µå¹?*/
#define HANDLER_SOT_SET_DW_LOAD   do{ PORT_OUT(HW_HANDLER_UD)=1; }while(0)

/* OK ä¿¡å· (PC6) */
#define HANDLER_OK_INIT  do{ STM_IO_SET_DIR_PP(HW_HANDLER_OK); }while(0)
#define HANDLER_OK_SET   PORT_OUT(HW_HANDLER_OK)=1
#define HANDLER_OK_CLR   PORT_OUT(HW_HANDLER_OK)=0

/* NG ä¿¡å· (PC7) */
#define HANDLER_NG_INIT  do{ STM_IO_SET_DIR_PP(HW_HANDLER_NG); }while(0)
#define HANDLER_NG_SET   PORT_OUT(HW_HANDLER_NG)=1
#define HANDLER_NG_CLR   PORT_OUT(HW_HANDLER_NG)=0

/* BUSY ä¿¡å· (PA8) */
#define HANDLER_BUSY_INIT  do{ STM_IO_SET_DIR_PP(HW_HANDLER_BUSY); }while(0)
#define HANDLER_BUSY_SET  PORT_OUT(HW_HANDLER_BUSY)=1
#define HANDLER_BUSY_CLR  PORT_OUT(HW_HANDLER_BUSY)=0

/* EOT (End of Test) â€?æœ?é¡¹ç›®ä¸?æ— ç‹¬ç«?EOT å¼•è„š, ä½¿ç”¨ OK/NG + BUSY ç»„åˆè¡¨ç¤ºï¼?
 * ä½†ä¸ºæ¥å£å…¼å?¹ï¼Œå®šä¹‰å ä½å®? (å®é™…ä¸ä¼šè§¦å‘)ã€?*/
#define HANDLER_EOT_INIT {}
#define HANDLER_EOT_SET   {}
#define HANDLER_EOT_CLR   {}

/* â”?â”? åˆå?‹åŒ–æ—¶å°†æ‰?æœ‰è¾“å‡ºä¿¡å·å½’ä½?â”?â”? */
#define SET_BIN_TO_DEFAULT         do{ \
            PORT_OUT(HW_HANDLER_OK)=0;   \
            PORT_OUT(HW_HANDLER_NG)=1;   \
            PORT_OUT(HW_HANDLER_BUSY)=0; \
        }while(0)

/* â”?â”? å‚æ•°ç»“æ„ä½?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”? */

/** Handler é…ç½® (ä¿å­˜åœ?Flashä¸? å?é€šè¿‡å‘½ä»¤ä¿?æ”?) */
typedef struct {
    uint8_t  sotLevel;              /* SOT æœ‰æ•ˆç”µå¹³: 0=ä½æœ‰æ•? 1=é«˜æœ‰æ•?*/
    uint8_t  eotLevel;              /* EOT æœ‰æ•ˆç”µå¹³ (ä¿ç•™, æš‚æ— ç¡?ä»?) */
    uint8_t  busyLevel;             /* BUSY æœ‰æ•ˆç”µå¹³: 0=ä½æœ‰æ•? 1=é«˜æœ‰æ•?*/
    uint8_t  passLevel;             /* OK æœ‰æ•ˆç”µå¹³: 0=ä½æœ‰æ•? 1=é«˜æœ‰æ•?*/
    uint8_t  ngLevel;               /* NG æœ‰æ•ˆç”µå¹³: 0=ä½æœ‰æ•? 1=é«˜æœ‰æ•?*/
    uint16_t delayMsBinToEot;       /* BIN åˆ¤å†³è¾“å‡º â†?EOT æ‹‰èµ·çš„ç¨³å®šå»¶æ—?(ms) */
    uint16_t delayMsMinTestTime;    /* æœ?çŸ?æµ‹è¯•æ—¶é??(ms), ç”¨äºæ»¤é™¤æŠ–åŠ¨ */
} HandlerConfigerType;

/** ç»Ÿè?¡è?¡æ•°å™?å­˜æ”¾åœ¨EEPROMä¸?ï¼Œè?°å½•æµ‹è¯•æ•°æ® */
typedef struct {
    uint32_t realTotal;             /* å®é™…æ£?æµ‹æ?»èŠ¯ç‰‡æ•° */
    uint32_t realPassed;            /* å®é™…é€šè¿‡çš„èŠ¯ç‰‡æ•° */
    uint32_t realFaild;             /* å®é™…å¤±è´¥çš„èŠ¯ç‰‡æ•° */
    uint32_t logicTotal;            /* é€»è¾‘æ£?æµ‹æ?»èŠ¯ç‰‡æ•° */
    uint32_t logicPassed;           /* é€»è¾‘é€šè¿‡èŠ?ç‰‡æ??*/
    uint32_t logicFaild;            /* é€»è¾‘å¤±è´¥èŠ?ç‰‡æ??*/
} statisticsType;

/* â”?â”? å…¨å±€å˜é‡ â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”? */
extern HandlerConfigerType  usedHandler;
extern statisticsType       usedStatistics;

/* â”?â”? å‡½æ•°åŸå‹ â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”?â”? */

/** ä»é…ç½?ç¼“å†²åŒºæ›´æ–? Handler ç”µå¹³è®¾ç½® */
void HandlerChangeLevel(uint8_t* pBuff);

/** è¯»å– Handler é…ç½®åˆ°ç¼“å†²åŒº (å?é€‰è¾“å‡? */
void HandlerReadConfig(uint8_t* pByteBuff);

/** å°?Handler é…ç½®å†™å…¥éæ˜“å¤±å­˜å‚?*/
void HandlerUpdateFlash(void);

/** ç»Ÿè?¡ä¿¡æ?è¯»å–/æ›´æ–°/å¤ä½ */
void StatisticsReadParam(uint8_t* pBuff);
void StatisticsUpdataParam(uint8_t* pBuff);
uint8_t StatisticResetParam(void);

/** åˆå?‹å??Handler æ¨¡å—: é…ç½®ç«?å? + è¯»å–å­˜å‚¨çš„é…ç½?*/
void Handler_Task_Init(void);

/** è®¾ç½® BIN ä¿¡å·: bin=0 è¡¨ç¤º PASS, é? è¡¨ç¤º FAIL */
void HandlerSetBin(uint8_t bin);

/**
 * @brief  Handler çŠ¶æ?æœºä¸»å‡½æ•?(æ¯?1ms è°ƒç”¨ä¸?æ¬?
 * @param  stateIndex  0xFF = è‡?ç”±è¿è¡?; å…¶å®ƒå€?= å¼ºåˆ¶è®¾ç½®æ–°çŠ¶æ€?
 *                     (0=å¤ä½, 1=ç­‰å¾…SOT, 2=ç­‰å¾…SOTç»“æŸ, 3=è¾“å‡ºBIN)
 * @param  bin         BIN å€?(3å·çŠ¶æ€æ—¶ä½¿ç”¨)
 * @return 1 = SOT æœ‰æ•ˆ (æ£?æµ‹åˆ°èŠ?ç‰‡å°±ä½?), 0 = ç­‰å¾…ä¸?
 */
uint16_t HandlerTask(uint8_t stateIndex, uint8_t bin);

#endif /* __HANDLER_H__ */

