/*
 * SPI NOR Flash ����ģ��
 * ���� STM32F103VET6 �� SPI2 ����
 * ֧�� W25X/W25Q ϵ�� SPI Flash оƬ
 * �ṩ��ѯ�� DMA ���ִ��䷽ʽ
 */

#include "flash.h"
#include "spi.h"
#include "delay.h"
#include <string.h>
#include <stdio.h>

u16 SPI_FLASH_TYPE = 0;                 /* Flash оƬ�ͺţ�����ֵ�� */
static u8 SPI_FLASH_BUFFER[FLASH_SECTOR_SIZE];  /* ��-��-д����������С=1������ */
static volatile u8 g_spiFlashTimeout;

#define SPI_FLASH_BUSY_TIMEOUT  1000000UL

/*
 * SPI_Flash_ClipLength - �ضϳ����Բ�����оƬ�����߽�
 * addr: ��ʼ��ַ
 * len:  ���󳤶�
 * ����ֵ: ʵ�ʿ��ó��ȣ����������򷵻�0��ض�ֵ��
 */
static u16 SPI_Flash_ClipLength(u32 addr, u16 len)
{
    u32 remain;

    if (addr >= FLASH_CAPACITY)
        return 0;                       /* ��ʼ��ַ��������������0 */

    remain = FLASH_CAPACITY - addr;     /* ʣ����ÿռ� */
    if ((u32)len > remain)
        len = (u16)remain;              /* �ضϵ����ÿռ䳤�� */

    return len;
}

/*
 * SPI_Flash_Init - ��ʼ�� SPI Flash ģ��
 * ���� CS �� WP ����Ϊ GPIO ���
 * ��ʼ�� SPI2 ����
 * ��ȡ������ Flash оƬ ID
 */
void SPI_Flash_Init(void)
{
    u16 id;

    g_spiFlashTimeout = 0U;

    /* ʹ�� CS �� WP ���ŵ� GPIO ʱ�� */
    PORT_RCC_CLK(HW_FLASH_CS);
    PORT_RCC_CLK(HW_FLASH_WP);

    /* ���� CS �� WP Ϊ������� */
    PORT_SET_DIR_PP(HW_FLASH_CS);
    PORT_SET_DIR_PP(HW_FLASH_WP);

    FLASH_CS_H();           /* CS ��ʼΪ�ߣ���ѡ�У� */
    FLASH_WP_H();           /* WP ��ʼΪ�ߣ���ֹд������ */

    SPI2_Init();            /* ��ʼ�� SPI2 */
    SPI2_SetSpeed(SPI_SPEED_2); /* ���� SPI ʱ��Ϊ���٣�Լ 9MHz/2�� */

    /* ��ȡоƬ ID ��ȷ�� Flash �Ƿ��������� */
    id = SPI_Flash_ReadID();
    if (id == 0x0000 || id == 0xFFFF)   /* ID ��Чʱ���Զ� JEDEC ID */
        id = (u16)(SPI_Flash_ReadJEDECID() & 0xFFFFU);

    SPI_FLASH_TYPE = id;    /* ����оƬ�ͺ� */
}

/*
 * SPI_Flash_ReadSR - ��״̬�Ĵ���
 * ����ֵ: ״̬�Ĵ���ֵ
 *   bit0: BUSY��1=æ��
 *   bit1: WEL��дʹ�����棩
 */
u8 SPI_Flash_ReadSR(void)
{
    u8 byte;

    FLASH_CS_L();                       /* ѡ��оƬ */
    SPI2_ReadWriteByte(W25X_ReadStatusReg); /* ���Ͷ�״̬�Ĵ���ָ�� */
    byte = SPI2_ReadWriteByte(0xFF);    /* ��ȡ״̬�Ĵ���ֵ */
    FLASH_CS_H();                       /* ȡ��ѡ�� */

    return byte;
}

/*
 * SPI_FLASH_Write_SR - д״̬�Ĵ���
 * sr: Ҫд���״̬�Ĵ���ֵ
 */
void SPI_FLASH_Write_SR(u8 sr)
{
    SPI_FLASH_Write_Enable();           /* д��״̬�Ĵ���ǰ��Ҫдʹ�� */
    FLASH_CS_L();
    SPI2_ReadWriteByte(W25X_WriteStatusReg);
    SPI2_ReadWriteByte(sr);
    FLASH_CS_H();
    SPI_Flash_Wait_Busy();              /* �ȴ�������� */
    SPI_FLASH_Write_Disable();
}

/*
 * SPI_FLASH_Write_Enable - дʹ�ܣ����� WREN ָ�
 * ��ÿ��д����������ǰ�������
 */
void SPI_FLASH_Write_Enable(void)
{
    FLASH_CS_L();
    SPI2_ReadWriteByte(W25X_WriteEnable);
    FLASH_CS_H();
}

/*
 * SPI_FLASH_Write_Disable - д��ֹ������ WRDI ָ�
 */
void SPI_FLASH_Write_Disable(void)
{
    FLASH_CS_L();
    SPI2_ReadWriteByte(W25X_WriteDisable);
    FLASH_CS_H();
}

/*
 * SPI_Flash_ReadID - ������/�豸 ID��˫�ֽڣ�
 * ����ֵ: (����ID << 8) | �豸ID
 */
u16 SPI_Flash_ReadID(void)
{
    u16 temp;

    FLASH_CS_L();
    SPI2_ReadWriteByte(W25X_ManufactDeviceID);  /* ������/�豸 ID ָ�� */
    SPI2_ReadWriteByte(0x00);                   /* 3�ֽڵ�ַ��ȫ0�� */
    SPI2_ReadWriteByte(0x00);
    SPI2_ReadWriteByte(0x00);
    temp  = (u16)SPI2_ReadWriteByte(0xFF) << 8; /* ������ ID */
    temp |= SPI2_ReadWriteByte(0xFF);           /* ���豸 ID */
    FLASH_CS_H();

    return temp;
}

/*
 * SPI_Flash_ReadJEDECID - �� JEDEC ID�����ֽڣ�
 * ����ֵ: ����ID(��8λ) | �ڴ�����(��8λ) | ����(��8λ)
 */
u32 SPI_Flash_ReadJEDECID(void)
{
    u32 id;

    FLASH_CS_L();
    SPI2_ReadWriteByte(W25X_JedecDeviceID);     /* �� JEDEC ID ָ�� */
    id  = (u32)SPI2_ReadWriteByte(0xFF) << 16;  /* ���� ID */
    id |= (u32)SPI2_ReadWriteByte(0xFF) << 8;   /* �ڴ����� */
    id |= (u32)SPI2_ReadWriteByte(0xFF);        /* ���� */
    FLASH_CS_H();

    return id;
}

/*
 * SPI_Flash_Wait_Busy - �ȴ� Flash æ״̬����
 * ��ѯ״̬�Ĵ����� BUSY λ��bit0����ֱ��Ϊ0
 */
