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
 * - SMP: Single-core mode (dual-core disabled until boot sequence is stable)
 */

// =========================================================================
// Scheduler Configuration
// =========================================================================

#define configUSE_PREEMPTION 1    // Preemptive scheduling
#define configTICK_RATE_HZ 1000   // 1000 Hz = 1ms tick
#define configMAX_PRIORITIES 5    // Priority range: 0-4
#define configUSE_TIME_SLICING 1  // Tasks of same priority share time
#define configNUMBER_OF_CORES 1   // Single-core for now — re-enable when boot is stable

// =========================================================================
// Memory Configuration
// =========================================================================

/**
 * Heap size: 32 KB (32768 bytes)
 * RP2350 has 520 KB total SRAM. SDK typically uses ~100 KB for other subsystems,
 * leaving ~420 KB  for user code + heap. 32 KB heap allows ~390 KB for other code.
 */
#define configTOTAL_HEAP_SIZE (131072)  // 128 KB

// Stack size defaults (in words, typically 4 bytes per word on ARM)
#define configMINIMAL_STACK_SIZE (1024)      // 4 KB — idle task
#define configTIMER_TASK_STACK_DEPTH (1024)  // 4 KB — "Tmr Svc" needs room for SDK interop

// Support dynamic memory allocation (standard for FreeRTOS)
#define configSUPPORT_DYNAMIC_ALLOCATION 1
#define configSUPPORT_STATIC_ALLOCATION 1

// =========================================================================
// Memory Hooks
// =========================================================================

#define configUSE_MALLOC_FAILED_HOOK 1    // Call hook if malloc fails
#define configCHECK_FOR_STACK_OVERFLOW 2  // Check for stack overflow (method 2)

// Assertions for debugging
// NOTE: intentionally a no-op for Pico builds while diagnosing startup issues.
// Any assertion firing silently kills the system (ISR context + cpsid i).
// xTaskCreate return-value checking in startup tasks provides equivalent coverage.
#define configASSERT(x) ((void)(x))

// =========================================================================
// Task Name and Statistics
// =========================================================================

#define configMAX_TASK_NAME_LEN 16       // Task name max length
#define configGENERATE_RUN_TIME_STATS 0  // Disable for now (can enable later for profiling)
#define configUSE_TRACE_FACILITY 0       // Disable trace for now

// =========================================================================
// Optional Features (Most disabled for minimal footprint)
// =========================================================================

// Synchronization primitives
#define configUSE_MUTEXES 1              // Enable mutex support
#define configUSE_RECURSIVE_MUTEXES 0    // Simplified mutex (no recursion)
#define configUSE_SEMAPHORES 1           // Enable semaphores
#define configUSE_COUNTING_SEMAPHORES 1  // Enable counting semaphores

// Advanced features (disabled to minimize overhead)
#define configUSE_QUEUE_SETS 0
#define configUSE_NEWLIB_REENTRANT 0
#define configENABLE_BACKWARD_COMPATIBILITY 1  // For compatibility with older code
#define configUSE_CO_ROUTINES 0                // Not needed for modern RTOS use

// =========================================================================
// Timer Task Configuration
// =========================================================================

#define configUSE_TIMERS 1  // Enable software timers
#define configTIMER_TASK_PRIORITY                                                                  \
  (configMAX_PRIORITIES - 2)  // Priority 3 (high, but below control)
#define configTIMER_QUEUE_LENGTH 10

// =========================================================================
// Pico SDK Interop / SMP (guarded: only enable when using dual-core)
// =========================================================================

#define configSUPPORT_PICO_SYNC_INTEROP 1  // Support Pico SDK sync primitives

#if configNUMBER_OF_CORES > 1
  /* Core-affinity API — only valid in SMP mode */
  #define configUSE_CORE_AFFINITY 1
  #define configUSE_TASK_AFFINITY_SET 1
  #define INCLUDE_xTaskCreateAffinitySet 1  // SMP core-pinning API
#else
  /* Single-core: affinity API must be disabled */
  #define configUSE_CORE_AFFINITY 0
  #define INCLUDE_xTaskCreateAffinitySet 0
#endif

// =========================================================================
// Task API Includes
// =========================================================================

#define INCLUDE_vTaskSuspend 1
#define INCLUDE_vTaskDelete 1
#define INCLUDE_vTaskDelayUntil 1
#define INCLUDE_vTaskDelay 1
#define INCLUDE_vTaskPrioritySet 1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define INCLUDE_xTaskGetSchedulerState 1
#define INCLUDE_xTaskGetIdleTaskHandle 1
#define INCLUDE_eTaskGetState 1
#define INCLUDE_xEventGroupSetBitsFromISR 1
#define INCLUDE_xTimerPendFunctionCall 1
#define INCLUDE_xTaskGetHandle 1

// =========================================================================
// Missing Definitions for Pico SDK SMP Port
// =========================================================================

#define configUSE_16_BIT_TICKS 0
#define configUSE_IDLE_HOOK 0
#define configUSE_TICK_HOOK 0
#define configUSE_MALLOC_FAILED_HOOK 1
#define configCHECK_FOR_STACK_OVERFLOW 2
#define configUSE_PASSIVE_IDLE_HOOK 0

// Hardware specific
#define configCPU_CLOCK_HZ (150000000UL)

#define configUSE_EVENT_GROUPS 1

// =========================================================================
// RP2350 Compatibility (for the RP2040 FreeRTOS port)
// =========================================================================

// RP2350 (Cortex-M33) does NOT have the SIO hardware divider that RP2040 has.
// The RP2040 FreeRTOS port's PendSV handler saves/restores divider state at
// SIO offsets 0x60-0x74 — these registers don't exist on RP2350 (gap between
// SPINLOCK_ST at 0x5C and INTERP0 at 0x80).  Accessing them may cause a
// BusFault on M33.  Disable divider save/restore for RP2350; the M33 has
// native UDIV/SDIV instructions and doesn't use the SIO divider.
#if defined(PICO_RP2350) || defined(__ARM_ARCH_8M_MAIN__)
  #ifndef PICO_DIVIDER_DISABLE_INTERRUPTS
    #define PICO_DIVIDER_DISABLE_INTERRUPTS 1
  #endif
#endif


#if !defined(PICO_RP2350) && !defined(__ARM_ARCH_8M_MAIN__) && !defined(__riscv)
#ifndef SIO_IRQ_PROC0
    #define SIO_IRQ_PROC0 15
#endif
#endif

#endif  // FREERTOS_CONFIG_H

/* Cortex-M33 specifics */
#define configENABLE_FPU 1
#define configRUN_FREERTOS_SECURE_ONLY 1
#define configENABLE_MPU 0
#define configENABLE_TRUSTZONE 0

#define configPRIO_BITS 3 /* 8 priority levels on Cortex-M33 (optional, often 4 but Pico SDK has defaults) */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 0x07
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configKERNEL_INTERRUPT_PRIORITY (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))


#define vPortSVCHandler isr_svcall
#define xPortPendSVHandler isr_pendsv
#define PendSV_Handler isr_pendsv
#define SVC_Handler isr_svcall
#define SysTick_Handler isr_systick
#define xPortSysTickHandler isr_systick
#define vPortSVCHandler isr_svcall
#define xPortSysTickHandler isr_systick
