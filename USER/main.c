#include "sys.h"
#include "usart.h"
#include "delay.h"
#include "led.h"
#include "beep.h"
#include "key.h"
#include "exti.h"
#include "wdg.h"
#include "timer.h"
#include "rtc.h"
#include "wkup.h"
#include "adc.h"
#include "dac.h"
#include "dma.h"
#include "flash.h"
#include "eeprom.h"
#include "sdcard.h"

#include "usb_lib.h"
#include "hw_USB_config.h"
#include "usb_pwr.h"
#include "Lcd12864Bmp.h"
#include "Lcd12864.h"
#include "power.h"
#include "dutBus.h"
#include "usb_hid_user.h"
#include "usb_cdc_user.h"
#include "usb_winusb_user.h"
#include "usbPort.h"
#include "stk500Protocol.h"
#include "handler.h"
#include "offLinePgmer.h"
#include "debugBin.h"

#define APP_VERSION_TEXT  "APP 01.00.00"
#define APP_BUILD_TIME_TEXT "2026-08-28 00:00:00"

extern uint8_t stkBootConfirmApplicationReady(void);

const boot_app_image_info_t g_appImageInfo __attribute__((used, at(APP_INFO_ADDR))) =
{
    BOOT_APP_IMAGE_INFO_MAGIC,
    BOOT_APP_IMAGE_INFO_VERSION,
    BOOT_APP_IMAGE_TYPE_APP,
    APP_CODE_BASE,
    0U,
    0U,
    APP_VERSION_TEXT,
    APP_BUILD_TIME_TEXT,
    {0U, 0U, 0U, 0U, 0U},
    0U
};

/* Offline replay result record in SPI EEPROM (0x0040, 4 bytes):
 * byte0 magic 0xA5, byte1 version 1, byte2-3 last failed packet no (u16 BE).
 * All-0xFF means no failure recorded. */
#define OFFLINE_REPLAY_RESULT_MAGIC     0xA5U
#define OFFLINE_REPLAY_RESULT_VER       1U
#define OFFLINE_REPLAY_RESULT_ADDR      0x0040UL

/// USB 口对应的MCU引脚定义
#define HW_USB_DP_PORT  A,12

/// @brief 设置USB“使能”状�?
/// @param enable = 0：关闭；1：使�?
void usb_port_set(u8 enable)
{
    RCC->APB2ENR|=1<<2;
    if(enable){
        _SetCNTR(_GetCNTR() & (~(1<<1)));
    }else{
        _SetCNTR(_GetCNTR() | (1<<1));
        PORT_SET_DIR_PP(HW_USB_DP_PORT);
        PORT_OUT(HW_USB_DP_PORT)=0;
    }
}

