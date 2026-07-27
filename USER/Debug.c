/*====================================================================
 * Debug.c - 调试模块
 * 通过串口命令对各硬件模块做基础联调。
 * 当前支持的主要功能：
 * 1. USB HID 重新枚举
 * 2. LED / 蜂鸣器控制
 * 3. DUT 电源与总线方向控制
 * 4. EEPROM / Flash / SD 卡调试
 * 5. PE4 / PE6 直驱测试
 *====================================================================*/

#include "sys.h"
#include "Hardware_Config.h"
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
#include "power.h"
#include "MCP4017_VPP.h"
#include "MCP4017_VDD.h"

#include "dutBus.h"
#include "sdCardUser.h"
#include "picDeviceConst.h"
#include "avrDeviceConst.h"
#include "Debug.h"
#include <string.h>
#include <stdlib.h>

/* 最多支持 8 个参数，每个参数最长 15 个字符。 */
#define CMD_PARAM_MAX       8
#define CMD_PARAM_LEN_MAX   16
static char cmdParam[CMD_PARAM_MAX][CMD_PARAM_LEN_MAX];

/* 外部函数：控制 USB 口重新断开/接入。 */
extern void usb_port_set(u8 enable);

/* 命令字符串表，顺序必须与 debugCmdEnum 保持一致。 */
char debugCmdList[][15] =
{
    "hid",
    "led",
    "beep",
    "vbus",
    "vppset",
    "vddset",
    "dutvpp",
    "dutvdd",
    "dbgpin",
    "dbgdelay",
    "pin",
    "handler",
    "adc",
    "weeprom",
    "reeprom",
    "wflash",
    "rflash",
    "wsd",
    "rsd",
    "lsd",
    "rdavrparam",
    "rdpicparam",
    "help",
    "test",
};

void debugPrintHelp(void)
{
    printf("-------------------HELP:--------------------------\r\n");
    printf("\"命令 参数1 参数2 ...\" => 执行对应调试命令\r\n");
    printf("hid\t=> 重新枚举 USB HID\r\n");
    printf("led\t序号(0/1/2) 状态(on/off/chg) => 设置 LED 状态\r\n");
    printf("beep\t状态(on/off) => 设置蜂鸣器状态\r\n");
    printf("vbus\t状态(on/off) => 控制 DUT 的 USB 5V 电源\r\n");
    printf("vppset\t电压值(单位V) => 设置 VPP 输出电压\r\n");
    printf("vddset\t电压值(单位V) => 设置 VDD 输出电压\r\n");
    printf("dutvpp\tvpp/gnd/float => 设置 DUT VPP 导通状态\r\n");
    printf("dutvdd\tvdd/gnd/float => 设置 DUT VDD 导通状态\r\n");
    printf("dbgpin\t运行总线输出翻转测试\r\n");
    printf("dbgdelay\t运行延时函数测试\r\n");
    printf("pin\t掩码(HEX) 方向(in/out) 电平(0/1) => 设置 DUT PIN4~8\r\n");
    printf("handler\t预留\r\n");
    printf("adc\t预留\r\n");
    printf("weeprom\t运行 EEPROM 调试示例\r\n");
    printf("reeprom\t预留\r\n");
    printf("wflash\t运行 Flash 调试示例\r\n");
    printf("rflash\t预留\r\n");
    printf("wsd\t运行 SD 文件系统调试示例\r\n");
    printf("rsd\t运行 SD 块读写调试示例\r\n");
    printf("lsd\t列出 SD 卡根目录文件\r\n");
    printf("rdavrparam\t读取 AVR 芯片参数\r\n");
    printf("rdpicparam\t读取 PIC 芯片参数\r\n");
    printf("help\t显示帮助\r\n");
    printf("test\tpe => 直接测试 PE4/PE6 输出\r\n");
    printf("---------------------------------------------------\r\n");
}

void dutPinOutDebug(void)
{
    u16 i;

    DUT_VPP_SET_VPP;
    DUT_VDD_SET_VDD;
    DUT_BUS_SET_OUTPUT;

    for (i = 0; i < 1000; i++)
    {
        if (i % 200 == 0)
        {
            DUT_VPP_SET_GND;
            DUT_VDD_SET_GND;
        }
        else if (i % 250 == 0)
        {
            DUT_VPP_SET_VPP;
            DUT_VDD_SET_VDD;
        }

        PORT_OUT(HW_DUT_PIN4_DAT) = 1;
        PORT_OUT(HW_DUT_PIN5_DAT) = 1;
        PORT_OUT(HW_DUT_PIN6_DAT) = 1;
        PORT_OUT(HW_DUT_PIN7_DAT) = 1;
        PORT_OUT(HW_DUT_PIN8_DAT) = 1;

        PORT_OUT(HW_DUT_PIN4_DAT) = 0;
        PORT_OUT(HW_DUT_PIN5_DAT) = 0;
        PORT_OUT(HW_DUT_PIN6_DAT) = 0;
        PORT_OUT(HW_DUT_PIN7_DAT) = 0;
        PORT_OUT(HW_DUT_PIN8_DAT) = 0;
    }

    DUT_VPP_SET_FLOAT;
    DUT_VDD_SET_FLOAT;
    DUT_BUS_SET_INPUT;
}

