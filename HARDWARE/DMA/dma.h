/*
 * DMA驱动头文件 - DMA通道配置和传输函数
 */

#ifndef __DMA_H
#define __DMA_H

#include "sys.h"

void MYDMA_Config(DMA_Channel_TypeDef *DMA_CHx, u32 cpar, u32 cmar, u16 cndtr);
void MYDMA_Enable(DMA_Channel_TypeDef *DMA_CHx);
void MYDMA_Disable(DMA_Channel_TypeDef *DMA_CHx);

#endif


