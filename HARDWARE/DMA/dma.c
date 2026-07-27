#include "dma.h"

static u16 dmaSavedLength[12];

static s8 MYDMA_GetChannelIndex(DMA_Channel_TypeDef *DMA_CHx)
{
    if (DMA_CHx == DMA1_Channel1) return 0;
    if (DMA_CHx == DMA1_Channel2) return 1;
    if (DMA_CHx == DMA1_Channel3) return 2;
    if (DMA_CHx == DMA1_Channel4) return 3;
    if (DMA_CHx == DMA1_Channel5) return 4;
    if (DMA_CHx == DMA1_Channel6) return 5;
    if (DMA_CHx == DMA1_Channel7) return 6;
#ifdef DMA2_Channel1
    if (DMA_CHx == DMA2_Channel1) return 7;
    if (DMA_CHx == DMA2_Channel2) return 8;
    if (DMA_CHx == DMA2_Channel3) return 9;
    if (DMA_CHx == DMA2_Channel4) return 10;
    if (DMA_CHx == DMA2_Channel5) return 11;
#endif
    return -1;
}

static void MYDMA_EnableClock(DMA_Channel_TypeDef *DMA_CHx)
{
    if ((u32)DMA_CHx >= (u32)DMA1_Channel1 && (u32)DMA_CHx <= (u32)DMA1_Channel7)
        RCC->AHBENR |= RCC_AHBENR_DMA1EN;
#ifdef RCC_AHBENR_DMA2EN
    else
        RCC->AHBENR |= RCC_AHBENR_DMA2EN;
#endif
}

void MYDMA_Config(DMA_Channel_TypeDef *DMA_CHx, u32 cpar, u32 cmar, u16 cndtr)
{
    s8 chIndex;

    if (DMA_CHx == 0 || cndtr == 0)
        return;

    MYDMA_EnableClock(DMA_CHx);
    MYDMA_Disable(DMA_CHx);

    chIndex = MYDMA_GetChannelIndex(DMA_CHx);
    if (chIndex >= 0)
        dmaSavedLength[(u8)chIndex] = cndtr;

    DMA_CHx->CPAR = cpar;
    DMA_CHx->CMAR = cmar;
    DMA_CHx->CNDTR = cndtr;

    /* 该封装只适合简单的“存储器 -> 外设，8 位宽度，普通模式”场景。 */
    DMA_CHx->CCR = 0;
    DMA_CHx->CCR |= DMA_CCR1_DIR;
    DMA_CHx->CCR |= DMA_CCR1_MINC;
    DMA_CHx->CCR |= DMA_CCR1_PL_0;
}

void MYDMA_Enable(DMA_Channel_TypeDef *DMA_CHx)
{
    s8 chIndex;

    if (DMA_CHx == 0)
        return;

    DMA_CHx->CCR &= ~DMA_CCR1_EN;

    chIndex = MYDMA_GetChannelIndex(DMA_CHx);
    if (chIndex >= 0)
        DMA_CHx->CNDTR = dmaSavedLength[(u8)chIndex];

    DMA_CHx->CCR |= DMA_CCR1_EN;
}

void MYDMA_Disable(DMA_Channel_TypeDef *DMA_CHx)
{
    if (DMA_CHx == 0)
        return;

    DMA_CHx->CCR &= ~DMA_CCR1_EN;
}