void SPI_Flash_Wait_Busy(void)
{
    u32 timeout = SPI_FLASH_BUSY_TIMEOUT;

    while ((SPI_Flash_ReadSR() & 0x01U) != 0U)
    {
        if (timeout-- == 0U)
        {
            g_spiFlashTimeout = 1U;
            break;
        }
    }
}


/*
 * SPI_Flash_Read - �� Flash ��ȡ����
 * pBuffer:    ���������
 * ReadAddr:   ��ȡ��ʼ��ַ��0~�������-1��
 * NumByteToRead: Ҫ��ȡ���ֽ���
 */
void SPI_Flash_Read(u8 *pBuffer, u32 ReadAddr, u16 NumByteToRead)
{
    if (pBuffer == 0)
        return;

    NumByteToRead = SPI_Flash_ClipLength(ReadAddr, NumByteToRead);
    if (NumByteToRead == 0)
        return;

    FLASH_CS_L();
    SPI2_ReadWriteByte(W25X_ReadData);                      /* ������ָ�� */
    SPI2_ReadWriteByte((u8)(ReadAddr >> 16));               /* ��ַ��8λ */
    SPI2_ReadWriteByte((u8)(ReadAddr >> 8));                /* ��ַ��8λ */
    SPI2_ReadWriteByte((u8)ReadAddr);                       /* ��ַ��8λ */
    SPI2_ReadBuf(pBuffer, NumByteToRead);                   /* ������ȡ���� */
    FLASH_CS_H();
}

/*
 * SPI_Flash_Write_Page - дһҳ���ݣ���� 256 �ֽڣ�
 * pBuffer:       ����Դ������
 * WriteAddr:     д����ʼ��ַ������ҳ�ڶ��룩
 * NumByteToWrite: Ҫд����ֽ�����������ҳʣ��ռ䣩
 */
void SPI_Flash_Write_Page(const u8 *pBuffer, u32 WriteAddr, u16 NumByteToWrite)
{
    u16 pageRemain;

    if (pBuffer == 0)
        return;

    NumByteToWrite = SPI_Flash_ClipLength(WriteAddr, NumByteToWrite);
    if (NumByteToWrite == 0)
        return;

    /* ���㵱ǰҳ��ʣ��ռ� */
    pageRemain = (u16)(FLASH_PAGE_SIZE - (WriteAddr & (FLASH_PAGE_SIZE - 1UL)));
    if (NumByteToWrite > pageRemain)
        NumByteToWrite = pageRemain;    /* ����ҳ */

    SPI_FLASH_Write_Enable();           /* дʹ�� */
    FLASH_CS_L();
    SPI2_ReadWriteByte(W25X_PageProgram);   /* ҳ���ָ�� */
    SPI2_ReadWriteByte((u8)(WriteAddr >> 16));  /* ��ַ��8λ */
    SPI2_ReadWriteByte((u8)(WriteAddr >> 8));   /* ��ַ��8λ */
    SPI2_ReadWriteByte((u8)WriteAddr);          /* ��ַ��8λ */
    SPI2_WriteBuf(pBuffer, NumByteToWrite);     /* ����д������ */
    FLASH_CS_H();
    SPI_Flash_Wait_Busy();              /* �ȴ������� */
    SPI_FLASH_Write_Disable();
}

/*
 * SPI_Flash_Write_NoCheck - ����д���ҳ���ݣ�������Ƿ���Ҫ������
 * ����Ŀ���ַ�Ѳ�����ȫ0xFF����ֱ��д��
 */
void SPI_Flash_Write_NoCheck(const u8 *pBuffer, u32 WriteAddr, u16 NumByteToWrite)
{
    u16 pageRemain;

    if (pBuffer == 0)
        return;

    NumByteToWrite = SPI_Flash_ClipLength(WriteAddr, NumByteToWrite);
    if (NumByteToWrite == 0)
        return;

    pageRemain = (u16)(FLASH_PAGE_SIZE - (WriteAddr % FLASH_PAGE_SIZE));
    if (NumByteToWrite <= pageRemain)
        pageRemain = NumByteToWrite;

    while (1)
    {
        SPI_Flash_Write_Page(pBuffer, WriteAddr, pageRemain);
        if (NumByteToWrite == pageRemain)
            break;                      /* д���˳� */

        pBuffer += pageRemain;
        WriteAddr += pageRemain;
        NumByteToWrite -= pageRemain;

        if (NumByteToWrite > FLASH_PAGE_SIZE)
            pageRemain = FLASH_PAGE_SIZE;
        else
            pageRemain = NumByteToWrite;
    }
}

/*
 * SPI_Flash_Write - д�����ݣ��������������-��-д������
 * �Զ�����������д��Ŀ����������ȫ0xFF���Ȳ�����д��
 */
void SPI_Flash_Write(const u8 *pBuffer, u32 WriteAddr, u16 NumByteToWrite)
{
    u32 secpos;
    u32 sectorAddr;
    u16 secoff;
    u16 secremain;
    u16 i;

    if (pBuffer == 0)
        return;

    NumByteToWrite = SPI_Flash_ClipLength(WriteAddr, NumByteToWrite);
    if (NumByteToWrite == 0)
        return;

    secpos    = WriteAddr / FLASH_SECTOR_SIZE;      /* ��ʼ������ */
    secoff    = (u16)(WriteAddr % FLASH_SECTOR_SIZE);/* ������ƫ�� */
    secremain = (u16)(FLASH_SECTOR_SIZE - secoff);  /* ��ǰ����ʣ��ռ� */
    if (NumByteToWrite <= secremain)
        secremain = NumByteToWrite;

    while (1)
    {
        sectorAddr = secpos * FLASH_SECTOR_SIZE;
        /* ��ȡ���������������� */
        SPI_Flash_Read(SPI_FLASH_BUFFER, sectorAddr, FLASH_SECTOR_SIZE);

        /* ���Ŀ�������Ƿ�����ȫ0xFF����������� */
        for (i = 0; i < secremain; i++)
        {
            if (SPI_FLASH_BUFFER[secoff + i] != 0xFF)
                break;
        }

        if (i < secremain)
        {
            /* ��Ҫ�������Ȳ����������ٸ��»��������ݣ����д�� */
            SPI_Flash_Erase_Sector(secpos);
            for (i = 0; i < secremain; i++)
                SPI_FLASH_BUFFER[secoff + i] = pBuffer[i];

            SPI_Flash_Write_NoCheck(SPI_FLASH_BUFFER, sectorAddr, FLASH_SECTOR_SIZE);
        }
        else
        {
            /* Ŀ���������ǿգ�0xFF����ֱ��д�� */
            SPI_Flash_Write_NoCheck(pBuffer, WriteAddr, secremain);
        }

        if (NumByteToWrite == secremain)
            break;          /* ȫ��д�� */

        secpos++;           /* ������һ������ */
        secoff = 0;
        pBuffer += secremain;
        WriteAddr += secremain;
        NumByteToWrite -= secremain;

        if (NumByteToWrite > FLASH_SECTOR_SIZE)
            secremain = FLASH_SECTOR_SIZE;
        else
            secremain = NumByteToWrite;
    }
}

