/*
 * 软件I2C驱动头文件 - I2C总线操作函数
 */

#ifndef __IIC_SOFTWARE_H
#define __IIC_SOFTWARE_H

#include "sys.h"

/*
 * 统一软件IIC驱动接口
 *
 * 使用结构体抽象IIC引脚操作，一套时序函数控制任意IIC总线。
 * 所有引脚操作均通过 sys.h 中的宏实现，与 Config.h 中的硬件定义绑定。
 *
 * IIC_BUS_FUNCS 宏用于在 .c 文件中快速创建 IIC 总线实例：
 *
 *   // 在 MCP4017_VPP.c 中建立自己的总线实例
 *   IIC_BUS_FUNCS(HW_DVR_VPP_IIC_SCL, HW_DVR_VPP_IIC_SDA, vpp)
 *   const IIC_IO_t g_iicSoftware_VPP = { ... 赋值展开后的函数 ... };
 *
 * 也可以直接使用 iicSoftware.c 中预定义的全局实例：
 *   g_iic_vpp  - VPP数控电位器
 *   g_iic_vdd  - VDD数控电位器
 */


/* ==================== IIC 总线引脚操作函数生成宏 ==================== */

/*
 * 使用宏一次性生成某组引脚的全部操作函数
 * 参数 scl_io / sda_io 应传入 Config.h 中定义的引脚宏
 * prefix          : 函数名前缀，用于区分不同总线
 *
 * 展开后生成（均为 static）：
 *   prefix##_scl_high/low  - SCL 输出高/低
 *   prefix##_sda_high/low  - SDA 输出高/低
 *   prefix##_read_sda      - 读取 SDA 电平
 *   prefix##_sda_out       - 设置 SDA 为推挽输出
 *   prefix##_sda_in        - 设置 SDA 为上拉输入
 *   prefix##_init          - 初始化 GPIO 时钟和引脚模式
 */
#define IIC_BUS_FUNCS(SCL_IO, SDA_IO, prefix)                       \
                                                                    \
static void prefix##_scl_high(void) { PORT_OUT(SCL_IO) = 1; }       \
static void prefix##_scl_low(void)  { PORT_OUT(SCL_IO) = 0; }       \
static void prefix##_sda_high(void) { PORT_OUT(SDA_IO) = 1; }       \
static void prefix##_sda_low(void)  { PORT_OUT(SDA_IO) = 0; }       \
static u8   prefix##_read_sda(void) { return PORT_IN(SDA_IO); }     \
static void prefix##_sda_out(void)  { PORT_SET_DIR_PP(SDA_IO);}     \
static void prefix##_sda_in(void)   { PORT_SET_DIR_IN_PU(SDA_IO);}  \
static void prefix##_init(void)                                     \
{                                                                   \
    PORT_RCC_CLK(SCL_IO);                                           \
    PORT_RCC_CLK(SDA_IO);                                           \
    PORT_SET_DIR_PP(SCL_IO);                                        \
    PORT_SET_DIR_PP(SDA_IO);                                        \
    PORT_OUT(SCL_IO) = 1;                                           \
    PORT_OUT(SDA_IO) = 1;                                           \
}


/* ==================== IIC 引脚操作结构体 ==================== */

typedef struct {
    void (*sda_out)(void);   /* 设置SDA为推挽输出模式 */
    void (*sda_in)(void);    /* 设置SDA为上拉输入模式 */
    void (*sda_high)(void);  /* SDA输出高电平 */
    void (*sda_low)(void);   /* SDA输出低电平 */
    u8   (*read_sda)(void);  /* 读取SDA电平 */
    void (*scl_high)(void);  /* SCL输出高电平 */
    void (*scl_low)(void);   /* SCL输出低电平 */
    void (*init)(void);      /* 初始化GPIO时钟和引脚模式 */
} IIC_IO_t;


/* ==================== 全局 IIC 总线实例（在 iicSoftware.c 中定义） ==================== */

extern const IIC_IO_t g_iic_vpp;  /* VPP数控电位器 */
extern const IIC_IO_t g_iic_vdd;  /* VDD数控电位器 */


/* ==================== 统一 IIC 时序函数 ==================== */

void IIC_Start(const IIC_IO_t *io);
void IIC_Stop(const IIC_IO_t *io);
u8   IIC_Wait_Ack(const IIC_IO_t *io);
void IIC_Ack(const IIC_IO_t *io);
void IIC_NAck(const IIC_IO_t *io);
void IIC_Send_Byte(const IIC_IO_t *io, u8 txd);
u8   IIC_Read_Byte(const IIC_IO_t *io, unsigned char ack);

#endif

