// Minimal stub implementation of FreeRTOS API for host builds
// This allows compilation on Linux without requiring FreeRTOS kernel
#ifndef FREERTOS_H
#define FREERTOS_H

#include <stdint.h>
#include <string.h>

typedef uint32_t TickType_t;
typedef void (*TaskFunction_t)(void *);
typedef void *TaskHandle_t;

#define pdMS_TO_TICKS(xTimeInMs) (xTimeInMs)
#define pdTRUE  1
#define pdFALSE 0

// Stub implementations
#define vTaskDelayUntil(pxPreviousWakeTime, xTimeIncrement) \
    do { *pxPreviousWakeTime += xTimeIncrement; } while(0)

#define xTaskGetTickCount() 0

#define vTaskStartScheduler() do { printf("[FreeRTOS] Scheduler stub (no actual RTOS on host)\\n"); } while(0)

// Task creation stub
#define xTaskCreate(pvTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxCreatedTask) \
    do { \
        printf("[FreeRTOS] Created task: %s (stub)\\n", (pcName)); \
    } while(0)

#endif // FREERTOS_H