/*
 * SPI_Flash_Erase_Chip - ��Ƭ����
 * �����д洢��Ԫ����Ϊ 0xFF
 * ��ʱ�ϳ���ͨ�����룩
 */
void SPI_Flash_Erase_Chip(void)
{
    SPI_FLASH_Write_Enable();
    SPI_Flash_Wait_Busy();

    FLASH_CS_L();
    SPI2_ReadWriteByte(W25X_ChipErase);     /* ��Ƭ����ָ�� */
    FLASH_CS_H();

    SPI_Flash_Wait_Busy();
    SPI_FLASH_Write_Disable();
}

/*
 * SPI_Flash_Erase_Sector - ����ָ��������4KB��
 * sectorIndex: ���������ţ�0~���������-1��
 */
void SPI_Flash_Erase_Sector(u32 sectorIndex)
{
    u32 addr;

    addr = sectorIndex * FLASH_SECTOR_SIZE;     /* ����������ʼ��ַ */

    SPI_FLASH_Write_Enable();
    SPI_Flash_Wait_Busy();

    FLASH_CS_L();
    SPI2_ReadWriteByte(W25X_SectorErase);       /* ��������ָ�� */
    SPI2_ReadWriteByte((u8)(addr >> 16));
    SPI2_ReadWriteByte((u8)(addr >> 8));
    SPI2_ReadWriteByte((u8)addr);
    FLASH_CS_H();

    SPI_Flash_Wait_Busy();
    SPI_FLASH_Write_Disable();
}

/*
 * SPI_Flash_Erase_Block - ����ָ���飨64KB��
 * blockIndex: �������ţ�0~������-1��
 */
void SPI_Flash_Erase_Block(u32 blockIndex)
{
    u32 addr;

    addr = blockIndex * FLASH_BLOCK_SIZE;       /* �������ʼ��ַ */

    SPI_FLASH_Write_Enable();
    SPI_Flash_Wait_Busy();

    FLASH_CS_L();
    SPI2_ReadWriteByte(W25X_BlockErase);        /* �����ָ�� */
    SPI2_ReadWriteByte((u8)(addr >> 16));
    SPI2_ReadWriteByte((u8)(addr >> 8));
    SPI2_ReadWriteByte((u8)addr);
    FLASH_CS_H();

    SPI_Flash_Wait_Busy();
    SPI_FLASH_Write_Disable();
}

/*
 * SPI_Flash_PowerDown - �������ģʽ
 * ����͹���״̬����Ҫ���� WAKEUP �ָ�
 */
void SPI_Flash_PowerDown(void)
{
    FLASH_CS_L();
    SPI2_ReadWriteByte(W25X_PowerDown);         /* ����ָ�� */
    FLASH_CS_H();
    delay_us(3);
}

/*
 * SPI_Flash_WAKEUP - �ӵ���ģʽ����
 */
void SPI_Flash_WAKEUP(void)
{
    FLASH_CS_L();
    SPI2_ReadWriteByte(W25X_ReleasePowerDown);  /* �ͷŵ���ָ�� */
    FLASH_CS_H();
    delay_us(3);
}

/*
 * SPI_Flash_WP_Set - ����д��������״̬
 * enable: 1=д����ʹ��, 0=д������ֹ
 */
void SPI_Flash_WP_Set(u8 enable)
{
    if (enable)
        FLASH_WP_L();       /* WP=0 ʹ��Ӳ��д���� */
    else
        FLASH_WP_H();       /* WP=1 ��ֹӲ��д���� */
}

/*====================================================================
 * SPI2 DMA ���亯��
 * STM32F103 ����ӳ��:
 *   DMA1_Channel4: SPI2_RX������ -> �ڴ棩
 *   DMA1_Channel5: SPI2_TX���ڴ� -> ���裩
 * SPI ���谴 8 λ����֡��������� DMA Ҳ�������ó� 8 λ���ȡ�
 *
 * ����ͬʱ�ṩ����ӿڣ�
 *   1. Start + IsFinished: �������ӿڣ�����ƹ�һ���/��ˮ�߶�ȡ��
 *   2. SPI_Flash_Read_DMA / SPI_Flash_Write_Page_DMA: ���ݾɴ���������ӿڡ�
 *====================================================================*/

#define SPI_FLASH_DMA_RX_CH     DMA1_Channel4
#define SPI_FLASH_DMA_TX_CH     DMA1_Channel5
#define SPI_FLASH_DMA_TC_FLAGS  (DMA_ISR_TCIF4 | DMA_ISR_TCIF5)
#define SPI_FLASH_DMA_CLR_FLAGS (DMA_IFCR_CTCIF4 | DMA_IFCR_CTCIF5)

#define SPI_FLASH_DMA_IDLE      0U
#define SPI_FLASH_DMA_READ      1U
#define SPI_FLASH_DMA_WRITE_DMA 2U
#define SPI_FLASH_DMA_WRITE_BUSY 3U

static u8 s_flashDmaTxDummy = 0xFFU;
static u8 s_flashDmaRxDummy;
static volatile u8 s_flashDmaState = SPI_FLASH_DMA_IDLE;

/*
 * SPI_Flash_DMA_Init - ��ʼ�� DMA1_Ch4/Ch5 ���� SPI2 ����
 * ����ֻ���ù̶������ַ�����巽�򡢵�ַ�����ͳ�����ÿ�δ���ǰ���á�
 */
void SPI_Flash_DMA_Init(void)
{
    /* ʹ�� DMA1 ʱ�ӣ�AHB�� */
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    delay_ms(1);

    SPI_FLASH_DMA_RX_CH->CCR = 0;
    SPI_FLASH_DMA_TX_CH->CCR = 0;
    SPI_FLASH_DMA_RX_CH->CPAR = (u32)&SPI2->DR;
    SPI_FLASH_DMA_TX_CH->CPAR = (u32)&SPI2->DR;
    DMA1->IFCR = SPI_FLASH_DMA_CLR_FLAGS;
    s_flashDmaState = SPI_FLASH_DMA_IDLE;
}

/* ��� RX/TX ���� DMA ͨ���Ƿ���ɡ� */
static u8 SPI_Flash_DMA_TransferDone(void)
{
    return ((DMA1->ISR & SPI_FLASH_DMA_TC_FLAGS) == SPI_FLASH_DMA_TC_FLAGS) ? 1U : 0U;
}

/* DMA ���ݽ׶���ɺ�Ĺ�����β�� */
static void SPI_Flash_DMA_StopTransfer(void)
{
    while((SPI2->SR & SPI_SR_BSY) != 0)
    {
    }

    SPI_FLASH_DMA_RX_CH->CCR &= (u16)~DMA_CCR1_EN;
    SPI_FLASH_DMA_TX_CH->CCR &= (u16)~DMA_CCR1_EN;
    SPI2->CR2 &= (u16)~(SPI_CR2_RXDMAEN | SPI_CR2_TXDMAEN);
    DMA1->IFCR = SPI_FLASH_DMA_CLR_FLAGS;

    /* ������ܲ����Ľ������ݣ����������ѯ����������ֽڡ� */
    if ((SPI2->SR & SPI_SR_RXNE) != 0)
        (void)*(__IO u8 *)&SPI2->DR;
    (void)SPI2->SR;
}

