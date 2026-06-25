/**
 * @file freertos_hooks.c
 * @brief Common FreeRTOS hooks for Pico hardware builds.
 */

#include "FreeRTOS.h"
#include "task.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#ifdef PICO_BUILD
  #include "boot_info.h"
  #include "hardware/gpio.h"
  #include "hardware/watchdog.h"
  #include "internal_flash_layout.h"
  #include "pico/runtime.h"
  #include "post.h"

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
  /* Mark FMM metadata: write a failure record at the end of SRAM
   * so the bootloader or next application boot can read it. */
  boot_info_t bi;
  boot_info_read(&bi);
  bi.boot_reason = POST_BOOT_STACK_OVERFLOW; /* closest match for OOM */

  /* Write a simple failure marker — in a full implementation this would
   * go to the FMM metadata ring in internal flash.  For now, write to
   * the end of the boot_info region to signal the failure. */
  *(volatile uint32_t *)(BOOT_INFO_ADDR + 128) = 0xFADE0F1Eu;

  watchdog_enable(1, false); /* triggers reset in 1 ms */
  for (;;)
    __asm volatile("wfi");
}

/* ── Overflow-info block in BSS — survives the spin-loop so a debugger or
 *    a second-chance printout can read it later.                           ── */
static volatile char g_overflow_task[configMAX_TASK_NAME_LEN + 1];
static volatile uint32_t g_overflow_flag = 0;

// cppcheck-suppress constParameterPointer -- signature mandated by FreeRTOS API
void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName)
{
  (void)pxTask;

  /* 1. Save the offending task name to a known BSS location (for debugger). */
  g_overflow_flag = 0xDEAD0001u;
  for (int i = 0; i < configMAX_TASK_NAME_LEN && pcTaskName[i]; i++)
    g_overflow_task[i] = pcTaskName[i];
  g_overflow_task[configMAX_TASK_NAME_LEN] = '\0';

  /* 2. Store in watchdog scratch registers that survive a soft reset.
   *    scratch[0] = magic, scratch[1..3] = first 12 chars of task name. */
  watchdog_hw->scratch[0] = 0xDEAD0001u;
  /* Copy up to 12 bytes of task name into scratch[1..3], 4 bytes each. */
  uint32_t w = 0;
  for (int i = 0; i < 12; i++)
  {
    uint8_t c = (i < configMAX_TASK_NAME_LEN) ? (uint8_t)pcTaskName[i] : 0;
    w |= ((uint32_t)c) << (8 * (i % 4));
    if ((i % 4) == 3)
    {
      watchdog_hw->scratch[1 + i / 4] = w;
      w = 0;
    }
  }

  /* 2.1 Also mark FMM metadata region for post-mortem analysis. */
  *(volatile uint32_t *)(BOOT_INFO_ADDR + 128) = 0xDEAD0001u;

  /* 3. Force a watchdog reset in 1 ms.  On reboot, main() will check
   *    scratch[0] and print the saved task name before re-initialising. */
  watchdog_enable(1, false);
  for (;;)
    __asm volatile("wfi");
}

  #if defined(__arm__) || defined(__thumb__)
/**
 * ARM HardFault handler — prints CFSR, HFSR, and the faulting PC.
 *
 * Two-stage design:
 *
 *  isr_hardfault  (naked trampoline)
 *    – Runs with NO compiler-generated prologue so LR still holds the
 *      hardware-written EXC_RETURN value.
 *    – Selects the right stack (PSP when bit[2]=1, MSP otherwise) and passes
 *      it as r0 along with EXC_RETURN in r1 to the C helper below.
 *
 *  prvHardFaultHandler  (normal C function)
 *    – Clears CCR.UNALIGN_TRP first so that accessing the stacked frame
 *      cannot trigger a recursive UNALIGNED fault.
 *    – Reads the Cortex-M exception frame: {r0,r1,r2,r3,r12,lr,pc,xpsr}
 *      at indices                           [0][1][2][3][ 4][5][6][ 7]
 *    – Prints PC (frame[6]), task-LR (frame[5]), CFSR, HFSR, EXC_RETURN.
 *
 * The Pico SDK vector table uses the name "isr_hardfault".
 */
