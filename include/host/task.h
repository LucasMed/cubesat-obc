// Minimal stub for FreeRTOS task API
#ifndef TASK_H
#define TASK_H

typedef void *TaskHandle_t;

#ifndef tskIDLE_PRIORITY
  #define tskIDLE_PRIORITY ((unsigned int)0U)
#endif

#ifndef configMAX_PRIORITIES
  #define configMAX_PRIORITIES 5
#endif

#ifndef vTaskSuspend
  #define vTaskSuspend(xTaskToSuspend)                                                             \
    do                                                                                             \
    {                                                                                              \
      (void)(xTaskToSuspend);                                                                      \
    } while (0)
#endif

#ifndef vTaskPrioritySet
  #define vTaskPrioritySet(xTask, uxNewPriority)                                                   \
    do                                                                                             \
    {                                                                                              \
      (void)(xTask);                                                                               \
      (void)(uxNewPriority);                                                                       \
    } while (0)
#endif

#endif  // TASK_H
