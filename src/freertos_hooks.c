/**
 * @file freertos_hooks.c
 * @brief Common FreeRTOS hooks for Pico hardware builds.
 */

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>

#ifdef PICO_BUILD
// ============================================================================
// FreeRTOS Required Hooks
// ============================================================================

void vApplicationTickHook(void) {}
void vApplicationIdleHook(void) {}

void vApplicationMallocFailedHook(void)
{
  printf("FATAL: FreeRTOS malloc failed!\n");
  for (;;)
    ;
}

// cppcheck-suppress constParameterPointer -- signature mandated by FreeRTOS API
void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName)
{
  (void)pxTask;
  printf("FATAL: Stack overflow in task '%s'!\n", pcTaskName);
  for (;;)
    ;
}

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