/* ���ò����� SPI2 �� RX/TX DMA ���ݽ׶Ρ� */
static void SPI_Flash_DMA_StartTransfer(u8 *rxBuf,
                                        const u8 *txBuf,
                                        u16 len,
                                        u8 rxInc,
                                        u8 txInc)
{
    SPI_FLASH_DMA_RX_CH->CCR &= (u16)~DMA_CCR1_EN;
    SPI_FLASH_DMA_TX_CH->CCR &= (u16)~DMA_CCR1_EN;
    DMA1->IFCR = SPI_FLASH_DMA_CLR_FLAGS;

    SPI_FLASH_DMA_RX_CH->CMAR = (u32)rxBuf;
    SPI_FLASH_DMA_RX_CH->CNDTR = len;
    SPI_FLASH_DMA_RX_CH->CCR = DMA_CCR1_PL_1 | (rxInc ? DMA_CCR1_MINC : 0U);

    SPI_FLASH_DMA_TX_CH->CMAR = (u32)txBuf;
    SPI_FLASH_DMA_TX_CH->CNDTR = len;
    SPI_FLASH_DMA_TX_CH->CCR = DMA_CCR1_DIR | DMA_CCR1_PL_1 | (txInc ? DMA_CCR1_MINC : 0U);

    SPI2->CR2 |= SPI_CR2_RXDMAEN | SPI_CR2_TXDMAEN;

    /* �ȿ� RX���� TX�������һ�������ֽڶ�ʧ�� */
    SPI_FLASH_DMA_RX_CH->CCR |= DMA_CCR1_EN;
    SPI_FLASH_DMA_TX_CH->CCR |= DMA_CCR1_EN;
}

/*
 * SPI_Flash_Read_DMA_Start - ����һ�� DMA ������
 * ���� 1 ��ʾ�����ɹ������� 0 ��ʾ��������� DMA ��æ��
 */
u8 SPI_Flash_Read_DMA_Start(u8 *pBuffer, u32 ReadAddr, u16 NumByteToRead)
{
    if(pBuffer == 0) return 0U;
    if(s_flashDmaState != SPI_FLASH_DMA_IDLE) return 0U;

    NumByteToRead = SPI_Flash_ClipLength(ReadAddr, NumByteToRead);
    if(NumByteToRead == 0) return 0U;

    FLASH_CS_L();
    SPI2_ReadWriteByte(W25X_ReadData);
    SPI2_ReadWriteByte((u8)(ReadAddr >> 16));
    SPI2_ReadWriteByte((u8)(ReadAddr >> 8));
    SPI2_ReadWriteByte((u8)ReadAddr);

    s_flashDmaState = SPI_FLASH_DMA_READ;
    SPI_Flash_DMA_StartTransfer(pBuffer, &s_flashDmaTxDummy, NumByteToRead, 1U, 0U);
    return 1U;
}

/*
 * SPI_Flash_Read_DMA_IsFinished - ��ѯ DMA ���Ƿ����
 * ���� 1 ��ʾ�Ѿ���ɲ������β������ 0 ��ʾ���ڴ��䡣
 */
u8 SPI_Flash_Read_DMA_IsFinished(void)
{
    if(s_flashDmaState != SPI_FLASH_DMA_READ)
        return 1U;

    if(!SPI_Flash_DMA_TransferDone())
        return 0U;

    SPI_Flash_DMA_StopTransfer();
    FLASH_CS_H();
    s_flashDmaState = SPI_FLASH_DMA_IDLE;
    return 1U;
}

/*
 * SPI_Flash_Read_DMA - DMA ��ʽ��ȡ Flash ���ݣ��������ݽӿڣ�
 */
void SPI_Flash_Read_DMA(u8 *pBuffer, u32 ReadAddr, u16 NumByteToRead)
{
    if(SPI_Flash_Read_DMA_Start(pBuffer, ReadAddr, NumByteToRead) == 0U)
        return;

    while(SPI_Flash_Read_DMA_IsFinished() == 0U)
    {
    }
}

/*
 * SPI_Flash_Write_Page_DMA_Start - ����һ��ҳ�� DMA д����
 * ���� 1 ��ʾ�����ɹ������� 0 ��ʾ��������� DMA ��æ��
 * ע�⣺�ú���ֻ����дͬһҳ�ڵ����ݣ�����ҳβ���Զ��ü���
 */
u8 SPI_Flash_Write_Page_DMA_Start(const u8 *pBuffer, u32 WriteAddr, u16 NumByteToWrite)
{
    u16 pageRemain;

    if(pBuffer == 0) return 0U;
    if(s_flashDmaState != SPI_FLASH_DMA_IDLE) return 0U;

    NumByteToWrite = SPI_Flash_ClipLength(WriteAddr, NumByteToWrite);
    if(NumByteToWrite == 0) return 0U;

    pageRemain = (u16)(FLASH_PAGE_SIZE - (WriteAddr & (FLASH_PAGE_SIZE - 1UL)));
    if(NumByteToWrite > pageRemain) NumByteToWrite = pageRemain;

    SPI_FLASH_Write_Enable();

    FLASH_CS_L();
    SPI2_ReadWriteByte(W25X_PageProgram);
    SPI2_ReadWriteByte((u8)(WriteAddr >> 16));
    SPI2_ReadWriteByte((u8)(WriteAddr >> 8));
    SPI2_ReadWriteByte((u8)WriteAddr);

    s_flashDmaState = SPI_FLASH_DMA_WRITE_DMA;
    SPI_Flash_DMA_StartTransfer(&s_flashDmaRxDummy, pBuffer, NumByteToWrite, 0U, 1U);
    return 1U;
}

/*
 * SPI_Flash_Write_Page_DMA_IsFinished - ��ѯ DMA ҳд�Ƿ����
 * ���� 1 ��ʾ DMA ���ݽ׶κ� Flash �ڲ�ҳ��̶�����ɣ����� 0 ��ʾ����æ��
 */
u8 SPI_Flash_Write_Page_DMA_IsFinished(void)
{
    if(s_flashDmaState == SPI_FLASH_DMA_IDLE)
        return 1U;

    if(s_flashDmaState == SPI_FLASH_DMA_WRITE_DMA)
    {
        if(!SPI_Flash_DMA_TransferDone())
            return 0U;

        SPI_Flash_DMA_StopTransfer();
        FLASH_CS_H();
        s_flashDmaState = SPI_FLASH_DMA_WRITE_BUSY;
    }

    if(s_flashDmaState == SPI_FLASH_DMA_WRITE_BUSY)
    {
        if((SPI_Flash_ReadSR() & 0x01U) != 0U)
            return 0U;

        SPI_FLASH_Write_Disable();
        s_flashDmaState = SPI_FLASH_DMA_IDLE;
        return 1U;
    }

    return 0U;
}