/// @brief MCU主函数入�?
/// @param  
/// @return 
int main(void)
{
    u16 i=0; u8 key=0;
    u8 handlerKey = 0;
    u8 sysClockMHz;

    AppProgrammer_EarlyTrace();

    sysClockMHz = Stm32_Clock_Init(6);
    delay_init(sysClockMHz);
    uart_init(sysClockMHz,HW_DEBUG_BAUDRATE);
    uart1_WriteString("[App] start\r\n");
    if(sysClockMHz != 72U)
    {
        uart1_WriteString("[App] clock init failed, running on HSI 8MHz\r\n");
    }else{
        uart1_WriteString("[App] clock init success, running on HSI 72MHz\r\n");
	}

    LED_Init(); KEY_Init(); BEEP_Init();

    BEEP=1; usb_port_set(0); delay_ms(300); usb_port_set(1); BEEP=0;

    USB_Interrupts_Config();
    Set_USBClock();
    USB_Init();
    uart1_WriteString("[App] usb init\r\n");

    /* 保留 SWD，关�?JTAG 即可，避免误�?SWD 调试口�?*/
    Disable_JTAG_Keep_SWD();
    DutBus_Init();

    uint32_t timeout = 500;
    while(bDeviceState != CONFIGURED && timeout--) delay_ms(1);
    delay_ms(200);
    uart1_WriteString((bDeviceState == CONFIGURED) ? "[App] usb ok\r\n" : "[App] usb timeout\r\n");
    LED_ACTIVE=1;

    powerSoftInit(1200,50);
    delay_ms(100);

    LCD_GPIO_Init();
    LCD_Init();
    LCD_DisplayGraphic(1,1,64,64, bmp_defeng_Logo);
    LCD_DisplayString58(1,12,"DIF Micro");
    LCD_DisplayString58(3,12,"DefengTech");
    LCD_DisplayString58(5,12,"Programmer");

    timerInit();
    Adc_Init();             // ADC + DMA1_Channel1

    /* EEPROM 当前仅提供轮询版 SPI 读写接口，无需 DMA 初始化�?*/
    SPI_EEPROM_Init();
    uart1_WriteString("[App] eeprom init\r\n");

    /* Flash 默认读写接口也是轮询版�?
     * 只有在后续明确调�?SPI_Flash_Read_DMA()/SPI_Flash_Write_Page_DMA()
     * 时，才需要打开 SPI_Flash_DMA_Init()�?
     */
    SPI_Flash_Init();
    uart1_WriteString("[App] spi flash init\r\n");
    /* SPI_Flash_DMA_Init(); */

    /* FatFs / diskio 当前走的�?SD_ReadSingleBlock()/SD_ReadBlocks()
     * 这条轮询路径，不会自动使�?SD_ReadBlocks_DMA()/SD_WriteBlocks_DMA()�?
     * 因此默认只初始化 SDIO 本体；若后续切换到底�?DMA 接口�?
     * 再在 SD_Init() 成功后补�?SD_DMA_Init()�?
     */
    if(SD_Init() == SD_OK)
    {
        /* SD_DMA_Init(); */
        uart1_WriteString("[App] sd ok\r\n");
    }
    else
    {
        uart1_WriteString("[App] sd fail\r\n");
    }
		/* 初始�?Handler */
    Handler_Task_Init();
    HandlerTask(1,1);   // 初始化发送一个失效信�?
    /* 离线编程器初始化 */
    offlinePgmer_init();
    uart1_WriteString("[App] core init done\r\n");
    debugBin_Init();
    (void)stkBootConfirmApplicationReady();
    uart1_WriteString("[App] wait confirm\r\n");
    while(1)
    {
        i++;
        if(i==100000) 
        { 
            i=0; 
            LED_HALT=!LED_HALT; 
        }
        key=KEY_Scan(0);
        debugBin_Task();
        if(key!=0)
        { 
            BEEP = !BEEP; 
        }
        CDC_Task();     /* USB CDC: RX drain + TX flush (EP3)         */
        HID_Task();     /* USB HID: RX drain + TX flush (EP1)         */
        WinUSB_Task();  /* USB WinUSB Bulk: RX drain + TX flush (EP4) */
        /* 机械手信号读�?*/
        handlerKey = HandlerTask(0xFF,0);  /* free-run handler state machine */
        if(handlerKey>0){
            uint16_t replayResult = offlinePgmer();
            if (replayResult == 0U)
            {
                /* PASS: short beep + ACTIVE LED blink, clear fail record. */
                BEEP = 1; delay_ms(60); BEEP = 0;
                LED_ACTIVE = 1; delay_ms(100); LED_ACTIVE = 0;
                delay_ms(80);
                LED_ACTIVE = 1; delay_ms(100); LED_ACTIVE = 0;
                LED_RESET = 0;
                SPI_EEPROM_WriteByte(OFFLINE_REPLAY_RESULT_ADDR, 0xFFU);
            }
            else
            {
                /* FAIL: long beep + RESET LED on, record failed packet no. */
                BEEP = 1; delay_ms(400); BEEP = 0;
                LED_RESET = 1;
                SPI_EEPROM_WriteByte(OFFLINE_REPLAY_RESULT_ADDR + 0U, OFFLINE_REPLAY_RESULT_MAGIC);
                SPI_EEPROM_WriteByte(OFFLINE_REPLAY_RESULT_ADDR + 1U, OFFLINE_REPLAY_RESULT_VER);
                SPI_EEPROM_WriteByte(OFFLINE_REPLAY_RESULT_ADDR + 2U, (uint8_t)(replayResult >> 8U));
                SPI_EEPROM_WriteByte(OFFLINE_REPLAY_RESULT_ADDR + 3U, (uint8_t)(replayResult & 0xFFU));
            }
            /* Report PASS/FAIL to the handler after the offline test. */
            HandlerSetBin((uint8_t)(replayResult == 0U ? 0U : 1U));
        }
        if(stkFwUpgradeRequested())
        {
            /* Wait for the HID reply to be fetched, then give the CDC IN endpoint
             * time to finish, so STK_STATUS_CMD_OK is delivered before the reset. */
            if(stkGetTxCount() == 0)
            {
                delay_ms(100);
                Sys_Soft_Reset();
            }
        }
    }
}

