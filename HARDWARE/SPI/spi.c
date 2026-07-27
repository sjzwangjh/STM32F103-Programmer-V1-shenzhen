#include "spi.h"

/* SPI2:
 * PB13 -> SCK  (AF Push-Pull)
 * PB14 -> MISO (Input Floating)
 * PB15 -> MOSI (AF Push-Pull)
 *
 * FT25C64A 手册明确给出了 Mode 0 时序，
 * FM25W32 兼容 Mode 0 / Mode 3。
 * 因此统一按 Mode 0 初始化，兼容性更稳。
 */
void SPI2_Init(void)
{
    RCC->APB2ENR |= (1U << 3);   /* GPIOB clock */
    RCC->APB1ENR |= (1U << 14);  /* SPI2 clock */

    /* PB13=SCK, PB14=MISO, PB15=MOSI */
    GPIOB->CRH &= ~((u32)0xFFF << 20);
    GPIOB->CRH |=  ((u32)0xB << 20); /* PB13: 50 MHz AF PP */
    GPIOB->CRH |=  ((u32)0x4 << 24); /* PB14: input floating */
    GPIOB->CRH |=  ((u32)0xB << 28); /* PB15: 50 MHz AF PP */

    GPIOB->ODR |= (1U << 13) | (1U << 15);

    SPI2->CR1 = 0;
    SPI2->CR2 = 0;
    SPI2->I2SCFGR = 0;

    SPI2->CR1 |= SPI_CR1_MSTR;
    SPI2->CR1 |= SPI_CR1_SSM | SPI_CR1_SSI;
    SPI2->CR1 |= (SPI_SPEED_16 << 3); /* default 2.25 MHz */
    SPI2->CR1 |= SPI_CR1_SPE;

    (void)SPI2_ReadWriteByte(0xFF);
}

void SPI2_SetSpeed(u8 SpeedSet)
{
    SpeedSet &= 0x07;
    SPI2->CR1 &= ~(7U << 3);
    SPI2->CR1 |= (u16)(SpeedSet << 3);
}

u8 SPI2_ReadWriteByte(u8 TxData)
{
    u16 retry;

    retry = 0;
    while ((SPI2->SR & SPI_SR_TXE) == 0)
    {
        retry++;
        if (retry >= 0xFFFE)
            return 0;
    }

    *(__IO u8 *)&SPI2->DR = TxData;

    retry = 0;
    while ((SPI2->SR & SPI_SR_RXNE) == 0)
    {
        retry++;
        if (retry >= 0xFFFE)
            return 0;
    }

    return *(__IO u8 *)&SPI2->DR;
}

void SPI2_ReadBuf(u8 *rxBuf, u16 len)
{
    u16 i;

    if (rxBuf == 0 || len == 0)
        return;

    for (i = 0; i < len; i++)
        rxBuf[i] = SPI2_ReadWriteByte(0xFF);
}

void SPI2_WriteBuf(const u8 *txBuf, u16 len)
{
    u16 i;

    if (txBuf == 0 || len == 0)
        return;

    for (i = 0; i < len; i++)
        (void)SPI2_ReadWriteByte(txBuf[i]);
}

void SPI2_ReadWriteBuf(const u8 *txBuf, u8 *rxBuf, u16 len)
{
    u16 i;
    u8 tx;

    if (len == 0)
        return;

    for (i = 0; i < len; i++)
    {
        tx = (txBuf != 0) ? txBuf[i] : 0xFF;
        tx = SPI2_ReadWriteByte(tx);
        if (rxBuf != 0)
            rxBuf[i] = tx;
    }
}


