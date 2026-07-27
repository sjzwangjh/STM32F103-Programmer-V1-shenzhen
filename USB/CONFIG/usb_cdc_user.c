/*
 * USB CDC用户层实现 - 将CDC字节流接入STK500协议层
 *
 * CDC 数据流架构（中断+轮询模式）:
 *   USB中断 → CDC_DataOut_Callback()
 *             → PMAToUserBufferCopy() 复制数据到环形缓冲区
 *             → 设置 cdcRxPending 标志
 *             → 退出中断（极快，不执行命令）
 *
 *   主循环 → CDC_Task()
 *             → 从环形缓冲区取出字节 → CDC_ProcessByte() 逐字节组帧
 *             → 帧完整 → CDC_ProcessFrame() → stkEvaluateRxMessage()
 *             → 结果放入 TX 帧缓冲 → CDC_StartNextTxPacket() 发送回复
 */

#include "usb_lib.h"
#include "usb_conf.h"
#include "usb_regs.h"
#include "usb_cdc_user.h"
#include <string.h>

#define STK_STX                         27U
#define STK_TOKEN                       14U
#define STK_DATA_SOURCE_USB_CDC         1U

typedef struct stkDataFrame{
    const uint8_t *frame;
    uint16_t frameLen;
    uint8_t *txFrame;
    uint16_t txFrameSize;
    uint16_t txFrameLen;
    uint8_t  source;
} stkDataFrame_t;

void stkEvaluateRxMessage(stkDataFrame_t *pDataFrame);

static uint8_t cdcRxPacket[CDC_RX_PACKET_SIZE];
static uint8_t cdcTxPacket[CDC_TX_PACKET_SIZE];
static uint8_t cdcTxBusy;

/* 环形缓冲区：中断接收 → 主循环处理 */
static uint8_t  cdcRingBuf[CDC_RX_RING_SIZE];
static uint16_t cdcRingHead;
static uint16_t cdcRingTail;
static uint8_t  cdcRxPending;

/* STK500 帧组装缓冲区 */
static uint8_t  cdcFrame[CDC_STK_FRAME_SIZE];
static uint16_t cdcFramePos;
static uint16_t cdcFrameLen;

/* STK500 TX 帧缓冲（stkEvaluateRxMessage 的回复会写入这里） */
static uint8_t  cdcTxFrame[CDC_STK_FRAME_SIZE];
static uint16_t cdcTxFrameLen;
static uint16_t cdcTxFramePos;

static usb_cdc_line_coding_t cdcLineCoding = {
    115200U,
    0U,
    0U,
    8U
};
static uint8_t cdcLineCodingBuf[7];
static uint16_t cdcControlLineState;

/* 环形缓冲区操作 */
static void ringBufWrite(const uint8_t *data, uint16_t len);
static uint8_t ringBufRead(uint8_t *byte);

static void CDC_ProcessByte(uint8_t c);
static void CDC_ProcessFrame(void);
static void CDC_StartNextTxPacket(void);

void CDC_Init(void)
{
    cdcTxBusy = 0U;
    cdcTxFrameLen = 0U;
    cdcTxFramePos = 0U;
    cdcFramePos = 0U;
    cdcFrameLen = 0U;
    cdcControlLineState = 0U;
    cdcRxPending = 0U;
    cdcRingHead = 0U;
    cdcRingTail = 0U;
    CDC_FillLineCodingBuffer();
}

uint8_t CDC_SendData(const uint8_t *data, uint16_t len)
{
    uint16_t sendLen;

    if (data == 0 || len == 0U)
        return 0U;

    if (cdcTxBusy != 0U)
        return 1U;

    sendLen = len;
    if (sendLen > CDC_TX_PACKET_SIZE)
        sendLen = CDC_TX_PACKET_SIZE;

    memcpy(cdcTxPacket, data, sendLen);
    UserToPMABufferCopy(cdcTxPacket, ENDP3_TXADDR, sendLen);
    SetEPTxCount(ENDP3, sendLen);
    cdcTxBusy = 1U;
    SetEPTxValid(ENDP3);
    return 0U;
}

uint8_t CDC_IsTxBusy(void)
{
    return cdcTxBusy;
}

void CDC_DataIn_Callback(void)
{
    cdcTxBusy = 0U;
}

void CDC_DataOut_Callback(void)
{
    uint16_t rxLen;

    rxLen = GetEPRxCount(ENDP3);
    if (rxLen > CDC_RX_PACKET_SIZE)
        rxLen = CDC_RX_PACKET_SIZE;

    PMAToUserBufferCopy(cdcRxPacket, ENDP3_RXADDR, rxLen);
    SetEPRxValid(ENDP3);

    /* 将数据推入环形缓冲区，由主循环 CDC_Task() 处理 */
    ringBufWrite(cdcRxPacket, rxLen);
    cdcRxPending = 1U;
}

void CDC_Task(void)
{
    uint8_t byte;

    /* 处理接收数据：从环形缓冲区取出字节，逐字节组 STK500 帧 */
    if (cdcRxPending)
    {
        cdcRxPending = 0U;
        while (ringBufRead(&byte))
            CDC_ProcessByte(byte);
    }

    /* 处理发送：TX 帧缓冲有数据且无正在进行的发送，则启动发送 */
    if (cdcTxFrameLen != 0U && !cdcTxBusy)
        CDC_StartNextTxPacket();
}

