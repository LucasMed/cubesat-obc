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
#define portTICK_PERIOD_MS 1
#define pdTRUE 1
#define pdFALSE 0

// Stub implementations
#ifndef vTaskDelayUntil
  #define vTaskDelayUntil(pxPreviousWakeTime, xTimeIncrement)                                      \
    do                                                                                             \
    {                                                                                              \
      *pxPreviousWakeTime += xTimeIncrement;                                                       \
    } while (0)
#endif

#ifndef vTaskDelay
  #define vTaskDelay(xTicksToDelay)                                                                \
    do                                                                                             \
    {                                                                                              \
    } while (0)
#endif

#ifndef xTaskGetTickCount
  #define xTaskGetTickCount() 0
#endif

#define vTaskStartScheduler()                                                                      \
  do                                                                                               \
  {                                                                                                \
    printf("[FreeRTOS] Scheduler stub (no actual RTOS on host)\\n");                               \
  } while (0)

// Task creation stub — casts pvTaskCode to void* so the function is
// considered "used" by clang-tidy (avoids false unused-function warnings).
#define xTaskCreate(pvTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxCreatedTask)     \
  do                                                                                               \
  {                                                                                                \
    (void)(pvTaskCode);                                                                            \
    printf("[FreeRTOS] Created task: %s (stub)\\n", (pcName));                                     \
  } while (0)

// Task deletion stub
#define vTaskDelete(xTaskToDelete)                                                                 \
  do                                                                                               \
  {                                                                                                \
    (void)(xTaskToDelete);                                                                         \
  } while (0)

#endif  // FREERTOS_H
