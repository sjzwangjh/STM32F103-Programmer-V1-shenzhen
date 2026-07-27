#include "adc.h"
#include "delay.h"

u16 adcScanRecodeBuff[ADC_SCAN_BUF_SIZE];

static void Adc_GPIO_Init(void)
{
    RCC->APB2ENR |= (1 << 2) | (1 << 4);

    /* PA1~PA7 配置为模拟输入。 */
    GPIOA->CRL &= ~((u32)0xFFFFFFF0);

    /* PC4 配置为模拟输入。 */
    GPIOC->CRL &= ~((u32)0x000F0000);
}

void Adc_Init(void)
{
    u32 i;

    Adc_GPIO_Init();

    RCC->APB2ENR |= (1 << 9);
    RCC->AHBENR  |= (1 << 0);
    delay_ms(1);

    /* ADC 时钟 = PCLK2 / 6 = 12 MHz。 */
    RCC->CFGR &= ~(3 << 14);
    RCC->CFGR |=  (2 << 14);

    ADC1->CR1 = 0;
    ADC1->CR2 = 0;

    ADC1->CR1 |= ADC_CR1_SCAN;
    ADC1->CR2 |= ADC_CR2_CONT;
    ADC1->CR2 |= ADC_CR2_DMA;
    ADC1->CR2 |= ADC_CR2_EXTTRIG;
    ADC1->CR2 |= (7 << 17);

    ADC1->SQR1 = (7 << 20);
    ADC1->SQR2 = 0;
    ADC1->SQR3 = 0;

    ADC1->SQR3 |= (1  << 0);
    ADC1->SQR3 |= (2  << 5);
    ADC1->SQR3 |= (3  << 10);
    ADC1->SQR3 |= (4  << 15);
    ADC1->SQR3 |= (5  << 20);
    ADC1->SQR3 |= (6  << 25);
    ADC1->SQR2 |= (7  << 0);
    ADC1->SQR2 |= (14 << 5);

    ADC1->SMPR1 = 0;
    ADC1->SMPR2 = 0;
    ADC1->SMPR2 |= (7 << 3);
    ADC1->SMPR2 |= (7 << 6);
    ADC1->SMPR2 |= (7 << 9);
    ADC1->SMPR2 |= (7 << 12);
    ADC1->SMPR2 |= (7 << 15);
    ADC1->SMPR2 |= (7 << 18);
    ADC1->SMPR2 |= (7 << 21);
    ADC1->SMPR1 |= (7 << 12);

    /* STM32F103 的 ADC1 规则通道 DMA 请求固定对应 DMA1_Channel1。 */
    DMA1_Channel1->CCR = 0;
    DMA1_Channel1->CPAR = (u32)&ADC1->DR;
    DMA1_Channel1->CMAR = (u32)adcScanRecodeBuff;
    DMA1_Channel1->CNDTR = ADC_SCAN_BUF_SIZE;
    DMA1_Channel1->CCR |= DMA_CCR1_CIRC;
    DMA1_Channel1->CCR |= DMA_CCR1_MINC;
    DMA1_Channel1->CCR |= DMA_CCR1_PSIZE_0;
    DMA1_Channel1->CCR |= DMA_CCR1_MSIZE_0;
    DMA1_Channel1->CCR |= DMA_CCR1_PL_1;

    DMA1->IFCR = DMA_IFCR_CGIF1 | DMA_IFCR_CTCIF1 | DMA_IFCR_CHTIF1 | DMA_IFCR_CTEIF1;

    for (i = 0; i < ADC_SCAN_BUF_SIZE; i++)
        adcScanRecodeBuff[i] = 0;

    ADC1->CR2 |= ADC_CR2_ADON;
    delay_ms(1);

    ADC1->CR2 |= ADC_CR2_RSTCAL;
    while (ADC1->CR2 & ADC_CR2_RSTCAL)
    {
    }

    ADC1->CR2 |= ADC_CR2_CAL;
    while (ADC1->CR2 & ADC_CR2_CAL)
    {
    }

    DMA1_Channel1->CCR |= DMA_CCR1_EN;

    ADC1->CR2 |= ADC_CR2_ADON;
    ADC1->CR2 |= ADC_CR2_SWSTART;
}

u16 Adc_GetChannel(u8 ch)
{
    u16 cnt;
    u16 pos;
    u16 samplePos;

    if (ch >= ADC_SCAN_CHANNELS)
        return 0;

    cnt = DMA1_Channel1->CNDTR;
    pos = (ADC_SCAN_BUF_SIZE - cnt) & (ADC_SCAN_BUF_SIZE - 1);

    samplePos = (u16)((pos + ADC_SCAN_BUF_SIZE - ADC_SCAN_CHANNELS) & (ADC_SCAN_BUF_SIZE - 1));
    samplePos = (u16)((samplePos & ~(ADC_SCAN_CHANNELS - 1)) | ch);

    return adcScanRecodeBuff[samplePos];
}