/*
 * SPI_Flash_Write_Page_DMA - DMA ��ʽдһҳ���ݣ��������ݽӿڣ�
 */
void SPI_Flash_Write_Page_DMA(const u8 *pBuffer, u32 WriteAddr, u16 NumByteToWrite)
{
    if(SPI_Flash_Write_Page_DMA_Start(pBuffer, WriteAddr, NumByteToWrite) == 0U)
        return;

    while(SPI_Flash_Write_Page_DMA_IsFinished() == 0U)
    {
    }
}

/*
 * SPI_Flash_DebugDemo - SPI Flash ����ʾ��
 * ���� main() ��� USART1 ��ʼ�����ֶ����á�
 * ʾ�����̣�
 * 1. ��ȡоƬ ID
 * 2. ����������
 * 3. д���������
 * 4. �ض����Ƚ�
 * 5. ��Ƭ��������֤
 * ÿһ������ͨ�� USART1 ��ӡ������Ϣ��
 */
void SPI_Flash_DebugDemo(void)
{
    static const u8 txBuf[16] =
    {
        0x46, 0x4C, 0x41, 0x53, 0x48, 0x5F, 0x44, 0x45,
        0x4D, 0x4F, 0x5F, 0x31, 0x30, 0x33, 0x56, 0x45
    };
    u8 rxBuf[sizeof(txBuf)];
    u16 flashId;
    u8 compareOk;
    u16 i;

    memset(rxBuf, 0, sizeof(rxBuf));
    flashId = 0;
    compareOk = 0;

    /* ����1: ��ʼ�� SPI Flash */
    printf("��Flash���ԡ���ʼ SPI Flash ����...\r\n");
    SPI_Flash_Init();
    printf("��Flash���ԡ���ʼ�����\r\n");

    /* ����2: ��ȡоƬ ID */
    flashId = SPI_Flash_ReadID();
    if (flashId == 0x0000U || flashId == 0xFFFFU)
        flashId = (u16)(SPI_Flash_ReadJEDECID() & 0xFFFFU);
    printf("��Flash���ԡ�оƬ ID = 0x%04X\r\n", flashId);

    /* ����3: ������������4KB�� */
    printf("��Flash���ԡ��������� 0 ...\r\n");
    SPI_Flash_Erase_Sector(0);
    printf("��Flash���ԡ������������\r\n");

    /* ����4: д�� 16 �ֽڲ������ݵ���ַ 0x00000000 */
    printf("��Flash���ԡ�д�����ݵ���ַ 0x00000000: ");
    for (i = 0; i < sizeof(txBuf); i++)
        printf("%02X ", txBuf[i]);
    printf("\r\n");
    SPI_Flash_Write(txBuf, 0x00000000UL, sizeof(txBuf));
    printf("��Flash���ԡ�д�����\r\n");

    /* ����5: �ӵ�ַ 0x00000000 �ض����� */
    memset(rxBuf, 0, sizeof(rxBuf));
    SPI_Flash_Read(rxBuf, 0x00000000UL, sizeof(rxBuf));
    printf("��Flash���ԡ��ض�����: ");
    for (i = 0; i < sizeof(rxBuf); i++)
        printf("%02X ", rxBuf[i]);
    printf("\r\n");

    /* ����6: �Ƚ�д��ͻض����� */
    if (memcmp(txBuf, rxBuf, sizeof(txBuf)) == 0)
    {
        compareOk = 1;
        printf("��Flash���ԡ��ȽϽ��: һ�£���д����ͨ����\r\n");
    }
    else
    {
        printf("��Flash���ԡ��ȽϽ��: ��һ�£���д����ʧ�ܣ�\r\n");
    }

    /* ����7: ��Ƭ��������֤ */
    printf("��Flash���ԡ���ʼ��Ƭ����...\r\n");
    SPI_Flash_Erase_Chip();
    printf("��Flash���ԡ���Ƭ�������\r\n");

    memset(rxBuf, 0, sizeof(rxBuf));
    SPI_Flash_Read(rxBuf, 0x00000000UL, sizeof(rxBuf));
    printf("��Flash���ԡ��������ȡ 0x00000000: ");
    for (i = 0; i < sizeof(rxBuf); i++)
        printf("%02X ", rxBuf[i]);
    printf("\r\n");

    /* ���������Ƿ�ȫΪ 0xFF */
    compareOk = 1;
    for (i = 0; i < sizeof(rxBuf); i++)
    {
        if (rxBuf[i] != 0xFF)
        {
            compareOk = 0;
            break;
        }
    }
    if (compareOk != 0U)
        printf("��Flash���ԡ�������֤ͨ����ȫ��Ϊ 0xFF\r\n");
    else
        printf("��Flash���ԡ�������֤ʧ�ܣ����ڷ� 0xFF ����\r\n");

    printf("��Flash���ԡ�����\r\n");

    (void)flashId;
    (void)compareOk;
}

/*
 * SPI_Flash_DebugDemo_DMA - SPI Flash DMA ��д���Բ���
 *
 * �������̣�
 *   1. ��ʼ�� Flash �� DMA1
 *   2. ����ָ�������Ĳ�������
 *   3. �� DMA ��ʽ��ҳд�루ÿҳʹ�ò�ͬ��α���������䣩
 *   4. �� DMA ��ʽ��ҳ��ȡ��У������
 *   5. ���²���������ѯ��ʽ��ҳд�루ʹ����һ�鲻ͬ�����ݣ�
 *   6. ����ѯ��ʽ��ҳ��ȡ��У��
 *   7. ��ӡ��ʱ���ٶȶԱȱ���
 *
 * ʹ�� DWT ���ݹ۲������ٵ�Ԫ�����ڼ�������CYCCNT�����и߾��ȼ�ʱ��
 * ϵͳ��Ƶ 72 MHz ʱ��ÿ����Լ 13.89 ns��
 *
 * ע�⣺�ú������ƻ� Flash ǰ N �����������ݣ������ڵ��ԡ�
 */
