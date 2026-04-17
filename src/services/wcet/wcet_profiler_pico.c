/**
 * @file wcet_profiler_pico.c
 * @brief RP2350 DWT cycle counter WCET profiler implementation.
 *
 * Uses the ARM Cortex-M33 DWT (Data Watchpoint and Trace) unit on the
 * RP2350 to measure worst-case execution time of FreeRTOS tasks.
 *
 * DWT->CYCCNT counts CPU cycles at 133 MHz (~7.5 ns resolution) from
 * the point the counter is enabled.  The counter is free-running and
 * wraps at 2^32 cycles (~32 s at 133 MHz — far above any single task
 * execution time).
 *
 * Usage:
 *   1. Call wcet_profiler_init() once at start-up.
 *   2. Wrap each task's work section:
 *        wcet_task_begin(WCET_TASK_SENSOR_READ);
 *        vSensorReadTask_Step();
 *        wcet_task_end(WCET_TASK_SENSOR_READ);
 *   3. Call wcet_profiler_print_report() periodically (e.g. every 100 cycles).
 *
 * ISR-safety: DWT->CYCCNT is read with a single LDR instruction which is
 * atomic on ARMv8-M.  All WCET profiler state is static (no heap, no locks).
 *
 * Limitations:
 *   - Measures wall-clock cycles including any cycles stolen by interrupts.
 *     True WCET should be measured with interrupts disabled (or use
 *     DWT->EXCCNT to subtract exception cycles).
 *   - DWT is per-core.  On SMP builds both cores share DWT; measurements
 *     on Core 1 may include Core 0 activity.
 *
 * Spec ref: ARM Cortex-M33 TRM (DDI0553) §C1.2, RP2350 datasheet §4.7
 * CDR ref:  CDR-HW-06, OI-3 (FSW-SDD-001), OI-3 (OBC-DES-001)
 */

#ifdef PICO_BUILD

  #include "wcet_profiler.h"

  #include <stdio.h>
  #include <string.h>

/* ------------------------------------------------------------------ */
/* CMSIS DWT / CoreDebug registers (ARM Cortex-M33)                   */
/* ------------------------------------------------------------------ */

/*
 * ARMv8-M CoreDebug base address.
 * CMSIS defines this in core_cm33.h.
 */
  #ifndef CoreDebug_BASE
    #define CoreDebug_BASE 0xE000EDF0UL
  #endif

/*
 * DWT (Data Watchpoint and Trace) base address.
 * CMSIS defines this in core_cm33.h.
 */
  #ifndef DWT_BASE
    #define DWT_BASE 0xE0001000UL
  #endif

/*
 * Convenience macros for DWT register access via memory-mapped I/O.
 * volatile uint32_t* cast ensures each access is a distinct load/store.
 */
  #define DWT_CTRL_REG (*(volatile uint32_t *)(DWT_BASE + 0x0000U))   /* Control */
  #define DWT_CYCCNT_REG (*(volatile uint32_t *)(DWT_BASE + 0x0004U)) /* Cycle Count */
  #define DWT_LAR_REG (*(volatile uint32_t *)(DWT_BASE + 0x0FB8U))    /* Lock Access (ARMv8-M) */
  #define DWT_CYCCNTENA (1UL << 0U) /* Bit 0: Cycle counter enable */

/*
 * CoreDebug DEMCR (Debug Exception and Monitor Control Register).
 * TRCENA (Trace Enable, bit 24) must be set before accessing DWT.
 */
  #ifndef CoreDebug_DEMCR
    #define CoreDebug_DEMCR (*(volatile uint32_t *)(CoreDebug_BASE + 0x0CU))
    #define CoreDebug_DEMCR_TRCENA_Pos 24U
    #define CoreDebug_DEMCR_TRCENA_Msk (1UL << CoreDebug_DEMCR_TRCENA_Pos)
  #endif

/* ------------------------------------------------------------------ */
/* Internal profiler state                                              */
/* ------------------------------------------------------------------ */

