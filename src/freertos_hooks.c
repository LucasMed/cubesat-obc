/**
 * @file freertos_hooks.c
 * @brief Common FreeRTOS hooks for Pico hardware builds.
 */

#include "FreeRTOS.h"
#include "task.h"

#include <inttypes.h>
#include <stdio.h>

#ifdef PICO_BUILD
  #include "pico/runtime.h"

// ============================================================================
// Per-core runtime initialiser
// ============================================================================

/**
 * Clear CCR.UNALIGN_TRP on the current core.
 *
 * Registered as a PICO_RUNTIME_INIT_FUNC_PER_CORE at priority "00052" so it
 * runs on BOTH cores:
 *   - Core 0: called by the C-runtime initialisation chain before main().
 *   - Core 1: called in core1_wrapper() via runtime_run_per_core_initializers()
 *             before the FreeRTOS scheduler entry function (i.e. before any
 *             user task can execute).
 *
 * Priority "00052" is intentionally one step after the Pico SDK's builtin
 * PICO_RUNTIME_INIT_PER_CORE_BOOTROM_RESET ("00051") which calls
 * BOOTROM_STATE_RESET_CURRENT_CORE — that call RE-SETS CCR.UNALIGN_TRP to 1
 * (the RP2350 bootrom default).  Clearing it here at "00052" ensures the bit
 * stays cleared on both cores throughout the application's lifetime.
 *
 * Without this, core 1 hits a UsageFault (CFSR=0x01000000 UNALIGNED) on its
 * first context switch, which escalates to HardFault, dead-locks the FreeRTOS
 * SMP scheduler spinlock, and silently freezes both cores.
 */
static void prvClearUnalignTrap(void)
{
  /* CCR is at 0xE000ED14 in the Private Peripheral Bus — each core has its own
   * physical copy via the per-core PPB mapping. */
  *(volatile uint32_t *)0xE000ED14UL &= ~(1UL << 3); /* clear UNALIGN_TRP */
}
PICO_RUNTIME_INIT_FUNC_PER_CORE(prvClearUnalignTrap, "00052");

// ============================================================================
// FreeRTOS Required Hooks
// ============================================================================

void vApplicationTickHook(void) {}
void vApplicationIdleHook(void) {}

void vApplicationMallocFailedHook(void)
{
  printf("FATAL: FreeRTOS malloc failed!\r\n");
  fflush(stdout);
  for (;;)
    ;
}

// cppcheck-suppress constParameterPointer -- signature mandated by FreeRTOS API
void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName)
{
  (void)pxTask;
  printf("FATAL: Stack overflow in task '%s'!\r\n", pcTaskName);
  fflush(stdout);
  for (;;)
    ;
}

  #if defined(__arm__) || defined(__thumb__)
/**
 * ARM HardFault handler — prints CFSR without touching the exception frame.
 *
 * IMPORTANT: Do NOT read PSP here.  If the fault was triggered by a stack
 * overflow the PSP already points to corrupt/unmapped memory; dereferencing
 * it causes a second HardFault → RP2350 lockup reset → all USB output lost.
 *
 * CFSR (0xE000ED28) is an SCB register in the fixed System Control Space —
 * always readable regardless of stack state, so it is safe to read here.
 * It tells us *why* the fault fired which is enough to diagnose the cause.
 *
 * The Pico SDK vector table uses "isr_hardfault" (not HardFault_Handler).
 */
void isr_hardfault(void)
{
  /* CFSR: UsageFault [31:16] | BusFault [15:8] | MemManage [7:0] */
  uint32_t cfsr = *(volatile uint32_t *)0xE000ED28UL;
  /* HFSR: HardFault Status Register — bit 30 = FORCED (escalated fault) */
  uint32_t hfsr = *(volatile uint32_t *)0xE000ED2CUL;

  printf("FATAL: HardFault!\r\n"
         "  CFSR=0x%08" PRIx32 "  HFSR=0x%08" PRIx32 "\r\n",
         cfsr, hfsr);
  /* CFSR decode hints printed separately so each fits on one line */
  if (cfsr & 0x00000001u)
    printf("  [MMFSR] IACCVIOL - exec from non-executable region\r\n");
  if (cfsr & 0x00000002u)
    printf("  [MMFSR] DACCVIOL - data access violation\r\n");
  if (cfsr & 0x00000008u)
    printf("  [MMFSR] MUNSTKERR - MemManage on exception return\r\n");
  if (cfsr & 0x00000010u)
    printf("  [MMFSR] MSTKERR - MemManage on exception entry (stack overflow?)\r\n");
  if (cfsr & 0x00010000u)
    printf("  [UFSR] UNDEFINSTR - undefined instruction\r\n");
  if (cfsr & 0x00020000u)
    printf("  [UFSR] INVSTATE - invalid EPSR (NULL/bad function pointer)\r\n");
  if (cfsr & 0x00040000u)
    printf("  [UFSR] INVPC - bad EXC_RETURN\r\n");
  if (cfsr & 0x00080000u)
    printf("  [UFSR] NOCP - coprocessor access\r\n");
  if (cfsr & 0x00100000u)
    printf("  [UFSR] STKOF - stack overflow (Cortex-M33)\r\n");
  if (cfsr & 0x01000000u)
    printf("  [UFSR] UNALIGNED - unaligned memory access (CCR.UNALIGN_TRP is set)\r\n");
  if (cfsr & 0x02000000u)
    printf("  [UFSR] DIVBYZERO - divide by zero\r\n");
  fflush(stdout);
  for (;;)
    ;
}
  #endif

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
