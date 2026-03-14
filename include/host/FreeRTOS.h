// Minimal stub implementation of FreeRTOS API for host builds
// This allows compilation on Linux without requiring FreeRTOS kernel
#ifndef FREERTOS_H
#define FREERTOS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
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
  (xTaskCreate_Stub((void *)(pvTaskCode), (pcName)))

static inline BaseType_t xTaskCreate_Stub(void *pvTaskCode, const char *pcName)
{
  (void)pvTaskCode;
  printf("[FreeRTOS] Created task: %s (stub)\n", pcName);
  return pdPASS;
}

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

static inline BaseType_t xTaskNotifyFromISR_Mock(TaskHandle_t xTaskToNotify, uint32_t ulValue,
                                                 eNotifyAction eAction,
                                                 BaseType_t *pxHigherPriorityTaskWoken)
{
  (void)eAction;
  if (xTaskToNotify != NULL)
  {
    *(uint32_t *)xTaskToNotify |= ulValue;
  }
  if (pxHigherPriorityTaskWoken != NULL)
  {
    *pxHigherPriorityTaskWoken = pdFALSE;
  }
  return pdPASS;
}

static inline BaseType_t xTaskNotifyWait_Mock(uint32_t ulBitsToClearOnEntry,
                                              uint32_t ulBitsToClearOnExit,
                                              uint32_t *pulNotificationValue,
                                              TickType_t xTicksToWait)
{
  (void)ulBitsToClearOnEntry;
  (void)ulBitsToClearOnExit;
  (void)xTicksToWait;
  if (pulNotificationValue != NULL)
  {
    *pulNotificationValue = 0;
  }
  return pdPASS;
}

#define xTaskNotifyFromISR(xTaskToNotify, ulValue, eAction, pxHigherPriorityTaskWoken)             \
  xTaskNotifyFromISR_Mock(xTaskToNotify, ulValue, eAction, pxHigherPriorityTaskWoken)

#define xTaskNotifyWait(ulBitsToClearOnEntry, ulBitsToClearOnExit, pulNotificationValue,           \
                        xTicksToWait)                                                              \
  xTaskNotifyWait_Mock(ulBitsToClearOnEntry, ulBitsToClearOnExit, pulNotificationValue,            \
                       xTicksToWait)

#define portYIELD_FROM_ISR(xHigherPriorityTaskWoken) (void)(xHigherPriorityTaskWoken)

// Additional stubs for OBC
#define xTaskGetHandle(pcName) ((TaskHandle_t)((void)(pcName), NULL))
#define xTaskNotify(xTaskToNotify, ulValue, eAction)                                               \
  ((BaseType_t)((void)(xTaskToNotify), (void)(ulValue), (void)(eAction), pdPASS))
#define vTaskPrioritySet(xTask, uxNewPriority)                                                     \
  do                                                                                               \
  {                                                                                                \
    (void)(xTask);                                                                                 \
    (void)(uxNewPriority);                                                                         \
  } while (0)
#define xTaskNotifyGive(xTaskToNotify) ((BaseType_t)((void)(xTaskToNotify), pdPASS))

#endif  // FREERTOS_H