typedef struct
{
  const char *name;
  uint32_t start_cycles; /**< Saved DWT->CYCCNT at task begin  */
  uint32_t max_cycles;   /**< Worst-case observed cycle count   */
  uint64_t sum_cycles;   /**< Accumulated sum for running avg   */
  uint32_t samples;      /**< Number of measurements            */
} wcet_slot_t;

static wcet_slot_t g_slots[WCET_TASK_COUNT];

/* Task name table — indexed by wcet_task_id_t */
static const char *const g_task_names[WCET_TASK_COUNT] = {
    "SensorRead",   /* WCET_TASK_SENSOR_READ    */
    "AttitudeCtrl", /* WCET_TASK_ATTITUDE_CTRL  */
    "Telemetry",    /* WCET_TASK_TELEMETRY      */
    "HealthMon",    /* WCET_TASK_HEALTH_MON     */
    "Command",      /* WCET_TASK_COMMAND        */
    "GPS",          /* WCET_TASK_GPS            */
    "Payload",      /* WCET_TASK_PAYLOAD        */
};

/* Profiler state */
static bool g_initialised = false;

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

bool wcet_profiler_init(void)
{
  if (g_initialised)
  {
    return true;
  }

  /*
   * RP2350 requires the DWT Lock Access Register (DWT->LAR) to be
   * written before DWT registers can be accessed (ARMv8-M security
   * feature).  The magic value 0xC5ACCE55 unlocks the register.
   */
  DWT_LAR_REG = 0xC5ACCE55UL;

  /*
   * Enable trace infrastructure via CoreDebug DEMCR TRCENA.
   * This is required before any DWT register is writable.
   */
  CoreDebug_DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

  /*
   * Enable the DWT cycle counter.
   * Bit 0 of DWT->CTRL: CYCCNTENA (Cycle Counter Enable).
   */
  DWT_CTRL_REG |= DWT_CYCCNTENA;

  /* Zero the counter to start from a known baseline */
  DWT_CYCCNT_REG = 0U;

  /* Initialise all task slots */
  (void)memset(g_slots, 0, sizeof(g_slots));
  for (size_t i = 0; i < WCET_TASK_COUNT; i++)
  {
    g_slots[i].name = g_task_names[i];
  }

  g_initialised = true;

  printf("[wcet_profiler] DWT CYCCNT enabled @ %u Hz\n", WCET_CPU_HZ);
  (void)fflush(stdout);

  return true;
}

void wcet_task_begin(wcet_task_id_t task_id)
{
  if (!g_initialised || task_id >= WCET_TASK_COUNT)
  {
    return;
  }

  /* DWT->CYCCNT read is atomic on ARMv8-M — no lock needed */
  g_slots[task_id].start_cycles = DWT_CYCCNT_REG;
}

void wcet_task_end(wcet_task_id_t task_id)
{
  if (!g_initialised || task_id >= WCET_TASK_COUNT)
  {
    return;
  }

  uint32_t end_cycles = DWT_CYCCNT_REG;
  uint32_t elapsed = end_cycles - g_slots[task_id].start_cycles;

  wcet_slot_t *s = &g_slots[task_id];
  if (elapsed > s->max_cycles)
  {
    s->max_cycles = elapsed;
  }
  s->sum_cycles += elapsed;
  s->samples++;
}

bool wcet_get_result(wcet_task_id_t task_id, wcet_result_t *out)
{
  if (!out || task_id >= WCET_TASK_COUNT)
  {
    return false;
  }

  wcet_slot_t *s = &g_slots[task_id];
  out->name = s->name;
  out->max_cycles = s->max_cycles;
  out->avg_cycles = (s->samples > 0U) ? (uint32_t)(s->sum_cycles / s->samples) : 0U;
  out->samples = s->samples;

  /* Convert cycles to microseconds (133 MHz → divide by 133) */
  out->wcet_us = s->max_cycles / (WCET_CPU_HZ / 1000000U);

  return true;
}

