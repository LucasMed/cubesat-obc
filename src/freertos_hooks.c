/**
 * @file freertos_hooks.c
 * @brief Common FreeRTOS hooks for Pico hardware builds.
 */

#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"

#ifdef PICO_BUILD
// ============================================================================
// FreeRTOS Required Hooks
// ============================================================================

void vApplicationTickHook(void) {}
void vApplicationIdleHook(void) {}

void vApplicationMallocFailedHook(void) {
    printf("FATAL: FreeRTOS malloc failed!\n");
    for (;;);
}

void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName) {
    (void)pxTask;
    printf("FATAL: Stack overflow in task '%s'!\n", pcTaskName);
    for (;;);
}
#endif
