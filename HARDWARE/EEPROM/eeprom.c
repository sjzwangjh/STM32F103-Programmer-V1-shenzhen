/*
 * SPI EEPROM ����ģ��
 * ���� STM32F103VET6 �� SPI2 ����
 * ֧�� FT25C64A��64Kbit / 8KB���� SPI EEPROM оƬ
 */

#include "eeprom.h"
#include "spi.h"
#include <string.h>
#include <stdio.h>

static volatile u8 g_spiEepromTimeout;

#define SPI_EEPROM_BUSY_TIMEOUT  200000UL

/*
 * SPI_EEPROM_ClipLength - �ضϳ����Բ����� EEPROM �����߽�
 * addr: ��ʼ��ַ
 * len:  ���󳤶�
 * ����ֵ: ʵ�ʿ��ó��ȣ����������򷵻�0��ض�ֵ��
 */
static u16 SPI_EEPROM_ClipLength(u32 addr, u16 len)
{
    u32 remain;

    if (addr >= SPI_EEPROM_CAPACITY)
        return 0;                           /* ��ʼ��ַ��������������0 */

    remain = SPI_EEPROM_CAPACITY - addr;    /* ʣ����ÿռ� */
    if ((u32)len > remain)
        len = (u16)remain;                  /* �ضϵ����ÿռ䳤�� */

    return len;
}

/*
 * SPI_EEPROM_Init - ��ʼ�� SPI EEPROM ģ��
 * ���� CS �� WP ����Ϊ GPIO ���
 * ��ʼ�� SPI2 ���裬����ʱ���ٶ�Ϊ��Ƶ4
 * CS �� WP ��ʼ��Ϊ�ߵ�ƽ����ѡ�� / ��ֹд������
 */
void SPI_EEPROM_Init(void)
{
    g_spiEepromTimeout = 0U;
    /* ʹ�� CS �� WP ���ŵ� GPIO ʱ�� */
    PORT_RCC_CLK(HW_SPI_EEPROM_CS);
    PORT_RCC_CLK(HW_SPI_EEPROM_WP);

    /* ���� CS �� WP Ϊ������� */
    PORT_SET_DIR_PP(HW_SPI_EEPROM_CS);
    PORT_SET_DIR_PP(HW_SPI_EEPROM_WP);

    SPI_EEPROM_CS_H();          /* CS ��ʼΪ�ߣ���ѡ�У� */
    SPI_EEPROM_WP_H();          /* WP ��ʼΪ�ߣ���ֹд������ */

    SPI2_Init();                /* ��ʼ�� SPI2 ���� */
    SPI2_SetSpeed(SPI_SPEED_4); /* ���� SPI ʱ���ٶ� */
}

/*
 * SPI_EEPROM_WriteEnable - дʹ�ܣ����� WREN ָ�
 * ��ÿ��д����������ǰ�������
 */
void SPI_EEPROM_WriteEnable(void)
{
    SPI_EEPROM_CS_L();
    SPI2_ReadWriteByte(SPI_EEPROM_CMD_WREN);  /* ����дʹ��ָ�� */
    SPI_EEPROM_CS_H();
}

/*
 * SPI_EEPROM_WriteDisable - д��ֹ������ WRDI ָ�
 */
void SPI_EEPROM_WriteDisable(void)
{
    SPI_EEPROM_CS_L();
    SPI2_ReadWriteByte(SPI_EEPROM_CMD_WRDI);  /* ����д��ָֹ�� */
    SPI_EEPROM_CS_H();
}

/*
 * SPI_EEPROM_ReadStatusReg - ��״̬�Ĵ���
 * ����ֵ: ״̬�Ĵ���ֵ
 *   bit0(WIP): æ��־��1=���ڱ��/������
 *   bit1(WEL): дʹ�����棨1=��ʹ�ܣ�
 *   bit2~3(BP0~BP1): �鱣��λ
 *   bit7(WPEN): д����ʹ��
 */
u8 SPI_EEPROM_ReadStatusReg(void)
{
    u8 sr;

    SPI_EEPROM_CS_L();
    SPI2_ReadWriteByte(SPI_EEPROM_CMD_RDSR);  /* ���Ͷ�״̬�Ĵ���ָ�� */
    sr = SPI2_ReadWriteByte(0xFF);             /* ��ȡ״̬�Ĵ���ֵ */
    SPI_EEPROM_CS_H();

    return sr;
}