void SPI_Flash_DebugDemo_DMA(void)
{
    /* ============================================================
     *  ���Բ������ɵ�����
     *  TEST_SECTOR_CNT �� 4 KB = �ܲ���������
     *  Ĭ�� 10 ���� = 40 KB = 160 ҳ����˲��Գ������ִ��ʱ��
     * ============================================================ */
    #define DMA_TEST_SECTOR_CNT     10U
    #define DMA_TEST_PAGE_CNT       (DMA_TEST_SECTOR_CNT * (FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE))
    #define DMA_TEST_TOTAL_BYTES    (DMA_TEST_SECTOR_CNT * FLASH_SECTOR_SIZE)
    #define DMA_TEST_START_ADDR     0UL

    u32        i, j;
    u32        startCyc, endCyc;
    u32        cycDmaWrite, cycDmaRead, cycPollWrite, cycPollRead;
    u8         pageBuf[FLASH_PAGE_SIZE];
    u8         verifyBuf[FLASH_PAGE_SIZE];
    u8         dmaDataOk, pollDataOk;
    u32        dmaWriteKBps, dmaReadKBps, pollWriteKBps, pollReadKBps;

    /* ============================================================
     *  ��ʼ�� DWT ���ڼ�����
     *  DEMCR[24] = TRCENA (Trace Enable)
     *  DWT_CTRL[0] = CYCCNTENA (Cycle Counter Enable)
     * ============================================================ */
    *(__IO u32 *)0xE000EDFC |= (1UL << 24);     /* DEMCR |= TRCENA */
    *(__IO u32 *)0xE0001000 |= (1UL << 0);      /* DWT_CTRL |= CYCCNTENA */
    *(__IO u32 *)0xE0001004  = 0UL;             /* DWT_CYCCNT = 0 */

    dmaDataOk   = 1U;
    pollDataOk  = 1U;

    printf("\r\n========== SPI Flash DMA ���Բ��� ==========\r\n");

    /* ============================================================
     *  1. ��ʼ�� Flash & DMA
     * ============================================================ */
    SPI_Flash_Init();
    SPI_Flash_DMA_Init();
    SPI2_SetSpeed(SPI_SPEED_2);     /* 18 MHz �� Flash ���֧�� 25~50 MHz��ѡ����Ƶ */

    printf("Flash ID = 0x%04X\r\n", SPI_FLASH_TYPE);
    printf("����������: %lu ���� = %lu KB = %lu ҳ\r\n",
           (u32)DMA_TEST_SECTOR_CNT,
           (u32)DMA_TEST_TOTAL_BYTES / 1024UL,
           (u32)DMA_TEST_PAGE_CNT);

    /* ============================================================
     *  2. ������������
     * ============================================================ */
    printf("\r\n[������������]\r\n");
    for (i = 0U; i < DMA_TEST_SECTOR_CNT; i++)
    {
        SPI_Flash_Erase_Sector(i);
    }
    printf("  �Ѳ��� %lu ������\r\n", (u32)DMA_TEST_SECTOR_CNT);

    /* ============================================================
     *  3. DMA ��ҳд�� + ��ʱ
     *     ÿҳ��䲻ͬ��α������ݣ�pageBuf[j] = (pageIdx * 256 + j) ^ 0xA5
     * ============================================================ */
    printf("\r\n--- [1] DMA д�� (%lu KB) ---\r\n", (u32)DMA_TEST_TOTAL_BYTES / 1024UL);

    startCyc = *(__IO u32 *)0xE0001004;                 /* DWT_CYCCNT */
    for (i = 0U; i < DMA_TEST_PAGE_CNT; i++)
    {
        for (j = 0U; j < FLASH_PAGE_SIZE; j++)
            pageBuf[j] = (u8)((i * FLASH_PAGE_SIZE + j) ^ 0xA5U);

        SPI_Flash_Write_Page_DMA(pageBuf,
                                 DMA_TEST_START_ADDR + i * FLASH_PAGE_SIZE,
                                 FLASH_PAGE_SIZE);
    }
    endCyc = *(__IO u32 *)0xE0001004;
    cycDmaWrite = endCyc - startCyc;

    printf("  DMA д�����, ��ʱ %lu ����\r\n", cycDmaWrite);

    /* ============================================================
     *  4. DMA ��ҳ��ȡ + У�� + ��ʱ
     * ============================================================ */
    printf("--- [2] DMA ��ȡ + У�� ---\r\n");

    startCyc = *(__IO u32 *)0xE0001004;
    for (i = 0U; i < DMA_TEST_PAGE_CNT; i++)
    {
        SPI_Flash_Read_DMA(verifyBuf,
                           DMA_TEST_START_ADDR + i * FLASH_PAGE_SIZE,
                           FLASH_PAGE_SIZE);

        for (j = 0U; j < FLASH_PAGE_SIZE; j++)
        {
            u8 exp = (u8)((i * FLASH_PAGE_SIZE + j) ^ 0xA5U);
            if (verifyBuf[j] != exp)
            {
                if (dmaDataOk != 0U)
                {
                    printf("  [DMA] ���ݲ�һ��! ҳ%lu ƫ��%lu: ����0x%02X ʵ��0x%02X\r\n",
                           i, j, exp, verifyBuf[j]);
                }
                dmaDataOk = 0U;
            }
        }
    }
    endCyc = *(__IO u32 *)0xE0001004;
    cycDmaRead = endCyc - startCyc;

    printf("  DMA ��ȡ���, ��ʱ %lu ����\r\n", cycDmaRead);
    printf("  DMA ����У��: %s\r\n", (dmaDataOk != 0U) ? "ͨ��" : "ʧ��");

    /* ============================================================
     *  5. ���²�������������׼����ѯ���ԣ�ʹ����һ�����ݣ�
     * ============================================================ */
    printf("\r\n[���²�����������]\r\n");
    for (i = 0U; i < DMA_TEST_SECTOR_CNT; i++)
    {
        SPI_Flash_Erase_Sector(i);
    }
    printf("  �����²��� %lu ������\r\n", (u32)DMA_TEST_SECTOR_CNT);

    /* ============================================================
     *  6. ��ѯ��ҳд�� + ��ʱ
     *     ʹ�ò�ͬ��α������ӣ�pageBuf[j] = (pageIdx * 256 + j) ^ 0x5A
     * ============================================================ */
    printf("\r\n--- [3] ��ѯд�� (%lu KB) ---\r\n", (u32)DMA_TEST_TOTAL_BYTES / 1024UL);

    startCyc = *(__IO u32 *)0xE0001004;
    for (i = 0U; i < DMA_TEST_PAGE_CNT; i++)
    {
        for (j = 0U; j < FLASH_PAGE_SIZE; j++)
            pageBuf[j] = (u8)((i * FLASH_PAGE_SIZE + j) ^ 0x5AU);

        SPI_Flash_Write_Page(pageBuf,
                             DMA_TEST_START_ADDR + i * FLASH_PAGE_SIZE,
                             FLASH_PAGE_SIZE);
    }
    endCyc = *(__IO u32 *)0xE0001004;
    cycPollWrite = endCyc - startCyc;

    printf("  ��ѯд�����, ��ʱ %lu ����\r\n", cycPollWrite);

    /* ============================================================
     *  7. ��ѯ��ҳ��ȡ + У�� + ��ʱ
     * ============================================================ */
    printf("--- [4] ��ѯ��ȡ + У�� ---\r\n");

    startCyc = *(__IO u32 *)0xE0001004;
    for (i = 0U; i < DMA_TEST_PAGE_CNT; i++)
    {
        SPI_Flash_Read(verifyBuf,
                       DMA_TEST_START_ADDR + i * FLASH_PAGE_SIZE,
                       FLASH_PAGE_SIZE);

        for (j = 0U; j < FLASH_PAGE_SIZE; j++)
        {
            u8 exp = (u8)((i * FLASH_PAGE_SIZE + j) ^ 0x5AU);
            if (verifyBuf[j] != exp)
            {
                if (pollDataOk != 0U)
                {
                    printf("  [��ѯ] ���ݲ�һ��! ҳ%lu ƫ��%lu: ����0x%02X ʵ��0x%02X\r\n",
                           i, j, exp, verifyBuf[j]);
                }
                pollDataOk = 0U;
            }
        }
    }
    endCyc = *(__IO u32 *)0xE0001004;
    cycPollRead = endCyc - startCyc;

    printf("  ��ѯ��ȡ���, ��ʱ %lu ����\r\n", cycPollRead);
    printf("  ��ѯ����У��: %s\r\n", (pollDataOk != 0U) ? "ͨ��" : "ʧ��");

    /* ============================================================
     *  8. �����ٶȲ���ӡ���ܱ���
     *     ��Ƶ 72 MHz �� 1 us = 72 ����
     *     KB/s = (���ֽ��� / 1024) / (��ʱ_us / 1,000,000)
     *           = (���ֽ��� * 1,000,000) / (��ʱ_us * 1024)
     *           = (���ֽ��� * 1,000,000) / ((��ʱ����/72) * 1024)
     *           = (���ֽ��� * 1,000,000 * 72) / (��ʱ���� * 1024)
     * ============================================================ */
    #define CYCLES_PER_US   72UL
    #define US_PER_SEC      1000000UL

    dmaWriteKBps  = (u32)DMA_TEST_TOTAL_BYTES * US_PER_SEC / (cycDmaWrite  / CYCLES_PER_US) / 1024UL;
    dmaReadKBps   = (u32)DMA_TEST_TOTAL_BYTES * US_PER_SEC / (cycDmaRead   / CYCLES_PER_US) / 1024UL;
    pollWriteKBps = (u32)DMA_TEST_TOTAL_BYTES * US_PER_SEC / (cycPollWrite / CYCLES_PER_US) / 1024UL;
    pollReadKBps  = (u32)DMA_TEST_TOTAL_BYTES * US_PER_SEC / (cycPollRead  / CYCLES_PER_US) / 1024UL;

    printf("\r\n============================================\r\n");
    printf("  SPI Flash DMA / ��ѯ �ٶȶԱ�  (%lu KB)\r\n",
           (u32)DMA_TEST_TOTAL_BYTES / 1024UL);
    printf("============================================\r\n");
    printf("  ����һ����        DMA=%s  ��ѯ=%s\r\n",
           (dmaDataOk  != 0U) ? "OK" : "FAIL",
           (pollDataOk != 0U) ? "OK" : "FAIL");
    printf("--------------------------------------------\r\n");
    printf("  ���䷽ʽ      |  ��ʱ(us)    |  �ٶ�(KB/s)\r\n");
    printf("--------------------------------------------\r\n");
    printf("  DMA д��      | %12lu | %11lu\r\n",
           cycDmaWrite  / CYCLES_PER_US, dmaWriteKBps);
    printf("  DMA ��ȡ      | %12lu | %11lu\r\n",
           cycDmaRead   / CYCLES_PER_US, dmaReadKBps);
    printf("  ��ѯд��      | %12lu | %11lu\r\n",
           cycPollWrite / CYCLES_PER_US, pollWriteKBps);
    printf("  ��ѯ��ȡ      | %12lu | %11lu\r\n",
           cycPollRead  / CYCLES_PER_US, pollReadKBps);
    printf("--------------------------------------------\r\n");

    /* ============================================================
     *  9. ������������ �� ����д������ݵ��������ָ�ȫ 0xFF
     * ============================================================ */
    printf("\r\n[������������]\r\n");
    for (i = 0U; i < DMA_TEST_SECTOR_CNT; i++)
    {
        SPI_Flash_Erase_Sector(i);
    }
    printf("  �Ѳ��� %lu ������������Flash �ѻָ�ȫ 0xFF\r\n", (u32)DMA_TEST_SECTOR_CNT);
    printf("\r\n========== ���Խ��� ==========\r\n");
}

