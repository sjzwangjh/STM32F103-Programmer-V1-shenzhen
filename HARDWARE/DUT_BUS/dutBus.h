#ifndef __DUT_BUS_H__
#define __DUT_BUS_H__

#include "sys.h"
#include "Hardware_Config.h"

/*
 * 保留旧版宏定义方式，同时新增一套基于 STM_IO_* 的实现。
 * 默认启用新实现；若需要回退到旧实现，可将该宏改为 0。
 */
#ifndef DUT_BUS_USE_STM_IO_MACRO
#define DUT_BUS_USE_STM_IO_MACRO 1
#endif

#if (DUT_BUS_USE_STM_IO_MACRO == 0)

/* VPP 控制宏定义 */
#define DUT_VPP_SET_VPP           do { PORT_OUT(HW_DUT_VPP_VL_ON) = 0; PORT_OUT(HW_DUT_VPP_VH_ON) = 1; } while (0)
#define DUT_VPP_SET_GND           do { PORT_OUT(HW_DUT_VPP_VH_ON) = 0; PORT_OUT(HW_DUT_VPP_VL_ON) = 1; } while (0)
#define DUT_VPP_SET_FLOAT         do { PORT_OUT(HW_DUT_VPP_VL_ON) = 0; PORT_OUT(HW_DUT_VPP_VH_ON) = 0; } while (0)

/* VDD 控制宏定义 */
#define DUT_VDD_SET_VDD           do { PORT_OUT(HW_DUT_VDD_VL_ON) = 0; PORT_OUT(HW_DUT_VDD_VH_ON) = 1; } while (0)
#define DUT_VDD_SET_GND           do { PORT_OUT(HW_DUT_VDD_VH_ON) = 0; PORT_OUT(HW_DUT_VDD_VL_ON) = 1; } while (0)
#define DUT_VDD_SET_FLOAT         do { PORT_OUT(HW_DUT_VDD_VL_ON) = 0; PORT_OUT(HW_DUT_VDD_VH_ON) = 0; } while (0)

/* DUT PIN 设置为输入 */
#define DUT_PIN4_SET_INPUT        do { PORT_SET_DIR_IN_PD(HW_DUT_PIN4_DAT); PORT_OUT(HW_DUT_PIN4_CTRL) = 0; } while (0)
#define DUT_PIN5_SET_INPUT        do { PORT_SET_DIR_IN_PD(HW_DUT_PIN5_DAT); PORT_OUT(HW_DUT_PIN5_CTRL) = 0; } while (0)
#define DUT_PIN6_SET_INPUT        do { PORT_SET_DIR_IN_PD(HW_DUT_PIN6_DAT); PORT_OUT(HW_DUT_PIN6_CTRL) = 0; } while (0)
#define DUT_PIN7_SET_INPUT        do { PORT_SET_DIR_IN_PD(HW_DUT_PIN7_DAT); PORT_OUT(HW_DUT_PIN7_CTRL) = 0; } while (0)
#define DUT_PIN8_SET_INPUT        do { PORT_SET_DIR_IN_PD(HW_DUT_PIN8_DAT); PORT_OUT(HW_DUT_PIN8_CTRL) = 0; } while (0)

/* DUT PIN 设置为输出 */
#define DUT_PIN4_SET_OUTPUT       do { PORT_OUT(HW_DUT_PIN4_CTRL) = 1; PORT_SET_DIR_PP(HW_DUT_PIN4_DAT); } while (0)
#define DUT_PIN5_SET_OUTPUT       do { PORT_OUT(HW_DUT_PIN5_CTRL) = 1; PORT_SET_DIR_PP(HW_DUT_PIN5_DAT); } while (0)
#define DUT_PIN6_SET_OUTPUT       do { PORT_OUT(HW_DUT_PIN6_CTRL) = 1; PORT_SET_DIR_PP(HW_DUT_PIN6_DAT); } while (0)
#define DUT_PIN7_SET_OUTPUT       do { PORT_OUT(HW_DUT_PIN7_CTRL) = 1; PORT_SET_DIR_PP(HW_DUT_PIN7_DAT); } while (0)
#define DUT_PIN8_SET_OUTPUT       do { PORT_OUT(HW_DUT_PIN8_CTRL) = 1; PORT_SET_DIR_PP(HW_DUT_PIN8_DAT); } while (0)

#else