/*
 * SPI_EEPROM_WriteStatusReg - д״̬�Ĵ���
 * sr: Ҫд���״̬�Ĵ���ֵ���������ÿ鱣��λ�ȣ�
 */
void SPI_EEPROM_WriteStatusReg(u8 sr)
{
    SPI_EEPROM_WriteEnable();                 /* д��ǰ��Ҫдʹ�� */
    SPI_EEPROM_CS_L();
    SPI2_ReadWriteByte(SPI_EEPROM_CMD_WRSR);  /* ��д״̬�Ĵ���ָ�� */
    SPI2_ReadWriteByte(sr);                   /* д��״̬�Ĵ���ֵ */
    SPI_EEPROM_CS_H();
    SPI_EEPROM_WaitBusy();                    /* �ȴ�������� */
    SPI_EEPROM_WriteDisable();
}

/*
 * SPI_EEPROM_WaitBusy - �ȴ� EEPROM �ڲ��������
 * ��ѯ״̬�Ĵ����� WIP λ��bit0����ֱ��Ϊ0
 */
void SPI_EEPROM_WaitBusy(void)
{
    u32 timeout = SPI_EEPROM_BUSY_TIMEOUT;

    while ((SPI_EEPROM_ReadStatusReg() & SPI_EEPROM_SR_WIP) != 0U)
    {
        if (timeout-- == 0U)
        {
            g_spiEepromTimeout = 1U;
            break;
        }
    }
}


/*
 * SPI_EEPROM_ReadByte - ��ȡһ���ֽ�
 * addr: Ҫ��ȡ�ĵ�ַ��0x0000~0x1FFF��
 * ����ֵ: �õ�ַ�洢�������ֽ�
 */
u8 SPI_EEPROM_ReadByte(u32 addr)
{
    u8 data;

    addr &= SPI_EEPROM_MAX_ADDR;              /* ȷ����ַ�ںϷ���Χ�� */

    SPI_EEPROM_CS_L();
    SPI2_ReadWriteByte(SPI_EEPROM_CMD_READ);  /* ���Ͷ�����ָ�� */
    SPI2_ReadWriteByte((u8)(addr >> 8));      /* ��ַ��8λ */
    SPI2_ReadWriteByte((u8)addr);             /* ��ַ��8λ */
    data = SPI2_ReadWriteByte(0xFF);          /* ��ȡ�����ֽ� */
    SPI_EEPROM_CS_H();

    return data;
}

/*
 * SPI_EEPROM_WriteByte - д��һ���ֽ�
 * addr: Ҫд��ĵ�ַ
 * data: Ҫд�������
 */
void SPI_EEPROM_WriteByte(u32 addr, u8 data)
{
    addr &= SPI_EEPROM_MAX_ADDR;              /* ȷ����ַ�ںϷ���Χ�� */

    SPI_EEPROM_WriteEnable();                 /* дʹ�� */
    SPI_EEPROM_CS_L();
    SPI2_ReadWriteByte(SPI_EEPROM_CMD_WRITE); /* ����д����ָ�� */
    SPI2_ReadWriteByte((u8)(addr >> 8));      /* ��ַ��8λ */
    SPI2_ReadWriteByte((u8)addr);             /* ��ַ��8λ */
    SPI2_ReadWriteByte(data);                 /* д������ */
    SPI_EEPROM_CS_H();
    SPI_EEPROM_WaitBusy();                    /* �ȴ������� */
    SPI_EEPROM_WriteDisable();
}

/*
 * SPI_EEPROM_Read - ��ȡ��������
 * addr:  ��ʼ��ַ
 * pBuf:  ���������
 * len:   Ҫ��ȡ���ֽ���
 */
void SPI_EEPROM_Read(u32 addr, u8 *pBuf, u16 len)
{
    if (pBuf == 0)
        return;

    len = SPI_EEPROM_ClipLength(addr, len);   /* �ضϵ���Ч��Χ */
    if (len == 0)
        return;

    SPI_EEPROM_CS_L();
    SPI2_ReadWriteByte(SPI_EEPROM_CMD_READ);  /* ���Ͷ�����ָ�� */
    SPI2_ReadWriteByte((u8)(addr >> 8));      /* ��ַ��8λ */
    SPI2_ReadWriteByte((u8)addr);             /* ��ַ��8λ */
    SPI2_ReadBuf(pBuf, len);                  /* ������ȡ���� */
    SPI_EEPROM_CS_H();
}

