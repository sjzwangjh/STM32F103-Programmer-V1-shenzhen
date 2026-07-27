/*
 * testUsbHid.c
 * ------------------------------------------------------------
 * 用途：
 *   本文件整理了 4 组可用于调试 STM32F103VET6 Programmer V1
 *   当前 USB HID + STK500v2 数据耦合环境的测试数组。
 *
 * 说明：
 *   1. 这些数组按 AVR-Doper 的 HID/STK500v2 通讯格式整理。
 *   2. host_* 数组表示“上位机发送”的完整 HID Feature Report 数据。
 *   3. dev_*  数组表示“设备正确时应答”的完整 HID Feature Report 数据。
 *   4. 当前 STM32 工程中，真正的 USB 控制传输入口在 usb_prop.c，
 *      STK500v2 解析入口在 HID_Rx_Store() / stkSetRxChar()。
 *
 * HID 数据格式：
 *   byte0 : Report ID
 *   byte1 : 有效 STK500v2 数据长度
 *   byte2... : STK500v2 裸帧
 *
 * STK500v2 裸帧格式：
 *   1B seq lenH lenL 0E body... checksum
 */

#include <stdint.h>

typedef struct
{
    const char    *name;
    uint8_t        report_id;
    const uint8_t *host_data;
    uint16_t       host_len;
    const uint8_t *expect_data;
    uint16_t       expect_len;
} UsbHidTestVector;

/* ============================================================
 * 1. SIGN_ON
 * 作用：
 *   测试 HID 收发链、STK500v2 帧拼装、校验和最基础的命令分发。
 * 预期：
 *   返回 "STK500_2"
 * ============================================================ */
const uint8_t host_sign_on[31] =
{
    0x02, 0x07,
    0x1B, 0x01, 0x00, 0x01, 0x0E, 0x01, 0x14,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00
};

const uint8_t dev_sign_on[31] =
{
    0x02, 0x11,
    0x1B, 0x01, 0x00, 0x0B, 0x0E, 0x01, 0x00, 0x08,
    0x53, 0x54, 0x4B, 0x35, 0x30, 0x30, 0x5F, 0x32, 0x02,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00
};

/* ============================================================
 * 2. GET_PARAMETER(SW_MAJOR = 0x91)
 * 作用：
 *   测试短帧命令应答和参数读取。
 * 预期：
 *   当前 STM32 版应答软件主版本号 0x02
 * ============================================================ */
const uint8_t host_get_sw_major[15] =
{
    0x01, 0x08,
    0x1B, 0x02, 0x00, 0x02, 0x0E, 0x03, 0x91, 0x87,
    0x00, 0x00, 0x00, 0x00, 0x00
};

const uint8_t dev_get_sw_major[15] =
{
    0x01, 0x09,
    0x1B, 0x02, 0x00, 0x03, 0x0E, 0x03, 0x00, 0x02, 0x15,
    0x00, 0x00, 0x00, 0x00
};

/* ============================================================
 * 3. LOAD_ADDRESS(0x00000000)
 * 作用：
 *   测试 4 字节参数命令和地址装载路径。
 * 预期：
 *   返回 CMD_OK
 * ============================================================ */
const uint8_t host_load_addr0[15] =
{
    0x01, 0x0B,
    0x1B, 0x03, 0x00, 0x05, 0x0E, 0x06, 0x00, 0x00,
    0x00, 0x00, 0x15, 0x00, 0x00
};

const uint8_t dev_load_addr0[15] =
{
    0x01, 0x08,
    0x1B, 0x03, 0x00, 0x02, 0x0E, 0x06, 0x00, 0x12,
    0x00, 0x00, 0x00, 0x00, 0x00
};

/* ============================================================
 * 4. ENTER_PROGMODE_ISP
 * 作用：
 *   测试 avrdude 常见 ISP 进入编程模式命令路径。
 * 预期：
 *   当前 STM32 版 stub 应返回 CMD_OK
 * ============================================================ */
const uint8_t host_enter_progmode_isp[31] =
{
    0x02, 0x12,
    0x1B, 0x04, 0x00, 0x0C, 0x0E, 0x10,
    0xC8, 0x64, 0x19, 0x20, 0x00, 0x53, 0x03, 0xAC,
    0x53, 0x00, 0x00, 0x37,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};

const uint8_t dev_enter_progmode_isp[31] =
{
    0x02, 0x08,
    0x1B, 0x04, 0x00, 0x02, 0x0E, 0x10, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* ============================================================
 * 测试表
 * 说明：
 *   上位机程序可以遍历此表，依次发出 host_data，
 *   再将设备返回与 expect_data 做逐字节比较。
 * ============================================================ */
const UsbHidTestVector g_usb_hid_test_vectors[] =
{
    {
        "SIGN_ON",
        0x02,
        host_sign_on, sizeof(host_sign_on),
        dev_sign_on, sizeof(dev_sign_on)
    },
    {
        "GET_PARAMETER_SW_MAJOR",
        0x01,
        host_get_sw_major, sizeof(host_get_sw_major),
        dev_get_sw_major, sizeof(dev_get_sw_major)
    },
    {
        "LOAD_ADDRESS_00000000",
        0x01,
        host_load_addr0, sizeof(host_load_addr0),
        dev_load_addr0, sizeof(dev_load_addr0)
    },
    {
        "ENTER_PROGMODE_ISP",
        0x02,
        host_enter_progmode_isp, sizeof(host_enter_progmode_isp),
        dev_enter_progmode_isp, sizeof(dev_enter_progmode_isp)
    }
};

const uint32_t g_usb_hid_test_vector_count =
    sizeof(g_usb_hid_test_vectors) / sizeof(g_usb_hid_test_vectors[0]);