void prvHardFaultHandler(const uint32_t *frame, uint32_t exc_return)
{
  /* Step 1: clear CCR.UNALIGN_TRP so PSP frame accesses below can't re-fault. */
  *(volatile uint32_t *)0xE000ED14UL &= ~(1UL << 3);

  /* Step 2: force-release all 32 SIO hardware spinlocks.
   *
   * On RP2350 SMP, the faulting task may have been holding a FreeRTOS spinlock
   * when it crashed.  Core 0 will be spinning inside portENTER_CRITICAL() with
   * PRIMASK=1 (interrupts disabled), waiting to claim that spinlock.  While
   * core 0 has PRIMASK=1 the USB CDC interrupt can never fire, so printf()
   * below blocks forever and produces no output.
   *
   * Writing to a SIO spinlock register (SIO_BASE + 0x100 + 4*N) unconditionally
   * releases it, regardless of which core holds it.  This unblocks core 0:
   * it exits the critical section, restores PRIMASK=0, and starts servicing
   * USB interrupts again so the printf buffer can drain.
   */
  for (uint32_t i = 0u; i < 32u; i++)
  {
    *(volatile uint32_t *)(0xD0000100UL + 4u * i) = 1u;
  }
  __asm volatile("sev" ::: "memory"); /* nudge any WFE-sleeping core */

  /* Step 3: brief busy-wait (~4 ms at 125 MHz) for core 0 to exit its
   * critical section and re-enable interrupts before we call printf. */
  for (volatile uint32_t d = 0u; d < 500000u; d++)
  {
  }

  /* Step 4: read fault registers (safe now that UNALIGN_TRP is clear). */
  uint32_t fault_pc = frame[6];
  uint32_t task_lr = frame[5];
  uint32_t cfsr = *(volatile uint32_t *)0xE000ED28UL;
  uint32_t hfsr = *(volatile uint32_t *)0xE000ED2CUL;
  uint32_t bfar = *(volatile uint32_t *)0xE000ED38UL;  /* valid when BFARVALID set */
  uint32_t mmfar = *(volatile uint32_t *)0xE000ED34UL; /* valid when MMARVALID set */

  printf("FATAL: HardFault!\r\n"
         "  PC=0x%08" PRIx32 "  LR=0x%08" PRIx32 "\r\n"
         "  CFSR=0x%08" PRIx32 "  HFSR=0x%08" PRIx32 "\r\n"
         "  EXC_RETURN=0x%08" PRIx32 "\r\n",
         fault_pc, task_lr, cfsr, hfsr, exc_return);

  /* MemManage faults */
  if (cfsr & 0x00000001u)
    printf("  [MMFSR] IACCVIOL - MPU exec violation\r\n");
  if (cfsr & 0x00000002u)
    printf("  [MMFSR] DACCVIOL - MPU data violation\r\n");
  if (cfsr & 0x00000008u)
    printf("  [MMFSR] MUNSTKERR - MemManage on exception return\r\n");
  if (cfsr & 0x00000010u)
    printf("  [MMFSR] MSTKERR - MemManage on exception entry\r\n");
  if (cfsr & 0x00000080u)
    printf("  MMFAR=0x%08" PRIx32 "\r\n", mmfar);

  /* BusFaults */
  if (cfsr & 0x00000100u)
    printf("  [BFSR] IBUSERR - instruction bus error\r\n");
  if (cfsr & 0x00000200u)
    printf("  [BFSR] PRECISERR - precise data bus error\r\n");
  if (cfsr & 0x00000400u)
    printf("  [BFSR] IMPRECISERR - imprecise data bus error\r\n");
  if (cfsr & 0x00000800u)
    printf("  [BFSR] UNSTKERR - BusFault on exception return\r\n");
  if (cfsr & 0x00001000u)
    printf("  [BFSR] STKERR - BusFault on exception entry\r\n");
  if (cfsr & 0x00008000u)
    printf("  BFAR=0x%08" PRIx32 "\r\n", bfar);

  /* UsageFaults */
  if (cfsr & 0x00010000u)
    printf("  [UFSR] UNDEFINSTR - undefined instruction\r\n");
  if (cfsr & 0x00020000u)
    printf("  [UFSR] INVSTATE - invalid EPSR (bad/NULL function pointer?)\r\n");
  if (cfsr & 0x00040000u)
    printf("  [UFSR] INVPC - bad EXC_RETURN\r\n");
  if (cfsr & 0x00080000u)
    printf("  [UFSR] NOCP - coprocessor access\r\n");
  if (cfsr & 0x00100000u)
    printf("  [UFSR] STKOF - stack overflow (Cortex-M33)\r\n");
  if (cfsr & 0x01000000u)
    printf("  [UFSR] UNALIGNED - unaligned memory access\r\n");
  if (cfsr & 0x02000000u)
    printf("  [UFSR] DIVBYZERO - divide by zero\r\n");

  fflush(stdout);
  for (;;)
    ;
}

/**
 * Naked trampoline: runs with no prologue, so LR = EXC_RETURN as set by
 * hardware.  Passes (frame_ptr, exc_return) to prvHardFaultHandler().
 *
 *   r0 ← PSP  if EXC_RETURN bit[2] = 1  (thread-mode fault, task stack)
 *   r0 ← MSP  if EXC_RETURN bit[2] = 0  (fault in handler,  ISR stack)
 *   r1 ← LR   (= EXC_RETURN)
 */
__attribute__((naked)) void isr_hardfault(void)
{
  __asm volatile("tst   lr, #4          \n" /* test EXC_RETURN bit[2]      */
                 "ite   eq              \n"
                 "mrseq r0, msp         \n" /* bit[2]=0 → frame on MSP     */
                 "mrsne r0, psp         \n" /* bit[2]=1 → frame on PSP     */
                 "mov   r1, lr          \n" /* r1 = EXC_RETURN             */
                 "b     prvHardFaultHandler \n");
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