uint8_t *CDC_GetLineCodingBuffer(void)
{
    return cdcLineCodingBuf;
}

void CDC_FillLineCodingBuffer(void)
{
    cdcLineCodingBuf[0] = (uint8_t)(cdcLineCoding.bitrate);
    cdcLineCodingBuf[1] = (uint8_t)(cdcLineCoding.bitrate >> 8);
    cdcLineCodingBuf[2] = (uint8_t)(cdcLineCoding.bitrate >> 16);
    cdcLineCodingBuf[3] = (uint8_t)(cdcLineCoding.bitrate >> 24);
    cdcLineCodingBuf[4] = cdcLineCoding.format;
    cdcLineCodingBuf[5] = cdcLineCoding.paritytype;
    cdcLineCodingBuf[6] = cdcLineCoding.datatype;
}

void CDC_SetLineCodingFromBuffer(void)
{
    cdcLineCoding.bitrate = ((uint32_t)cdcLineCodingBuf[0]) |
                            ((uint32_t)cdcLineCodingBuf[1] << 8) |
                            ((uint32_t)cdcLineCodingBuf[2] << 16) |
                            ((uint32_t)cdcLineCodingBuf[3] << 24);
    cdcLineCoding.format = cdcLineCodingBuf[4];
    cdcLineCoding.paritytype = cdcLineCodingBuf[5];
    cdcLineCoding.datatype = cdcLineCodingBuf[6];
}

void CDC_SetControlLineState(uint16_t state)
{
    cdcControlLineState = state;
}

uint16_t CDC_GetControlLineState(void)
{
    return cdcControlLineState;
}

/* ========== 环形缓冲区实现 ========== */

static void ringBufWrite(const uint8_t *data, uint16_t len)
{
    uint16_t i;
    for (i = 0U; i < len; i++)
    {
        uint16_t next = (uint16_t)(cdcRingHead + 1U) % CDC_RX_RING_SIZE;
        if (next != cdcRingTail)
        {
            cdcRingBuf[cdcRingHead] = data[i];
            cdcRingHead = next;
        }
        /* 缓冲区满则丢弃后续字节 */
    }
}

static uint8_t ringBufRead(uint8_t *byte)
{
    if (cdcRingHead == cdcRingTail)
        return 0U;  /* 空 */

    *byte = cdcRingBuf[cdcRingTail];
    cdcRingTail = (uint16_t)(cdcRingTail + 1U) % CDC_RX_RING_SIZE;
    return 1U;
}

/* ========== STK500 帧组装 ========== */

static void CDC_ProcessByte(uint8_t c)
{
    if (cdcFramePos == 0U)
    {
        if (c == STK_STX)
        {
            cdcFrame[cdcFramePos++] = c;
            cdcFrameLen = 0U;
        }
        return;
    }

    if (cdcFramePos >= CDC_STK_FRAME_SIZE)
    {
        cdcFramePos = 0U;
        cdcFrameLen = 0U;
        return;
    }

    cdcFrame[cdcFramePos++] = c;

    if (cdcFramePos == 4U)
    {
        cdcFrameLen = (uint16_t)(((uint16_t)cdcFrame[2] << 8) | cdcFrame[3]);
        cdcFrameLen = (uint16_t)(cdcFrameLen + 6U);
        if (cdcFrameLen > CDC_STK_FRAME_SIZE)
        {
            cdcFramePos = 0U;
            cdcFrameLen = 0U;
        }
    }
    else if (cdcFramePos == 5U)
    {
        if (c != STK_TOKEN)
        {
            cdcFramePos = 0U;
            cdcFrameLen = 0U;
        }
    }
    else if (cdcFrameLen != 0U && cdcFramePos == cdcFrameLen)
    {
        CDC_ProcessFrame();
        cdcFramePos = 0U;
        cdcFrameLen = 0U;
    }
}

static void CDC_ProcessFrame(void)
{
    uint16_t i;
    uint8_t sum = 0U;
    stkDataFrame_t frame;
    uint16_t sent;

    for (i = 0U; i < cdcFrameLen; i++)
        sum ^= cdcFrame[i];

    if (sum != 0U)
        return;

    frame.frame = cdcFrame;
    frame.frameLen = cdcFrameLen;
    frame.txFrame = cdcTxFrame;
    frame.txFrameSize = CDC_STK_FRAME_SIZE;
    frame.txFrameLen = 0U;
    frame.source = STK_DATA_SOURCE_USB_CDC;
    stkEvaluateRxMessage(&frame);

    sent = frame.txFrameLen;
    cdcTxFrameLen = sent;
    cdcTxFramePos = 0U;
}

static void CDC_StartNextTxPacket(void)
{
    uint16_t chunk;

    if (cdcTxBusy != 0U)
        return;

    if (cdcTxFramePos >= cdcTxFrameLen)
    {
        cdcTxFrameLen = 0U;
        cdcTxFramePos = 0U;
        return;
    }

    chunk = (uint16_t)(cdcTxFrameLen - cdcTxFramePos);
    if (chunk > CDC_TX_PACKET_SIZE)
        chunk = CDC_TX_PACKET_SIZE;

    (void)CDC_SendData(&cdcTxFrame[cdcTxFramePos], chunk);
    cdcTxFramePos = (uint16_t)(cdcTxFramePos + chunk);
}