/*
 * SPI_Flash_Erase_Auto �� �Զ���Ⲣ������ʹ�õ� Flash ����
 *
 * ��ⷽ����
 *   ���ж� Flash ��������sector 0���Ƿ������߰��������ݡ�
 *   �� �� ���� offline_package_index_t ��������ֻ������ʹ������������2����
 *   �� �� ȫƬɨ�裬��������ȡ�ж��Ƿ�Ϊȫ 0xFF��ֻ�����ǿ�����������3����
 *
 * �Ӻ���˵����
 *   SPI_Flash_IsSectorEmpty(sectorIndex) �� �ж�ָ�������Ƿ�Ϊȫ 0xFF
 *   SPI_Flash_EraseDirtySectors �� ȫƬɨ�跽�����������������ݵ�����
 *   SPI_Flash_EraseByOfflineIndex �� ʹ�����߰���������׼����
 *
 * ����ֵ: ʵ�ʲ�������������
 */
static u8 SPI_Flash_IsSectorEmpty(u32 sectorIndex)
{
    u8  buf[8];
    u32 addr;
    u16 i;

    addr = sectorIndex * FLASH_SECTOR_SIZE;
    SPI_Flash_Read(buf, addr, sizeof(buf));

    for (i = 0U; i < sizeof(buf); i++)
    {
        if (buf[i] != 0xFFU)
            return 0U;      /* ���ַ� 0xFF �ֽڣ������ǿ� */
    }
    return 1U;              /* ǰ 8 �ֽ�ȫΪ 0xFF����Ϊ����Ϊ�� */
}

/*
 * SPI_Flash_EraseDirtySectors �� ȫƬɨ�裬�������������ݵ�����������3��
 *
 * �� Flash ȫ�� 1024 ��������4MB / 4KB������������ȡǰ 8 �ֽ��жϣ�
 * ���ַ� 0xFF ��ִ�в�����
 *
 * ����ֵ: ʵ�ʲ�������������
 */
static u32 SPI_Flash_EraseDirtySectors(void)
{
    u32 sectorCount;
    u32 i;
    u32 erased;

    sectorCount = FLASH_CAPACITY / FLASH_SECTOR_SIZE;
    erased = 0U;

    for (i = 0U; i < sectorCount; i++)
    {
        if (!SPI_Flash_IsSectorEmpty(i))
        {
            SPI_Flash_Erase_Sector(i);
            erased++;
        }
    }

    return erased;
}

/*
 * SPI_Flash_EraseByOfflineIndex �� ʹ�����߰���������׼����������2��
 *
 * ��ȡ Flash �������е� offline_package_index_t ��������
 * ����ÿ�� used=1 �� package_state=VALID/DELETED �����߰���
 * ������ռ�õ�������Χ��������
 *
 * ���������������֣�1008 �ֽڣ���
 *   32 ����Ŀ �� Լ 94 �ֽ�/��Ŀ
 *   ÿ����Ŀ�Ľṹ���ο� offLineRecorder.h����
 *     offset 0: used            u8
 *     offset 1: package_state   u8
 *     offset 2: package_index   u16 LE
 *     offset 4: flash_addr      u32 LE
 *     offset 8: total_size      u32 LE
 *
 * ����ֵ: ʵ�ʲ�������������
 */
