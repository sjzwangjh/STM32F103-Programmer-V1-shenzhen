/*
 * STM32F10x interrupt handlers used by the programmer app.
 * USB handlers keep the original behavior, and the fault handlers below
 * capture the failure context so boot-to-app startup faults become visible.
 */

#include "stm32f10x.h"
#include "usb_istr.h"
#include "usb_lib.h"
#include "usb_pwr.h"
#include "platform_config.h"

#define FAULT_TYPE_HARD       1U
#define FAULT_TYPE_MEMMANAGE  2U
#define FAULT_TYPE_BUS        3U
#define FAULT_TYPE_USAGE      4U
#define FAULT_INFO_MAGIC      0x46544C54UL

typedef struct
{
  uint32_t magic;
  uint32_t faultType;
  uint32_t stackedR0;
  uint32_t stackedR1;
  uint32_t stackedR2;
  uint32_t stackedR3;
  uint32_t stackedR12;
  uint32_t stackedLr;
  uint32_t stackedPc;
  uint32_t stackedPsr;
  uint32_t currentMsp;
  uint32_t currentPsp;
  uint32_t currentExcReturn;
  uint32_t currentControl;
  uint32_t cfsr;
  uint32_t hfsr;
  uint32_t dfsr;
  uint32_t afsr;
  uint32_t mmfar;
  uint32_t bfar;
  uint32_t shcsr;
} faultCapture_t;

volatile faultCapture_t g_faultCapture;

static void faultGpioInit(void)
{
  RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN;

  GPIOC->CRL &= ~((0xFUL << 0) | (0xFUL << 8));
  GPIOC->CRL |=  ((0x3UL << 0) | (0x3UL << 8));

  GPIOB->CRH &= ~(0xFUL << 4);
  GPIOB->CRH |=  (0x3UL << 4);
}

static void faultBusyDelay(volatile uint32_t count)
{
  while (count-- > 0U) {
    __NOP();
  }
}

static void faultSignalLoop(uint32_t faultType)
{
  uint32_t burst;

  faultGpioInit();

  while (1) {
    for (burst = 0U; burst < faultType; ++burst) {
      GPIOC->BSRR = (1U << 0) | (1U << 2);
      GPIOB->BSRR = (1U << 9);
      faultBusyDelay(1200000U);

      GPIOC->BRR = (1U << 0) | (1U << 2);
      GPIOB->BRR = (1U << 9);
      faultBusyDelay(500000U);
    }

    faultBusyDelay(2200000U);
  }
}

void faultRecordAndHalt(uint32_t *stackedRegs, uint32_t faultType, uint32_t excReturn)
{
  __disable_irq();

  g_faultCapture.magic            = FAULT_INFO_MAGIC;
  g_faultCapture.faultType        = faultType;
  g_faultCapture.stackedR0        = stackedRegs[0];
  g_faultCapture.stackedR1        = stackedRegs[1];
  g_faultCapture.stackedR2        = stackedRegs[2];
  g_faultCapture.stackedR3        = stackedRegs[3];
  g_faultCapture.stackedR12       = stackedRegs[4];
  g_faultCapture.stackedLr        = stackedRegs[5];
  g_faultCapture.stackedPc        = stackedRegs[6];
  g_faultCapture.stackedPsr       = stackedRegs[7];
  g_faultCapture.currentMsp       = __get_MSP();
  g_faultCapture.currentPsp       = __get_PSP();
  g_faultCapture.currentExcReturn = excReturn;
  g_faultCapture.currentControl   = __get_CONTROL();
  g_faultCapture.cfsr             = SCB->CFSR;
  g_faultCapture.hfsr             = SCB->HFSR;
  g_faultCapture.dfsr             = SCB->DFSR;
  g_faultCapture.afsr             = SCB->AFSR;
  g_faultCapture.mmfar            = SCB->MMFAR;
  g_faultCapture.bfar             = SCB->BFAR;
  g_faultCapture.shcsr            = SCB->SHCSR;

  faultSignalLoop(faultType);
}

__asm void HardFault_Handler(void)
{
  IMPORT faultRecordAndHalt
  TST LR, #4
  ITE EQ
  MRSEQ R0, MSP
  MRSNE R0, PSP
  MOV R2, LR
  MOV R1, #1
  B faultRecordAndHalt
}

__asm void MemManage_Handler(void)
{
  IMPORT faultRecordAndHalt
  TST LR, #4
  ITE EQ
  MRSEQ R0, MSP
  MRSNE R0, PSP
  MOV R2, LR
  MOV R1, #2
  B faultRecordAndHalt
}

__asm void BusFault_Handler(void)
{
  IMPORT faultRecordAndHalt
  TST LR, #4
  ITE EQ
  MRSEQ R0, MSP
  MRSNE R0, PSP
  MOV R2, LR
  MOV R1, #3
  B faultRecordAndHalt
}

__asm void UsageFault_Handler(void)
{
  IMPORT faultRecordAndHalt
  TST LR, #4
  ITE EQ
  MRSEQ R0, MSP
  MRSNE R0, PSP
  MOV R2, LR
  MOV R1, #4
  B faultRecordAndHalt
}

void USB_HP_CAN1_TX_IRQHandler(void)
{
}

void USB_LP_CAN1_RX0_IRQHandler(void)
{
  USB_Istr();
}

void USBWakeUp_IRQHandler(void)
{
  EXTI->PR |= 1U << 18;
}
