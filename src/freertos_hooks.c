/**
 * @file freertos_hooks.c
 * @brief Common FreeRTOS hooks for Pico hardware builds.
 */

#include "FreeRTOS.h"
#include "task.h"

#include <inttypes.h>
#include <stdio.h>

#ifdef PICO_BUILD
// ============================================================================
// FreeRTOS Required Hooks
// ============================================================================

void vApplicationTickHook(void) {}
void vApplicationIdleHook(void) {}

void vApplicationMallocFailedHook(void)
{
  printf("FATAL: FreeRTOS malloc failed!\r\n");
  fflush(stdout);
  for (;;)
    ;
}

// cppcheck-suppress constParameterPointer -- signature mandated by FreeRTOS API
void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName)
{
  (void)pxTask;
  printf("FATAL: Stack overflow in task '%s'!\r\n", pcTaskName);
  fflush(stdout);
  for (;;)
    ;
}

  #if defined(__arm__) || defined(__thumb__)
/**
 * ARM HardFault handler — prints CFSR + faulting PC without a naked trampoline.
 *
 * FreeRTOS tasks run on PSP.  We read PSP with a safe inline-asm constraint
 * (no naked function, no stack corruption risk) to find the stacked frame and
 * extract the PC.  MSP is the interrupt/exception stack and is not the task
 * frame when a task triggers the fault.
 *
 * Exception frame layout (Cortex-M basic frame, stacked on PSP by hardware):
 *   [0]=r0  [1]=r1  [2]=r2  [3]=r3
 *   [4]=r12 [5]=lr(EXC)  [6]=pc  [7]=xpsr
 *
 * The Pico SDK vector table uses "isr_hardfault" (not HardFault_Handler).
 */
void isr_hardfault(void)
{
  /* CFSR: UsageFault | BusFault | MemManage status bits */
  volatile uint32_t cfsr = *(volatile uint32_t *)0xE000ED28UL;

  /* Read PSP — safe inline-asm, not naked, compiler handles preamble */
  uint32_t psp_val;
  __asm volatile("mrs %0, psp" : "=r"(psp_val));
  const uint32_t *psp_frame = (const uint32_t *)psp_val;

  uint32_t pc = psp_frame[6];
  uint32_t lr = psp_frame[5];

  printf("FATAL: HardFault!\r\n"
         "  PC  =0x%08" PRIx32 "\r\n"
         "  LR  =0x%08" PRIx32 "\r\n"
         "  CFSR=0x%08" PRIx32 "\r\n",
         pc, lr, (uint32_t)cfsr);
  fflush(stdout);
  for (;;)
    ;
}
  #endif

  #if configSUPPORT_STATIC_ALLOCATION

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize)
{
  static StaticTask_t xIdleTaskTCB;
  static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
  *ppxIdleTaskStackBuffer = uxIdleTaskStack;
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

    #if (configNUMBER_OF_CORES > 1)
void vApplicationGetPassiveIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                          StackType_t **ppxIdleTaskStackBuffer,
                                          uint32_t *pulIdleTaskStackSize, BaseType_t xCoreID)
{
  (void)xCoreID;
  static StaticTask_t xPassiveIdleTaskTCBs[configNUMBER_OF_CORES - 1];
  static StackType_t uxPassiveIdleTaskStacks[configNUMBER_OF_CORES - 1][configMINIMAL_STACK_SIZE];
  *ppxIdleTaskTCBBuffer = &xPassiveIdleTaskTCBs[xCoreID];
  *ppxIdleTaskStackBuffer = uxPassiveIdleTaskStacks[xCoreID];
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
    #endif

    #if configUSE_TIMERS == 1
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize)
{
  static StaticTask_t xTimerTaskTCB;
  static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];
  *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
  *ppxTimerTaskStackBuffer = uxTimerTaskStack;
  *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
    #endif  // configUSE_TIMERS

  #endif  // configSUPPORT_STATIC_ALLOCATION

#endif