static u32 SPI_Flash_EraseByOfflineIndex(void)
{
    #define OFFLINE_ENTRY_SIZE      20U     /* ǰ 5 �������ֶε��ܺ� */
    #define OFFLINE_MAX_ENTRIES     32U

    u8                  rawIndex[OFFLINE_MAX_ENTRIES * OFFLINE_ENTRY_SIZE];
    u8                  sectorMap[128];     /* 1024 ����λͼ = 128 �ֽ� */
    u32                 i;
    u32                 erased;
    u16                 entry;
    u32                 secpos;
    u32                 flashAddr;
    u32                 totalSize;

    memset(sectorMap, 0, sizeof(sectorMap));

    /* ��ȡ����������������ǰ 32 ��Ŀ �� 20 �ֽ� = 640 �ֽڣ�*/
    SPI_Flash_Read(rawIndex, 0UL, sizeof(rawIndex));

    /* ����������Ŀ�������ʹ�õ����� */
    for (entry = 0U; entry < OFFLINE_MAX_ENTRIES; entry++)
    {
        u16 offset = entry * OFFLINE_ENTRY_SIZE;
        u8  used   = rawIndex[offset];
        u8  state  = rawIndex[offset + 1U];

        /* ֻ������Ч����ɾ������Ŀ������ EMPTY �� WRITING��*/
        if (used == 0U)
            continue;

        /* ���� flash_addr �� total_size��С����*/
        flashAddr  = (u32)rawIndex[offset + 4U];
        flashAddr |= (u32)rawIndex[offset + 5U] << 8;
        flashAddr |= (u32)rawIndex[offset + 6U] << 16;
        flashAddr |= (u32)rawIndex[offset + 7U] << 24;

        totalSize  = (u32)rawIndex[offset + 8U];
        totalSize |= (u32)rawIndex[offset + 9U] << 8;
        totalSize |= (u32)rawIndex[offset + 10U] << 16;
        totalSize |= (u32)rawIndex[offset + 11U] << 24;

        if (totalSize == 0U)
            continue;

        /* ��Ǹð�ռ�õ��������� */
        for (secpos = flashAddr / FLASH_SECTOR_SIZE;
             secpos <= (flashAddr + totalSize - 1U) / FLASH_SECTOR_SIZE;
             secpos++)
        {
            if (secpos < (FLASH_CAPACITY / FLASH_SECTOR_SIZE))
                sectorMap[secpos / 8U] |= (u8)(1U << (secpos % 8U));
        }
    }

    /* ֻ����λͼ���б�ǵ����� */
    erased = 0U;
    for (i = 0U; i < (FLASH_CAPACITY / FLASH_SECTOR_SIZE); i++)
    {
        if (sectorMap[i / 8U] & (u8)(1U << (i % 8U)))
        {
            SPI_Flash_Erase_Sector(i);
            erased++;
        }
    }

    return erased;
}

/*
 * SPI_Flash_IsValidOfflineIndex �� ��������������Ƿ�������߰���������ʽ
 *
 * ��֤����
 *   1. ÿ����Ŀ�� used �ֶα���Ϊ 0 �� 1�������� 0xFE/0xFF �ȷǷ�ֵ��
 *   2. �� used=1 ʱ��package_state ������ 1~3 ��Χ�ڣ�WRITING/VALID/DELETED��
 *   3. �� used=1 ʱ��package_index ���� < OFFLINE_MAX_PACKAGES (32)
 *   4. ���� 32 ����Ŀ�������� 1 �� used=1�����������������壩
 *   5. used+package_state+package_index �������ֶε��ۼӺͲ���Ϊ 0xFF
 *      ����ֹȫ 0xFF ������������Ϊ��Ч��������
 *
 * ����ֵ: 1=��Ч������, 0=��Ч
 */
static u8 SPI_Flash_IsValidOfflineIndex(void)
{
    #define OFFLINE_ENTRY_SIZE      20U
    #define OFFLINE_MAX_ENTRIES     32U

    u8   rawIndex[OFFLINE_MAX_ENTRIES * OFFLINE_ENTRY_SIZE];
    u16  entry;
    u8   hasValidEntry;
    u8   allZeroCheck;

    SPI_Flash_Read(rawIndex, 0UL, sizeof(rawIndex));

    hasValidEntry = 0U;
    allZeroCheck  = 0U;

    for (entry = 0U; entry < OFFLINE_MAX_ENTRIES; entry++)
    {
        u16 off = entry * OFFLINE_ENTRY_SIZE;
        u8  used          = rawIndex[off];
        u8  package_state = rawIndex[off + 1U];
        u16 package_index = (u16)rawIndex[off + 2U] |
                           ((u16)rawIndex[off + 3U] << 8);

        /* �ۼ�У��ͣ���������ֽڶ��� 0xFF �� total ��ܴ� */
        allZeroCheck |= used | package_state |
                       (u8)(package_index & 0xFFU) |
                       (u8)(package_index >> 8);

        /* used ������ 0 �� 1 */
        if (used > 1U)
            return 0U;

        if (used == 1U)
        {
            /* package_state ������ WRITING(1)/VALID(2)/DELETED(3) */
            if (package_state > 3U || package_state == 0U)
                return 0U;

            /* package_index ��������Ч��Χ�� */
            if (package_index >= OFFLINE_MAX_ENTRIES)
                return 0U;

            hasValidEntry = 1U;
        }
    }

    /* �����ֽڶ��� 0xFF��ȫ���������� ������Ч������ */
    if (allZeroCheck == 0U)
        return 0U;

    /* ����������һ����Ч��Ŀ */
    if (hasValidEntry == 0U)
        return 0U;

    return 1U;
}

/*
 * SPI_Flash_Erase_Auto �� �Զ�ʶ�𲢲�����ʹ�õ� Flash ����
 *
 * �Զ��б�·����
 *   �� �����������sector 0���Ƿ������������ݷ������߰���������ʽ��
 *      �� �� �߷���2�����������ܲ�������ֻ������ʹ�õ�������
 *      �� �� �߷���3��ȫƬɨ����������������пպ�ֻ�����ǿ�������
 *
 *   ˫����֤���������ǿ� + ���ݸ�ʽ�Ϸ������������С�
 *
 * ����ֵ: ʵ�ʲ�������������
 */
u32 SPI_Flash_Erase_Auto(void)
{
    u32 erased;

    if ((!SPI_Flash_IsSectorEmpty(0U)) && SPI_Flash_IsValidOfflineIndex())
    {
        /* �������������ҷ�����������ʽ �� �����������ܲ��� */
        erased = SPI_Flash_EraseByOfflineIndex();
    }
    else
    {
        /* ������Ϊ�ջ����ݸ�ʽ���� �� ȫƬɨ����������� */
        erased = SPI_Flash_EraseDirtySectors();
    }

    printf("SPI_Flash_Erase_Auto: �Ѳ��� %lu ������ (%lu KB)\r\n",
           erased, erased * (FLASH_SECTOR_SIZE / 1024UL));

    return erased;
}