void timerDelayDebug(void)
{
    u16 cnt;
    u8 dat = 0;

    DUT_VDD_SET_VDD;
    DUT_PIN8_SET_OUTPUT;

    for (cnt = 0; cnt < 6; cnt++)
    {
        timerMsDelay(10);
        PORT_OUT(HW_DUT_PIN8_DAT) = dat;
        dat ^= 1;
    }

    for (cnt = 0; cnt < 10; cnt++)
    {
        timerTicksDelay(10);
        PORT_OUT(HW_DUT_PIN8_DAT) = dat;
        dat ^= 1;
    }

    DUT_VDD_SET_FLOAT;
    DUT_PIN8_SET_INPUT;
}

void debugPe4Pe6DirectTest(void)
{
    u8 i;

    PORT_RCC_CLK(HW_DUT_PIN7_CTRL);
    PORT_RCC_CLK(HW_DUT_PIN8_CTRL);
    PORT_SET_DIR_PP(HW_DUT_PIN7_CTRL);
    PORT_SET_DIR_PP(HW_DUT_PIN8_CTRL);

    printf("PE4/PE6 直驱测试开始\r\n");
    for (i = 0; i < 6; i++)
    {
        PORT_OUT(HW_DUT_PIN7_CTRL) = 1;
        PORT_OUT(HW_DUT_PIN8_CTRL) = 1;
        printf("CTRL=1 ODR=0x%04X IDR=0x%04X\r\n", (u16)GPIOE->ODR, (u16)GPIOE->IDR);
        delay_ms(200);

        PORT_OUT(HW_DUT_PIN7_CTRL) = 0;
        PORT_OUT(HW_DUT_PIN8_CTRL) = 0;
        printf("CTRL=0 ODR=0x%04X IDR=0x%04X\r\n", (u16)GPIOE->ODR, (u16)GPIOE->IDR);
        delay_ms(200);
    }
    printf("PE4/PE6 直驱测试结束\r\n");
}

/// @brief 根据输入参数，设置DUT总线的PIN4~PIN8的方向和电平状态。
/// @param pinMask ：参考以下设置
/// * bit0 -> PIN4
/// * bit1 -> PIN5
/// * bit2 -> PIN6
/// * bit3 -> PIN7
/// * bit4 -> PIN8
/// @param pinState ：1表示设置为输出，0表示设置为输入。
/// @param pinValue ：当pinState为输出时，设置对应PIN的电平状态，1表示高电平，0表示低电平。
void debugSetPinState(u8 pinMask, u8 pinState, u8 pinValue)
{
    if (pinMask & 1U)
    {
        if (pinState)
        {
            DUT_PIN4_SET_OUTPUT;
            PORT_OUT(HW_DUT_PIN4_DAT) = pinValue;
            printf("DUT PIN4 设置为输出，电平=%d\r\n", pinValue);
        }
        else
        {
            DUT_PIN4_SET_INPUT;
            printf("DUT PIN4 设置为输入\r\n");
        }
    }

    if (pinMask & 2U)
    {
        if (pinState)
        {
            DUT_PIN5_SET_OUTPUT;
            PORT_OUT(HW_DUT_PIN5_DAT) = pinValue;
            printf("DUT PIN5 设置为输出，电平=%d\r\n", pinValue);
        }
        else
        {
            DUT_PIN5_SET_INPUT;
            printf("DUT PIN5 设置为输入\r\n");
        }
    }

    if (pinMask & 4U)
    {
        if (pinState)
        {
            DUT_PIN6_SET_OUTPUT;
            PORT_OUT(HW_DUT_PIN6_DAT) = pinValue;
            printf("DUT PIN6 设置为输出，电平=%d\r\n", pinValue);
        }
        else
        {
            DUT_PIN6_SET_INPUT;
            printf("DUT PIN6 设置为输入\r\n");
        }
    }

    if (pinMask & 8U)
    {
        if (pinState)
        {
            DUT_PIN7_SET_OUTPUT;
            PORT_OUT(HW_DUT_PIN7_DAT) = pinValue;
            printf("DUT PIN7 设置为输出，电平=%d\r\n", pinValue);
        }
        else
        {
            DUT_PIN7_SET_INPUT;
            printf("DUT PIN7 设置为输入\r\n");
        }
    }

    if (pinMask & 0x10U)
    {
        if (pinState)
        {
            DUT_PIN8_SET_OUTPUT;
            PORT_OUT(HW_DUT_PIN8_DAT) = pinValue;
            printf("DUT PIN8 设置为输出，电平=%d\r\n", pinValue);
        }
        else
        {
            DUT_PIN8_SET_INPUT;
            printf("DUT PIN8 设置为输入\r\n");
        }
    }
}

