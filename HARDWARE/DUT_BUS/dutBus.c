#include "dutBus.h"

void DutBus_Init(void)
{
    // VPP 控制引脚初始化-------悬空----------
    // VPP_VH控制引脚初始化
    PORT_RCC_CLK(HW_DUT_VPP_VH_ON);
    PORT_SET_DIR_PP(HW_DUT_VPP_VH_ON);
    PORT_OUT(HW_DUT_VPP_VH_ON) = 0;     // 初始化-关闭
    // VPP_VL控制引脚初始化
    PORT_RCC_CLK(HW_DUT_VPP_VL_ON);
    PORT_SET_DIR_PP(HW_DUT_VPP_VL_ON);
    PORT_OUT(HW_DUT_VPP_VL_ON) = 0;     // 初始化-关闭

    // VDD 控制引脚初始化--------悬空---------
    // VDD_VH控制引脚初始化
    PORT_RCC_CLK(HW_DUT_VDD_VH_ON);
    PORT_SET_DIR_PP(HW_DUT_VDD_VH_ON);
    PORT_OUT(HW_DUT_VDD_VH_ON) = 0;
    // VDD_VL控制引脚初始化
    PORT_RCC_CLK(HW_DUT_VDD_VL_ON);
    PORT_SET_DIR_PP(HW_DUT_VDD_VL_ON);
    PORT_OUT(HW_DUT_VDD_VL_ON) = 0;

    // 总线方向控制引脚初始化-全部初始化为输入----
    // 设置PIN4引脚下拉输入
    PORT_RCC_CLK(HW_DUT_PIN4_DAT);
    PORT_SET_DIR_IN_PD(HW_DUT_PIN4_DAT);
    // 设置控制端为输入
    PORT_RCC_CLK(HW_DUT_PIN4_CTRL);
    PORT_SET_DIR_PP(HW_DUT_PIN4_CTRL);
    PORT_OUT(HW_DUT_PIN4_CTRL) = 0;     // DUT-PIN4设置为输入模式

    // 设置PIN5引脚下拉输入
    PORT_RCC_CLK(HW_DUT_PIN5_DAT);
    PORT_SET_DIR_IN_PD(HW_DUT_PIN5_DAT);
    // 设置控制端为输入
    PORT_RCC_CLK(HW_DUT_PIN5_CTRL);
    PORT_SET_DIR_PP(HW_DUT_PIN5_CTRL);
    PORT_OUT(HW_DUT_PIN5_CTRL) = 0;     // DUT-PIN5设置为输入模式

    // 设置PIN6引脚下拉输入
    PORT_RCC_CLK(HW_DUT_PIN6_DAT);
    PORT_SET_DIR_IN_PD(HW_DUT_PIN6_DAT);
    // 设置控制端为输入
    PORT_RCC_CLK(HW_DUT_PIN6_CTRL);
    PORT_SET_DIR_PP(HW_DUT_PIN6_CTRL);
    PORT_OUT(HW_DUT_PIN6_CTRL) = 0;     // DUT-PIN6设置为输入模式

    // 设置PIN7引脚下拉输入
    PORT_RCC_CLK(HW_DUT_PIN7_DAT);
    PORT_SET_DIR_IN_PD(HW_DUT_PIN7_DAT);
    // 设置控制端为输入
    PORT_RCC_CLK(HW_DUT_PIN7_CTRL);
    PORT_SET_DIR_PP(HW_DUT_PIN7_CTRL);
    PORT_OUT(HW_DUT_PIN7_CTRL) = 0;     // DUT-PIN7设置为输入模式

    // 设置PIN8引脚下拉输入
    PORT_RCC_CLK(HW_DUT_PIN8_DAT);
    PORT_SET_DIR_IN_PD(HW_DUT_PIN8_DAT);
    // 设置控制端为输入
    PORT_RCC_CLK(HW_DUT_PIN8_CTRL);
    PORT_SET_DIR_PP(HW_DUT_PIN8_CTRL);
    PORT_OUT(HW_DUT_PIN8_CTRL) = 0;     // DUT-PIN8设置为输入模式

}

