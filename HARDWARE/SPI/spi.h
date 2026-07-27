#ifndef __SPI_H
#define __SPI_H

#include "sys.h"
#include "Hardware_Config.h"

/* SPI2 分频参数
 * SPI 时钟 = PCLK1 / 2^(SpeedSet + 1)
 * PCLK1 = 36 MHz
 */
#define SPI_SPEED_2             0   /* 18 MHz  */
#define SPI_SPEED_4             1   /* 9 MHz   */
#define SPI_SPEED_8             2   /* 4.5 MHz */
#define SPI_SPEED_16            3   /* 2.25 MHz */
#define SPI_SPEED_32            4   /* 1.125 MHz */
#define SPI_SPEED_64            5   /* 562.5 KHz */
#define SPI_SPEED_128           6   /* 281.25 KHz */
#define SPI_SPEED_256           7   /* 140.625 KHz */

void SPI2_Init(void);
void SPI2_SetSpeed(u8 SpeedSet);
u8   SPI2_ReadWriteByte(u8 TxData);
void SPI2_ReadBuf(u8 *rxBuf, u16 len);
void SPI2_WriteBuf(const u8 *txBuf, u16 len);
void SPI2_ReadWriteBuf(const u8 *txBuf, u8 *rxBuf, u16 len);

#endif


