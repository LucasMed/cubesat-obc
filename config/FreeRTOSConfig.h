// FreeRTOS Configuration for CubeSat OBC
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

// Processor and compiler specific definitions
#define configUSE_PREEMPTION                    1
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configTICK_RATE_HZ                      1000
#define configMAX_PRIORITIES                    5
#define configMINIMAL_STACK_SIZE                128
#define configTOTAL_HEAP_SIZE                   (8192)
#define configMAX_TASK_NAME_LEN                 16

// Memory management
#define configSUPPORT_STATIC_ALLOCATION         0
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configUSE_MALLOC_FAILED_HOOK            0

// Optional features
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_SEMAPHORES                    1
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_QUEUE_SETS                    0
#define configUSE_TIME_SLICING                  1
#define configUSE_NEWLIB_REENTRANT              0
#define configENABLE_BACKWARD_COMPATIBILITY     1

// Co-routine definitions (typically disabled for modern RTOS usage)
#define configUSE_CO_ROUTINES                   0
#define configMAX_CO_ROUTINE_PRIORITIES         2

// Timer task
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               3
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            256

// Debug and assertions
#define configASSERT(x)         if((x) == 0) { taskDISABLE_INTERRUPTS(); for(;;); }
#define configCHECK_FOR_STACK_OVERFLOW          0

// Function API selection
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetIdleTaskHandle          1

// Optional feature for CPU usage stats
#define configGENERATE_RUN_TIME_STATS           0

#endif // FREERTOS_CONFIG_H