/*
 * SPI_EEPROM_WritePage - д��һҳ���ݣ���� 32 �ֽڣ�����ҳ��
 * addr:  ��ʼ��ַ������ҳ����ʼλ�ã�
 * pBuf:  ����Դ������
 * len:   Ҫд����ֽ������Զ������ڵ�ǰҳʣ��ռ��ڣ�
 */
void SPI_EEPROM_WritePage(u32 addr, const u8 *pBuf, u16 len)
{
    u16 pageRemain;

    if (pBuf == 0)
        return;

    len = SPI_EEPROM_ClipLength(addr, len);   /* �ضϵ���Ч��Χ */
    if (len == 0)
        return;

    /* ���㵱ǰҳ��ʣ��ռ� */
    pageRemain = (u16)(SPI_EEPROM_PAGE_SIZE - (addr & (SPI_EEPROM_PAGE_SIZE - 1U)));
    if (len > pageRemain)
        len = pageRemain;                     /* ������һҳ�� */

    SPI_EEPROM_WriteEnable();                 /* дʹ�� */
    SPI_EEPROM_CS_L();
    SPI2_ReadWriteByte(SPI_EEPROM_CMD_WRITE); /* ����д����ָ�� */
    SPI2_ReadWriteByte((u8)(addr >> 8));      /* ��ַ��8λ */
    SPI2_ReadWriteByte((u8)addr);             /* ��ַ��8λ */
    SPI2_WriteBuf(pBuf, len);                 /* ����д������ */
    SPI_EEPROM_CS_H();
    SPI_EEPROM_WaitBusy();                    /* �ȴ������� */
    SPI_EEPROM_WriteDisable();
}

/*
 * SPI_EEPROM_Write - д���������ݣ��Զ�������ҳ��
 * ���д���Խҳ�߽磬�Զ����Ϊ���ҳд��
 * addr:  ��ʼ��ַ
 * pBuf:  ����Դ������
 * len:   Ҫд����ֽ���
 */
void SPI_EEPROM_Write(u32 addr, const u8 *pBuf, u16 len)
{
    u16 writeLen;
    u16 pageOffset;

    if (pBuf == 0)
        return;

    len = SPI_EEPROM_ClipLength(addr, len);   /* �ضϵ���Ч��Χ */
    while (len > 0)
    {
        /* ���㵱ǰҳ��ƫ�ƺͱ��ο�д�볤�� */
        pageOffset = (u16)(addr & (SPI_EEPROM_PAGE_SIZE - 1U));
        writeLen = (u16)(SPI_EEPROM_PAGE_SIZE - pageOffset);
        if (writeLen > len)
            writeLen = len;

        SPI_EEPROM_WritePage(addr, pBuf, writeLen);  /* д��һҳ */

        addr  += writeLen;                    /* ��ַǰ�� */
        pBuf  += writeLen;                    /* ָ��ǰ�� */
        len   -= writeLen;                    /* ʣ���ֽ������� */
    }
}

/*
 * SPI_EEPROM_EraseAll - ȫƬ����
 * �� EEPROM ���д洢��Ԫд�� 0xFF
 * ע��EEPROM ��֧��Ӳ����Ƭ����ָ�
 *     ��ͨ����ҳд�� 0xFF ʵ��
 */
void SPI_EEPROM_EraseAll(void)
{
    u32 addr;
    u8 pageBuf[SPI_EEPROM_PAGE_SIZE];
    u16 i;

    /* ����һҳȫ 0xFF �Ļ����� */
    for (i = 0; i < SPI_EEPROM_PAGE_SIZE; i++)
        pageBuf[i] = 0xFF;

    /* ��ҳд�� 0xFF */
    for (addr = 0; addr < SPI_EEPROM_CAPACITY; addr += SPI_EEPROM_PAGE_SIZE)
        SPI_EEPROM_WritePage(addr, pageBuf, SPI_EEPROM_PAGE_SIZE);
}