void wcet_profiler_print_report(const uint16_t task_period_ms[WCET_TASK_COUNT])
{
  if (!g_initialised)
  {
    printf("[wcet_profiler] Not initialised.\n");
    return;
  }

  /*
   * Default task periods if caller does not supply them.
   * These match the FreeRTOS configuration in config.h.
   */
  static const uint16_t s_default_periods[WCET_TASK_COUNT] = {
      100,  /* SensorRead    10 Hz */
      100,  /* AttitudeCtrl   10 Hz */
      1000, /* Telemetry    1000 ms */
      1000, /* HealthMon     1000 ms */
      0,    /* Command     event-driven */
      1000, /* GPS          1000 ms */
      1000, /* Payload      1000 ms */
  };

  const uint16_t *periods = task_period_ms ? task_period_ms : s_default_periods;

  /* Fixed-point arithmetic: cycles per period / period_ns × 1000 (‰) */
  /*   cpu_load_ppt = (max_cycles / period_ms) / (WCET_CPU_HZ / 1e6) × 1000  */
  /*               = max_cycles × 1000 / period_ms / (WCET_CPU_HZ / 1e6)       */
  /*               = max_cycles × 1e3 / period_ms × 1e6 / WCET_CPU_HZ         */
  /*               = max_cycles × 1e9 / period_ms / WCET_CPU_HZ               */
  /* For WCET_CPU_HZ=133e6, use 64-bit to avoid overflow:                     */
  /*   max_cycles × 1000000 / (period_ms × 133) gives ‰                        */

  printf("\n");
  printf("=== WCET Report (DWT CYCCNT @ %u Hz) ===\n", WCET_CPU_HZ);
  printf("%-16s | %11s | %8s | %8s | %7s | %9s\n", "Task", "WCET(cycles)", "WCET(us)", "Avg(us)",
         "Samples", "CPU Load");
  printf("%s-|-%s-|-%s-|-%s-|-%s-|-%s\n", "----------------", "-------------", "--------",
         "--------", "-------", "---------");

  uint32_t total_cpu_load_ppt = 0U;

  for (size_t i = 0; i < WCET_TASK_COUNT; i++)
  {
    wcet_slot_t *s = &g_slots[i];
    uint32_t wcet_us = s->max_cycles / (WCET_CPU_HZ / 1000000U);
    uint32_t avg_us =
        (s->samples > 0U) ? (uint32_t)(s->sum_cycles / s->samples / (WCET_CPU_HZ / 1000000U)) : 0U;
    uint32_t cpu_load_ppt = 0U;

    if (periods[i] > 0 && s->max_cycles > 0U)
    {
      /* cpu_load_ppt = (wcet_cycles / period_s) / (WCET_CPU_HZ) × 1000‰ */
      /*             = wcet_cycles × 1000 / (period_ms × WCET_CPU_HZ / 1000) */
      /*             = wcet_cycles × 1_000_000 / (period_ms × WCET_CPU_HZ)  */
      cpu_load_ppt = (uint32_t)(((uint64_t)s->max_cycles * 1000000ULL) /
                                ((uint64_t)periods[i] * (uint64_t)WCET_CPU_HZ));
      total_cpu_load_ppt += cpu_load_ppt;
    }

    printf("%-16s | %11u | %8u | %8u | %7u | %7u.%01u ‰\n", s->name, s->max_cycles, wcet_us, avg_us,
           s->samples, cpu_load_ppt / 10, cpu_load_ppt % 10);
  }

  printf("%s-+-%s-+-%s-+-%s-+-%s-+-%s\n", "----------------", "-------------", "--------",
         "--------", "-------", "---------");
  printf("%-16s | %11s | %8s | %8s | %7s | %7u.%01u ‰\n", "TOTAL CPU LOAD", "—", "—", "—", "—",
         total_cpu_load_ppt / 10, total_cpu_load_ppt % 10);
  printf("WCET measurement complete.\n");
  (void)fflush(stdout);
}

void wcet_profiler_reset(void)
{
  if (!g_initialised)
  {
    return;
  }
  (void)memset(g_slots, 0, sizeof(g_slots));
  for (size_t i = 0; i < WCET_TASK_COUNT; i++)
  {
    g_slots[i].name = g_task_names[i];
  }
  DWT_CYCCNT_REG = 0U; /* Reset the hardware counter too */
}

#endif /* PICO_BUILD */