/* VPP 控制宏定义 */
#define DUT_VPP_SET_VPP           do { STM_IO_CLR(HW_DUT_VPP_VL_ON); STM_IO_SET(HW_DUT_VPP_VH_ON); } while (0)
#define DUT_VPP_SET_GND           do { STM_IO_CLR(HW_DUT_VPP_VH_ON); STM_IO_SET(HW_DUT_VPP_VL_ON); } while (0)
#define DUT_VPP_SET_FLOAT         do { STM_IO_CLR(HW_DUT_VPP_VL_ON); STM_IO_CLR(HW_DUT_VPP_VH_ON); } while (0)

/* VDD 控制宏定义 */
#define DUT_VDD_SET_VDD           do { STM_IO_CLR(HW_DUT_VDD_VL_ON); STM_IO_SET(HW_DUT_VDD_VH_ON); } while (0)
#define DUT_VDD_SET_GND           do { STM_IO_CLR(HW_DUT_VDD_VH_ON); STM_IO_SET(HW_DUT_VDD_VL_ON); } while (0)
#define DUT_VDD_SET_FLOAT         do { STM_IO_CLR(HW_DUT_VDD_VL_ON); STM_IO_CLR(HW_DUT_VDD_VH_ON); } while (0)

/* DUT PIN 设置为输入 */
#define DUT_PIN4_SET_INPUT        do { STM_IO_SET_DIR_IN_PD(HW_DUT_PIN4_DAT); STM_IO_CLR(HW_DUT_PIN4_CTRL); } while (0)
#define DUT_PIN5_SET_INPUT        do { STM_IO_SET_DIR_IN_PD(HW_DUT_PIN5_DAT); STM_IO_CLR(HW_DUT_PIN5_CTRL); } while (0)
#define DUT_PIN6_SET_INPUT        do { STM_IO_SET_DIR_IN_PD(HW_DUT_PIN6_DAT); STM_IO_CLR(HW_DUT_PIN6_CTRL); } while (0)
#define DUT_PIN7_SET_INPUT        do { STM_IO_SET_DIR_IN_PD(HW_DUT_PIN7_DAT); STM_IO_CLR(HW_DUT_PIN7_CTRL); } while (0)
#define DUT_PIN8_SET_INPUT        do { STM_IO_SET_DIR_IN_PD(HW_DUT_PIN8_DAT); STM_IO_CLR(HW_DUT_PIN8_CTRL); } while (0)

/* DUT PIN 设置为输出 */
#define DUT_PIN4_SET_OUTPUT       do { STM_IO_SET(HW_DUT_PIN4_CTRL); STM_IO_SET_DIR_PP(HW_DUT_PIN4_DAT); } while (0)
#define DUT_PIN5_SET_OUTPUT       do { STM_IO_SET(HW_DUT_PIN5_CTRL); STM_IO_SET_DIR_PP(HW_DUT_PIN5_DAT); } while (0)
#define DUT_PIN6_SET_OUTPUT       do { STM_IO_SET(HW_DUT_PIN6_CTRL); STM_IO_SET_DIR_PP(HW_DUT_PIN6_DAT); } while (0)
#define DUT_PIN7_SET_OUTPUT       do { STM_IO_SET(HW_DUT_PIN7_CTRL); STM_IO_SET_DIR_PP(HW_DUT_PIN7_DAT); } while (0)
#define DUT_PIN8_SET_OUTPUT       do { STM_IO_SET(HW_DUT_PIN8_CTRL); STM_IO_SET_DIR_PP(HW_DUT_PIN8_DAT); } while (0)

#endif

/* DUT 总线全部设置为输入 */
#define DUT_BUS_SET_INPUT         do { DUT_PIN4_SET_INPUT; DUT_PIN5_SET_INPUT; DUT_PIN6_SET_INPUT; DUT_PIN7_SET_INPUT; DUT_PIN8_SET_INPUT; } while (0)

/* DUT 总线全部设置为输出 */
#define DUT_BUS_SET_OUTPUT        do { DUT_PIN4_SET_OUTPUT; DUT_PIN5_SET_OUTPUT; DUT_PIN6_SET_OUTPUT; DUT_PIN7_SET_OUTPUT; DUT_PIN8_SET_OUTPUT; } while (0)

/* 输出数据位 */
#if (DUT_BUS_USE_STM_IO_MACRO == 0)
#define DUT_PIN_OUT(pin, val)     PORT_OUT(HW_DUT_PIN##pin##_DAT) = (val)
#else
#define DUT_PIN_OUT(pin, val)     do { if (val) { STM_IO_SET(HW_DUT_PIN##pin##_DAT); } else { STM_IO_CLR(HW_DUT_PIN##pin##_DAT); } } while (0)
#endif

void DutBus_Init(void);

#endif
