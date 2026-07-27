/*
 * Handler 自动机模�?�?Handler AutoMate Interface
 *
 * 功能:
 *   实现全自动编程器与机械手 (Handler) 之间的标准握手信�?
 *     SOT (Start of Test)  �?机械�?�?编程�? 通知"芯片已就�?
 *     EOT (End of Test)    �?编程�?�?机械�? 通知"测试/编程完成"
 *     BUSY                  �?编程�?�?机械�? 通知"正在处理, 禁止取放"
 *     OK / NG               �?编程�?�?机械�? 通知"良品/不良�?分拣
 *
 * 端口映射 (来自 Hardware_Config.h �?4~79�?:
 *     HW_HANDLER_OK      C,6   (PA8   �?BUSY)
 *     HW_HANDLER_NG      C,7   (PC7   �?NG)
 *     HW_HANDLER_BUSY    A,8   (PC6   �?OK)
 *     HW_HANDLER_START   D,1   (PD1   �?SOT输入)
 *     HW_HANDLER_UD      D,0   (PD0   �?SOT �?下拉控制)
 *
 * NOTE: Hardware_Config.h �?SOT=START=D1, UD=D0，�?BUSY/OK/NG 恰好
 *       与原 DFM 项目�?"OK=C6, NG=C7, BUSY=A8" 的定义顺序一致�?
 *       此处沿用 Hardware_Config.h 已定义的端口宏，不做额外重定义�?
 *
 * 移植�? DFMProgrammer/Product_VET6/.../Src/handler.c
 * 原模块使�?STM32 HAL 库，现改为本项目裸机寄存器风�?(PORT_OUT / PORT_IN)�?
 */

#ifndef __HANDLER_H__
#define __HANDLER_H__

#include "sys.h"
#include "Hardware_Config.h"

/* ── 端口宏定�?────────────────────────────────────────────────── */
/* 所有引脚已�?Hardware_Config.h 中通过 HW_HANDLER_xxx 宏定义�?
 * 为避免重复定义，下面仅定义读写快捷宏�?*/

/* 读取 SOT (Start of Test) 信号: PD1 */
#define HANDLER_SOT_GET           PORT_IN(HW_HANDLER_START)

/* SOT 上拉 (PD0=0)：SOT 空闲时被拉到低电�?*/
#define HANDLER_SOT_SET_UP_LOAD   do{ PORT_OUT(HW_HANDLER_UD)=0; }while(0)
/* SOT 下拉 (PD0=1)：SOT 空闲时被拉到高电�?*/
#define HANDLER_SOT_SET_DW_LOAD   do{ PORT_OUT(HW_HANDLER_UD)=1; }while(0)

/* OK 信号 (PC6) */
#define HANDLER_OK_INIT  do{ STM_IO_SET_DIR_PP(HW_HANDLER_OK); }while(0)
#define HANDLER_OK_SET   PORT_OUT(HW_HANDLER_OK)=1
#define HANDLER_OK_CLR   PORT_OUT(HW_HANDLER_OK)=0

/* NG 信号 (PC7) */
#define HANDLER_NG_INIT  do{ STM_IO_SET_DIR_PP(HW_HANDLER_NG); }while(0)
#define HANDLER_NG_SET   PORT_OUT(HW_HANDLER_NG)=1
#define HANDLER_NG_CLR   PORT_OUT(HW_HANDLER_NG)=0

/* BUSY 信号 (PA8) */
#define HANDLER_BUSY_INIT  do{ STM_IO_SET_DIR_PP(HW_HANDLER_BUSY); }while(0)
#define HANDLER_BUSY_SET  PORT_OUT(HW_HANDLER_BUSY)=1
#define HANDLER_BUSY_CLR  PORT_OUT(HW_HANDLER_BUSY)=0

/* EOT (End of Test) �?本项目中无独�?EOT 引脚, 使用 OK/NG + BUSY 组合表示�?
 * 但为接口兼容，定义占位宏 (实际不会触发)�?*/
#define HANDLER_EOT_INIT {}
#define HANDLER_EOT_SET   {}
#define HANDLER_EOT_CLR   {}

/* ── 初始化时将所有输出信号归�?── */
#define SET_BIN_TO_DEFAULT         do{ \
            PORT_OUT(HW_HANDLER_OK)=0;   \
            PORT_OUT(HW_HANDLER_NG)=1;   \
            PORT_OUT(HW_HANDLER_BUSY)=0; \
        }while(0)

/* ── 参数结构�?────────────────────────────────────────────────── */

/** Handler 配置 (保存�?Flash�? 可通过命令修改) */
typedef struct {
    uint8_t  sotLevel;              /* SOT 有效电平: 0=低有�? 1=高有�?*/
    uint8_t  eotLevel;              /* EOT 有效电平 (保留, 暂无硬件) */
    uint8_t  busyLevel;             /* BUSY 有效电平: 0=低有�? 1=高有�?*/
    uint8_t  passLevel;             /* OK 有效电平: 0=低有�? 1=高有�?*/
    uint8_t  ngLevel;               /* NG 有效电平: 0=低有�? 1=高有�?*/
    uint16_t delayMsBinToEot;       /* BIN 判决输出 �?EOT 拉起的稳定延�?(ms) */
    uint16_t delayMsMinTestTime;    /* 最短测试时�?(ms), 用于滤除抖动 */
} HandlerConfigerType;

/** 统计计数�?存放在EEPROM中，记录测试数据 */
typedef struct {
    uint32_t realTotal;             /* 实际检测总芯片数 */
    uint32_t realPassed;            /* 实际通过的芯片数 */
    uint32_t realFaild;             /* 实际失败的芯片数 */
    uint32_t logicTotal;            /* 逻辑检测总芯片数 */
    uint32_t logicPassed;           /* 逻辑通过芯片�?*/
    uint32_t logicFaild;            /* 逻辑失败芯片�?*/
} statisticsType;

/* ── 全局变量 ──────────────────────────────────────────────────── */
extern HandlerConfigerType  usedHandler;
extern statisticsType       usedStatistics;

/* ── 函数原型 ──────────────────────────────────────────────────── */

/** 从配置缓冲区更新 Handler 电平设置 */
void HandlerChangeLevel(uint8_t* pBuff);

/** 读取 Handler 配置到缓冲区 (可选输�? */
void HandlerReadConfig(uint8_t* pByteBuff);

/** �?Handler 配置写入非易失存�?*/
void HandlerUpdateFlash(void);

/** 统计信息读取/更新/复位 */
void StatisticsReadParam(uint8_t* pBuff);
void StatisticsUpdataParam(uint8_t* pBuff);
uint8_t StatisticResetParam(void);

/** 初始�?Handler 模块: 配置端口 + 读取存储的配�?*/
void Handler_Task_Init(void);

/** 设置 BIN 信号: bin=0 表示 PASS, �? 表示 FAIL */
void HandlerSetBin(uint8_t bin);

/**
 * @brief  Handler 状态机主函�?(�?1ms 调用一�?
 * @param  stateIndex  0xFF = 自由运行; 其它�?= 强制设置新状�?
 *                     (0=复位, 1=等待SOT, 2=等待SOT结束, 3=输出BIN)
 * @param  bin         BIN �?(3号状态时使用)
 * @return 1 = SOT 有效 (检测到芯片就位), 0 = 等待�?
 */
uint16_t HandlerTask(uint8_t stateIndex, uint8_t bin);

#endif /* __HANDLER_H__ */

