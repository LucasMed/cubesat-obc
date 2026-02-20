// FreeRTOS Configuration for CubeSat OBC on Pico 2W / RP2040
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/**
 * RP2040/Pico 2W Specific Notes:
 * - CPU: ARM Cortex-M33 dual-core @ ~150 MHz  
 * - RAM: 520 KB total SRAM
 * - Tick rate: 1000 Hz (1ms resolution, common for RTOS)
 * - Heap: Set to 32 KB (typical for Pico projects with FreeRTOS)
 * - Priority levels: 5 (0=lowest/idle, 4=highest)
 * - SMP: Dual-core support enabled
 */

// =========================================================================
// Scheduler Configuration
// =========================================================================

#define configUSE_PREEMPTION                    1       // Preemptive scheduling
#define configTICK_RATE_HZ                      1000    // 1000 Hz = 1ms tick
#define configMAX_PRIORITIES                    5       // Priority range: 0-4
#define configUSE_TIME_SLICING                  1       // Tasks of same priority share time
#define configNUMBER_OF_CORES                   2       // RP2350 is dual-core

// =========================================================================
// Memory Configuration
// =========================================================================

/**
 * Heap size: 32 KB (32768 bytes)
 * RP2350 has 520 KB total SRAM. SDK typically uses ~100 KB for other subsystems,
 * leaving ~420 KB  for user code + heap. 32 KB heap allows ~390 KB for other code.
 */
#define configTOTAL_HEAP_SIZE                   (131072) // 128 KB

// Stack size defaults (in words, typically 4 bytes per word on ARM)
#define configMINIMAL_STACK_SIZE                (1024)  // 4KB - massive headroom
#define configTIMER_TASK_STACK_DEPTH            (512)   // 2KB

// Support dynamic memory allocation (standard for FreeRTOS)
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configSUPPORT_STATIC_ALLOCATION         0

// =========================================================================
// Memory Hooks
// =========================================================================

#define configUSE_MALLOC_FAILED_HOOK            1       // Call hook if malloc fails
#define configCHECK_FOR_STACK_OVERFLOW          2       // Check for stack overflow (method 2)

// Assertions for debugging
#define configASSERT(x)                                 \
    if ((x) == 0) {                                     \
        __asm volatile ("cpsid i" : : : "memory");      \
        for(;;);                                        \
    }

// =========================================================================
// Task Name and Statistics
// =========================================================================

#define configMAX_TASK_NAME_LEN                 16      // Task name max length
#define configGENERATE_RUN_TIME_STATS           0       // Disable for now (can enable later for profiling)
#define configUSE_TRACE_FACILITY                0       // Disable trace for now

// =========================================================================
// Optional Features (Most disabled for minimal footprint)
// =========================================================================

// Synchronization primitives
#define configUSE_MUTEXES                       1       // Enable mutex support
#define configUSE_RECURSIVE_MUTEXES             0       // Simplified mutex (no recursion)
#define configUSE_SEMAPHORES                    1       // Enable semaphores
#define configUSE_COUNTING_SEMAPHORES           1       // Enable counting semaphores

// Advanced features (disabled to minimize overhead)
#define configUSE_QUEUE_SETS                    0
#define configUSE_NEWLIB_REENTRANT              0
#define configENABLE_BACKWARD_COMPATIBILITY     1       // For compatibility with older code
#define configUSE_CO_ROUTINES                   0       // Not needed for modern RTOS use

// =========================================================================
// Timer Task Configuration
// =========================================================================

#define configUSE_TIMERS                        1       // Enable software timers
#define configTIMER_TASK_PRIORITY               (configMAX_PRIORITIES - 2)  // Priority 3 (high, but below control)
#define configTIMER_QUEUE_LENGTH                10

// =========================================================================
// Pico SDK Interop (SMP support for dual-core)
// =========================================================================

#define configSUPPORT_PICO_SYNC_INTEROP         1       // Support Pico SDK sync primitives
#define configUSE_CORE_AFFINITY                    1       // Required for SMP and flash safe execute
#define configUSE_TASK_AFFINTY_SET             1       // Enable task affinity set API

// =========================================================================
// Task API Includes (reduce footprint by including only what we use)
// =========================================================================

#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetIdleTaskHandle          1
#define INCLUDE_eTaskGetState                   1
#define INCLUDE_xTaskCreateAffinitySet          1  // Required for SMP core affinity
#define INCLUDE_xEventGroupSetBitsFromISR       1  // Required for SMP port
#define INCLUDE_xTimerPendFunctionCall          1  // Required for xEventGroupSetBitsFromISR

// =========================================================================
// Missing Definitions for Pico SDK SMP Port
// =========================================================================

#define configUSE_16_BIT_TICKS                  0
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_MALLOC_FAILED_HOOK            1
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_PASSIVE_IDLE_HOOK             0

// Hardware specific
#define configCPU_CLOCK_HZ                      ( 150000000UL )

#define configUSE_EVENT_GROUPS                  1

// =========================================================================
// RP2350 Compatibility (for the RP2040 FreeRTOS port)
// =========================================================================

#ifndef SIO_IRQ_PROC0
    #if defined(__ARM_ARCH_8M_MAIN__) || defined(__riscv) || defined(PICO_RP2350)
        // RP2350 both cores use SIO_IRQ_FIFO (25)
        // This hack ensures (SIO_IRQ_PROC0 + core_num) always equals 25
        #define SIO_IRQ_PROC0 (25 - (int)(*(volatile uint32_t*)0xd0000000))
    #endif
#endif

#endif // FREERTOS_CONFIG_H