void usartCmdTask(void)
{
    static debugCmdEnum_t nowDebugCmd = DEBUG_CMD_MAX;
    u8 cmdBuf[64];
    char fileList[30][SDCARD_USER_MAX_NAME_LEN];
    u16 len;
    u8 i;
    u8 paramCnt;
    char *token;
    u8 tmpU81;
    u16 tmpU16;
    float tmpFloat;
    char *endptr;

    len = uart1_ReadLine(cmdBuf, sizeof(cmdBuf));
    if (len == 0)
        return;

    paramCnt = 0;
    token = strtok((char *)cmdBuf, " ,");
    if (token == NULL)
        return;

    strncpy(cmdParam[0], token, CMD_PARAM_LEN_MAX - 1);
    cmdParam[0][CMD_PARAM_LEN_MAX - 1] = '\0';
    paramCnt = 1;

    while (paramCnt < CMD_PARAM_MAX)
    {
        token = strtok(NULL, " ,");
        if (token == NULL)
            break;
        strncpy(cmdParam[paramCnt], token, CMD_PARAM_LEN_MAX - 1);
        cmdParam[paramCnt][CMD_PARAM_LEN_MAX - 1] = '\0';
        paramCnt++;
    }

    for (i = 0; i < (u8)DEBUG_CMD_MAX; i++)
    {
        if (strcmp(cmdParam[0], debugCmdList[i]) == 0)
        {
            nowDebugCmd = (debugCmdEnum_t)i;
            break;
        }
    }

    if (i >= (u8)DEBUG_CMD_MAX)
    {
        printf("收到未知命令\r\n");
        return;
    }

    printf("收到命令: %s, 序号=%d, 参数个数=%d\r\n", cmdParam[0], i, paramCnt - 1);

    switch (nowDebugCmd)
    {
    case DEBUG_CMD_HID:
        BEEP = 1;
        usb_port_set(0);
        delay_ms(300);
        usb_port_set(1);
        printf("USB HID 重新枚举完成\r\n");
        BEEP = 0;
        break;

    case DEBUG_CMD_LED:
        if ((paramCnt >= 3U) && (cmdParam[1][0] >= '0'))
        {
            if (strcmp(cmdParam[2], "off") == 0)
                tmpU81 = 0;
            else if (strcmp(cmdParam[2], "on") == 0)
                tmpU81 = 1;
            else
                tmpU81 = 2;

            ledSetState(cmdParam[1][0] - '0', tmpU81);
            printf("LED 操作完成: %s\r\n", cmdParam[2]);
        }
        else
        {
            printf("格式: led 0/1/2 on/off/chg\r\n");
        }
        break;

    case DEBUG_CMD_BEEP:
        if (paramCnt >= 2U)
        {
            if (strcmp(cmdParam[1], "on") == 0)
                BEEP = 1;
            else
                BEEP = 0;
        }
        else
        {
            printf("格式: beep on/off\r\n");
        }
        break;

    case DEBUG_CMD_VBUS:
        if (paramCnt >= 2U)
        {
            PORT_OUT(HW_USB_ON) = (strcmp(cmdParam[1], "on") == 0) ? 1 : 0;
            printf("DUT 5V 总开关: %s\r\n", cmdParam[1]);
        }
        else
        {
            printf("格式: vbus on/off\r\n");
        }
        break;

    case DEBUG_CMD_VPPSET:
        if (paramCnt >= 2U)
        {
            tmpFloat = (float)atof(cmdParam[1]);
            tmpU16 = (u16)(tmpFloat * 100.0f);
            MCP4017_VPP_SetVoltage(tmpU16);
            printf("VPP 设置为 %s V\r\n", cmdParam[1]);
        }
        else
        {
            printf("格式: vppset 10.5\r\n");
        }
        break;

    case DEBUG_CMD_VDDSET:
        if (paramCnt >= 2U)
        {
            tmpFloat = (float)atof(cmdParam[1]);
            tmpU16 = (u16)(tmpFloat * 100.0f);
            MCP4017_VDD_SetVoltage(tmpU16);
            printf("VDD 设置为 %s V\r\n", cmdParam[1]);
        }
        else
        {
            printf("格式: vddset 4.5\r\n");
        }
        break;

    case DEBUG_CMD_DUTVPP:
        if (paramCnt >= 2U)
        {
            if (strcmp(cmdParam[1], "vpp") == 0){
                DUT_VPP_SET_VPP;
            }else if (strcmp(cmdParam[1], "gnd") == 0){
                DUT_VPP_SET_GND;
            }else{
                DUT_VPP_SET_FLOAT;
            }
            printf("DUT VPP = %s，完成\r\n", cmdParam[1]);
        }
        else
        {
            printf("格式: dutvpp vpp/gnd/float\r\n");
        }
        break;

    case DEBUG_CMD_DUTVDD:
        if (paramCnt >= 2U)
        {
            if (strcmp(cmdParam[1], "vdd") == 0){
                DUT_VDD_SET_VDD;
            }else if (strcmp(cmdParam[1], "gnd") == 0){
                DUT_VDD_SET_GND;
            }else{
                DUT_VDD_SET_FLOAT;
            }

            printf("DUT VDD = %s，完成\r\n", cmdParam[1]);
        }
        else
        {
            printf("格式: dutvdd vdd/gnd/float\r\n");
        }
        break;

    case DEBUG_CMD_DBGPIN:
        dutPinOutDebug();
        printf("DUT 总线连续翻转测试完成\r\n");
        break;

    case DEBUG_CMD_DBGDELAY:
        timerDelayDebug();
        printf("延时函数测试完成，请抓波形确认延时时间\r\n");
        break;

    case DEBUG_CMD_PIN:
        if (paramCnt >= 4U)
        {
            debugSetPinState((u8)strtol(cmdParam[1], &endptr, 16),
                             (u8)((strcmp(cmdParam[2], "out") == 0) ? 1 : 0),
                             (u8)atoi(cmdParam[3]));
            printf("PIN 设置完成\r\n");
        }
        else
        {
            printf("格式: pin 1F out/in 0/1\r\n");
        }
        break;

    case DEBUG_CMD_HANDLER:
        printf("handler 命令暂未实现\r\n");
        break;

    case DEBUG_CMD_ADC:
        printf("adc 命令暂未实现\r\n");
        break;

    case DEBUG_CMD_WEEPROM:
        SPI_EEPROM_DebugDemo();
        break;

    case DEBUG_CMD_REEPROM:
        printf("reeprom 命令暂未实现\r\n");
        break;

    case DEBUG_CMD_WFLASH:
        SPI_Flash_DebugDemo();
        break;

    case DEBUG_CMD_RFLASH:
        SPI_Flash_DebugDemo_DMA();
        //printf("rflash 命令暂未实现\r\n");
        break;

    case DEBUG_CMD_WSD:
        SDCardUser_DebugDemo();
        break;

    case DEBUG_CMD_RSD:
        SDCardUser_BlockRwDebugDemo();
        printf("开始DMA传输测试\r\n");
        SDCard_DebugDemo_DMA();
        break;

    case DEBUG_CMD_LSD:
        tmpU16 = SDCardUser_ListFiles(fileList, 30);
        if (tmpU16 > 0U)
        {
            printf("SD 卡根目录文件列表:\r\n");
            for (i = 0; i < tmpU16; i++)
            {
                printf("文件%d: %s\r\n", i + 1, fileList[i]);
            }
        }
        else
        {
            printf("SD 卡根目录下没有文件\r\n");
        }
        break;
    case DEBUG_CMD_RDAVRPARAM:
        printf("读取 AVR 芯片参数命令暂未实现\r\n");
        break;
    case DEBUG_CMD_RDPICPARAM:
        printf("读取设备支持的 PIC 芯片名称列表\r\n");
        if(paramCnt >= 3U)
        {
            tmpU16 = pic8GetDeviceList((u16)atoi(cmdParam[1]), (u16)atoi(cmdParam[2]));
            printf("总计支持的 PIC 芯片数量: %d\r\n", tmpU16);
        }
        else if(paramCnt >= 2U)
        {
            tmpU16 = pic8GetDeviceList((u16)atoi(cmdParam[1]), PIC8_DEVICE_TABLE_SIZE);
            printf("总计支持的 PIC 芯片数量: %d\r\n", tmpU16);
        }
        else
        {
            tmpU16 = pic8GetDeviceList(0, PIC8_DEVICE_TABLE_SIZE);
            printf("格式: rdpicparam 起始索引 读取数量\r\n");
        }
        break;
    case DEBUG_CMD_HELP:
        debugPrintHelp();
        break;

    case DEBUG_CMD_TEST:
        if ((paramCnt >= 2U) && (strcmp(cmdParam[1], "pe") == 0))
        {
            debugPe4Pe6DirectTest();
        }
        else
        {
            printf("测试参数: %s\r\n", (paramCnt >= 2U) ? cmdParam[1] : "");
            printf("可用: test pe\r\n");
        }
        break;
        
    default:
        debugPrintHelp();
        break;
    }
}
