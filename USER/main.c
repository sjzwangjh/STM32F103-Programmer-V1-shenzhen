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
#include "stk500Protocol.h"
#include "handler.h"
#include "offLinePgmer.h"
#include "debugBin.h"

/// USB 口对应的MCU引脚定义
#define HW_USB_DP_PORT  A,12

/// @brief 设置USB“使能”状态
/// @param enable = 0：关闭；1：使能
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

/// @brief MCU主函数入口
/// @param  
/// @return 
int main(void)
{
    u16 i=0; u8 key=0;
    u8 handlerKey = 0;
    u8 sysClockMHz;

    sysClockMHz = Stm32_Clock_Init(6);
    delay_init(sysClockMHz);
    uart_init(sysClockMHz,HW_DEBUG_BAUDRATE);
		uart1_WriteString("app start...\r\n");
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

    /* 保留 SWD，关闭 JTAG 即可，避免误关 SWD 调试口。 */
    Disable_JTAG_Keep_SWD();
    DutBus_Init();

    uint32_t timeout = 500;
    while(bDeviceState != CONFIGURED && timeout--) delay_ms(1);
    delay_ms(200);
    LED_ACTIVE=1;

    powerSoftInit(1200,50);
    delay_ms(100);

    LCD_GPIO_Init();
    LCD_Init();
    LCD_DisplayString58(1,12,"DIF Micro");
    LCD_DisplayGraphic(1,1,64,64, bmp_defeng_Logo);
    LCD_DisplayGB2312String(3,9,"Defeng Tech");

    timerInit();
    Adc_Init();             // ADC + DMA1_Channel1

    /* EEPROM 当前仅提供轮询版 SPI 读写接口，无需 DMA 初始化。 */
    SPI_EEPROM_Init();

    /* Flash 默认读写接口也是轮询版。
     * 只有在后续明确调用 SPI_Flash_Read_DMA()/SPI_Flash_Write_Page_DMA()
     * 时，才需要打开 SPI_Flash_DMA_Init()。
     */
    SPI_Flash_Init();
    /* SPI_Flash_DMA_Init(); */

    /* FatFs / diskio 当前走的是 SD_ReadSingleBlock()/SD_ReadBlocks()
     * 这条轮询路径，不会自动使用 SD_ReadBlocks_DMA()/SD_WriteBlocks_DMA()。
     * 因此默认只初始化 SDIO 本体；若后续切换到底层 DMA 接口，
     * 再在 SD_Init() 成功后补做 SD_DMA_Init()。
     */
    if(SD_Init() == SD_OK)
    {
        /* SD_DMA_Init(); */
    }
		/* 初始化 Handler */
    Handler_Task_Init();
	HandlerTask(1,1);   // 初始化发送一个失效信号
    /* 离线编程器初始化 */
    offlinePgmer_init();
    debugBin_Init();
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
        /* 机械手信号读取 */
        handlerKey = HandlerTask(1,0);
        if(handlerKey>0){
            offlinePgmer();
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

