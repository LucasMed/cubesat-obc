// Minimal stub implementation of FreeRTOS API for host builds
// This allows compilation on Linux without requiring FreeRTOS kernel
#ifndef FREERTOS_H
#define FREERTOS_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef int32_t BaseType_t;
typedef uint32_t TickType_t;
typedef uint32_t UBaseType_t;
typedef void (*TaskFunction_t)(void *);
typedef void *TaskHandle_t;

#define pdMS_TO_TICKS(xTimeInMs) (xTimeInMs)
#define portTICK_PERIOD_MS 1
#define pdTRUE ((BaseType_t)1)
#define pdFALSE ((BaseType_t)0)
#define pdPASS ((BaseType_t)1)
#define pdFAIL ((BaseType_t)0)

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
// Returns pdPASS so the CHK() macro works on host builds.
#define xTaskCreate(pvTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxCreatedTask)     \
  ((void)(pvTaskCode), (void)printf("[FreeRTOS] Created task: %s (stub)\n", (pcName)), pdPASS)

// Task deletion stub
#define vTaskDelete(xTaskToDelete)                                                                 \
  do                                                                                               \
  {                                                                                                \
    (void)(xTaskToDelete);                                                                         \
  } while (0)

// Stack high-water mark stub (returns 0 on host)
#define uxTaskGetStackHighWaterMark(xTask) ((UBaseType_t)((void)(xTask), 0u))

// Free heap stubs (return 0 on host)
#define xPortGetFreeHeapSize() ((size_t)0u)
#define xPortGetMinimumEverFreeHeapSize() ((size_t)0u)

/* ---- Task Notifications (Stub) ---- */

typedef enum
{
  eNoAction = 0,
  eSetBits,
  eIncrement,
  eSetValueWithOverwrite,
  eSetValueWithoutOverwrite
} eNotifyAction;

#define xTaskNotifyFromISR(xTaskToNotify, ulValue, eAction, pxHigherPriorityTaskWoken)             \
  (*(uint32_t *)(xTaskToNotify) |= (ulValue), *(pxHigherPriorityTaskWoken) = pdFALSE, pdPASS)

#define xTaskNotifyWait(ulBitsToClearOnEntry, ulBitsToClearOnExit, pulNotificationValue,           \
                        xTicksToWait)                                                              \
  (*(pulNotificationValue) = *(uint32_t *)(NULL /* Need a real handle mock */), pdPASS)

#define portYIELD_FROM_ISR(xHigherPriorityTaskWoken) (void)(xHigherPriorityTaskWoken)

#endif  // FREERTOS_H