/*
 * SPI_EEPROM_ReadID - ��ȡ���� ID ���豸 ID
 * ͨ������ 0x9F��JEDEC ID��ָ���ȡ
 * mid: ������� ID ָ�루��Ϊ 0��
 * did: ����豸 ID ָ�루��Ϊ 0��
 */
void SPI_EEPROM_ReadID(u8 *mid, u8 *did)
{
    SPI_EEPROM_CS_L();
    SPI2_ReadWriteByte(0x9F);               /* �� JEDEC ID ָ�� */
    if (mid != 0)
        *mid = SPI2_ReadWriteByte(0xFF);    /* ������ ID */
    if (did != 0)
        *did = SPI2_ReadWriteByte(0xFF);    /* ���豸 ID */
    SPI_EEPROM_CS_H();
}

/*
 * SPI_EEPROM_DebugDemo - SPI EEPROM ����ʾ��
 * ���� main() ��� SPI_EEPROM_Init() ���ֶ����á�
 * ʾ�����̣�
 * 1. ��ȡ���� ID / �豸 ID
 * 2. д���������
 * 3. �ض����Ƚ�
 * ÿһ������ͨ�� USART1 ��ӡ������Ϣ��
 */
void SPI_EEPROM_DebugDemo(void)
{
    static const u8 txBuf[16] =
    {
        0x45, 0x45, 0x50, 0x52, 0x4F, 0x4D, 0x5F, 0x44,
        0x45, 0x4D, 0x4F, 0x5F, 0x36, 0x34, 0x41, 0x21
    };
    u8 rxBuf[sizeof(txBuf)];
    u8 mid;
    u8 did;
    u8 compareOk;
    u16 i;

    memset(rxBuf, 0, sizeof(rxBuf));
    mid = 0;
    did = 0;
    compareOk = 0;

    /* ����1: ��ʼ�� EEPROM */
    printf("��EEPROM���ԡ���ʼ SPI EEPROM ����...\r\n");
    SPI_EEPROM_Init();
    printf("��EEPROM���ԡ���ʼ�����\r\n");

    /* ����2: ��ȡ���� ID ���豸 ID */
    SPI_EEPROM_ReadID(&mid, &did);
    printf("��EEPROM���ԡ���ȡID: ����=0x%02X, �豸=0x%02X\r\n", mid, did);

    /* ����3: д�� 16 �ֽڲ������ݵ���ַ 0x0000 */
    printf("��EEPROM���ԡ�д�����ݵ���ַ 0x0000: ");
    for (i = 0; i < sizeof(txBuf); i++)
        printf("%02X ", txBuf[i]);
    printf("\r\n");
    SPI_EEPROM_Write(0x0000U, txBuf, sizeof(txBuf));
    printf("��EEPROM���ԡ�д�����\r\n");

    /* ����4: �ӵ�ַ 0x0000 �ض����� */
    SPI_EEPROM_Read(0x0000U, rxBuf, sizeof(rxBuf));
    printf("��EEPROM���ԡ��ض�����: ");
    for (i = 0; i < sizeof(rxBuf); i++)
        printf("%02X ", rxBuf[i]);
    printf("\r\n");

    /* ����5: �Ƚ�д��ͻض����� */
    if (memcmp(txBuf, rxBuf, sizeof(txBuf)) == 0)
    {
        compareOk = 1;
        printf("��EEPROM���ԡ��ȽϽ��: һ�£���д����ͨ����\r\n");
    }
    else
    {
        printf("��EEPROM���ԡ��ȽϽ��: ��һ�£���д����ʧ�ܣ�\r\n");
    }

    /* ����6: ȫƬ��������֤ */
    printf("��EEPROM���ԡ���ʼȫƬ����...\r\n");
    SPI_EEPROM_EraseAll();
    printf("��EEPROM���ԡ�ȫƬ�������\r\n");

    memset(rxBuf, 0, sizeof(rxBuf));
    SPI_EEPROM_Read(0x0000U, rxBuf, sizeof(rxBuf));
    printf("��EEPROM���ԡ��������ȡ 0x0000: ");
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
        printf("��EEPROM���ԡ�������֤ͨ����ȫ��Ϊ 0xFF\r\n");
    else
        printf("��EEPROM���ԡ�������֤ʧ�ܣ����ڷ� 0xFF ����\r\n");

    printf("��EEPROM���ԡ�����\r\n");

    (void)compareOk;
}

