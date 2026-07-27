/*
 * 统一软件IIC驱动
 *
 * 通过IIC_IO_t结构体抽象引脚操作，一套时序函数控制任意IIC总线。
 * 所有引脚操作均通过 sys.h 提供的宏实现：
 *   PORT_RCC_CLK(io)      - 使能端口时钟
 *   PORT_SET_DIR_PP(io)   - 推挽输出 50MHz
 *   PORT_SET_DIR_IN_PU(io)- 上拉输入
 *   PORT_OUT(io)          - 位带输出
 *   PORT_IN(io)           - 位带输入
 *
 * 引脚信息在 Config.h 中定义，修改引脚只需改 Config.h 一处。
 *
 * 当前总线实例：
 *   g_iic_vpp - VPP数控电位器 (HW_DVR_VPP_IIC_SCL, HW_DVR_VPP_IIC_SDA)
 *   g_iic_vdd - VDD数控电位器 (HW_DVR_VDD_IIC_SCL, HW_DVR_VDD_IIC_SDA)
 */

#include "iicSoftware.h"
#include "delay.h"
#include "Hardware_Config.h"


/* ==================== 用 sys.h 宏生成引脚操作函数 ==================== */

/* IIC_BUS_FUNCS 宏定义已移至 iicSoftware.h */

/* ==================== 生成两套引脚操作函数 ==================== */

IIC_BUS_FUNCS(HW_DVR_VPP_IIC_SCL, HW_DVR_VPP_IIC_SDA, vpp)
IIC_BUS_FUNCS(HW_DVR_VDD_IIC_SCL, HW_DVR_VDD_IIC_SDA, vdd)

/* ==================== 总线实例定义 ==================== */

const IIC_IO_t g_iic_vpp = {
    .sda_out  = vpp_sda_out,
    .sda_in   = vpp_sda_in,
    .sda_high = vpp_sda_high,
    .sda_low  = vpp_sda_low,
    .read_sda = vpp_read_sda,
    .scl_high = vpp_scl_high,
    .scl_low  = vpp_scl_low,
    .init     = vpp_init,
};

const IIC_IO_t g_iic_vdd = {
    .sda_out  = vdd_sda_out,
    .sda_in   = vdd_sda_in,
    .sda_high = vdd_sda_high,
    .sda_low  = vdd_sda_low,
    .read_sda = vdd_read_sda,
    .scl_high = vdd_scl_high,
    .scl_low  = vdd_scl_low,
    .init     = vdd_init,
};

/* ==================== 统一IIC时序函数 ==================== */

/* 产生IIC起始信号 */
void IIC_Start(const IIC_IO_t *io)
{
    io->sda_out();       /* sda线输出 */
    io->sda_high();
    delay_us(4);
    io->scl_high();
    delay_us(4);
    io->sda_low();       /* START: CLK高时，DATA从高变低 */
    delay_us(4);
    io->scl_low();       /* 钳住I2C总线，准备发送或接收数据 */
}

/* 产生IIC停止信号 */
void IIC_Stop(const IIC_IO_t *io)
{
    io->sda_out();       /* sda线输出 */
    io->scl_low();
    delay_us(4);
    io->sda_low();
    delay_us(4);
    io->scl_high();
    delay_us(4);
    io->sda_high();      /* STOP: CLK高时，DATA从低变高 */
    delay_us(4);
}

/* 等待应答信号到来
 * 返回值：1，接收应答失败；0，接收应答成功
 */
u8 IIC_Wait_Ack(const IIC_IO_t *io)
{
    u8 ucErrTime = 0;

    io->sda_in();        /* SDA设置为输入 */
    io->sda_high();
    delay_us(4);
    io->scl_high();
    delay_us(4);

    while (io->read_sda())
    {
        ucErrTime++;
        if (ucErrTime > 250)
        {
            IIC_Stop(io);
            return 1;
        }
    }
    io->scl_low();       /* 时钟输出0 */
    return 0;
}

/* 产生ACK应答 */
void IIC_Ack(const IIC_IO_t *io)
{
    io->scl_low();
    io->sda_out();
    io->sda_low();
    delay_us(4);
    io->scl_high();
    delay_us(4);
    io->scl_low();
}

/* 不产生ACK应答 */
void IIC_NAck(const IIC_IO_t *io)
{
    io->scl_low();
    io->sda_out();
    io->sda_high();
    delay_us(4);
    io->scl_high();
    delay_us(4);
    io->scl_low();
}

/* IIC发送一个字节 */
void IIC_Send_Byte(const IIC_IO_t *io, u8 txd)
{
    u8 t;

    io->sda_out();
    io->scl_low();       /* 拉低时钟开始数据传输 */

    for (t = 0; t < 8; t++)
    {
        if (txd & 0x80)
            io->sda_high();
        else
            io->sda_low();

        txd <<= 1;
        delay_us(4);
        io->scl_high();
        delay_us(4);
        io->scl_low();
        delay_us(4);
    }
}

/* 读1个字节，ack=1时发送ACK，ack=0时发送nACK */
u8 IIC_Read_Byte(const IIC_IO_t *io, unsigned char ack)
{
    unsigned char i, receive = 0;

    io->sda_in();        /* SDA设置为输入 */

    for (i = 0; i < 8; i++)
    {
        io->scl_low();
        delay_us(4);
        io->scl_high();
        receive <<= 1;
        if (io->read_sda())
            receive++;
        delay_us(4);
    }

    if (!ack)
        IIC_NAck(io);    /* 发送nACK */
    else
        IIC_Ack(io);     /* 发送ACK */

    return receive;
}

