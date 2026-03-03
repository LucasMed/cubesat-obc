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
 * ARM HardFault handler — captures the exception stack frame and prints the
 * faulting PC, LR, and CFSR so we can identify the crash site in the map file.
 *
 * The Pico SDK vector table uses "isr_hardfault" (not HardFault_Handler).
 * The naked trampoline selects MSP vs PSP based on EXC_RETURN bit 2, then
 * calls the C handler with a pointer to the saved register frame.
 *
 * Exception frame layout (Cortex-M, basic frame):
 *   [0]=r0  [1]=r1  [2]=r2  [3]=r3
 *   [4]=r12 [5]=lr  [6]=pc  [7]=xpsr
 */
void hardfault_c(uint32_t *frame)
{
  /* CFSR: Configurable Fault Status Register — UFSR | BFSR | MMFSR */
  volatile uint32_t cfsr = *(volatile uint32_t *)0xE000ED28UL;
  printf("FATAL: HardFault!\r\n"
         "  PC  =0x%08" PRIx32 "\r\n"
         "  LR  =0x%08" PRIx32 "\r\n"
         "  R0  =0x%08" PRIx32 "  R1=0x%08" PRIx32 "\r\n"
         "  CFSR=0x%08" PRIx32 "\r\n",
         frame[6], frame[5], frame[0], frame[1], (uint32_t)cfsr);
  fflush(stdout);
  for (;;)
    ;
}

/* naked trampoline: figure out which stack held the exception frame */
void isr_hardfault(void) __attribute__((naked));
void isr_hardfault(void)
{
  __asm volatile("tst   lr, #4        \n" /* EXC_RETURN bit2: 0=MSP, 1=PSP */
                 "ite   eq            \n"
                 "mrseq r0, msp       \n"
                 "mrsne r0, psp       \n"
                 "b     hardfault_c   \n" /* r0 = frame pointer → first arg */
  );
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
