/**
 * @file wcet_profiler.h
 * @brief WCET Profiler — DWT cycle counter instrumentation for FreeRTOS tasks.
 *
 * Provides worst-case execution time (WCET) measurement for all 7+ FreeRTOS
 * tasks using the ARM Cortex-M33 DWT (Data Watchpoint and Trace) unit on the
 * RP2350. DWT->CYCCNT counts CPU cycles at 133 MHz with ~7.5 ns resolution.
 *
 * Integration model:
 *
 *   Before each task's main work section (after vTaskDelayUntil):
 *
 *     void vSensorReadTask(void *pv) {
 *         ...
 *         vTaskDelayUntil(&xLastWakeTime, xFrequency);
 *         wcet_task_begin(WCET_TASK_SENSOR_READ);
 *         vSensorReadTask_Step();           // <-- measured section
 *         wcet_task_end(WCET_TASK_SENSOR_READ);
 *         ...
 *     }
 *
 *   At any point (e.g. at the end of HealthMon or Telemetry period):
 *
 *     wcet_profiler_print_report();   // prints table to stdout
 *
 *   The profiler can be queried programmatically:
 *
 *     wcet_result_t r;
 *     wcet_get_result(WCET_TASK_SENSOR_READ, &r);
 *     // r.max_cycles, r.avg_cycles, r.samples, r.wcet_us, r.cpu_load_ppt
 *
 * Platform implementations:
 *   PICO_BUILD (RP2350) : src/services/wcet/wcet_profiler_pico.c   (DWT)
 *   Host / unit-test    : src/services/wcet/wcet_profiler_host.c   (no-ops)
 *
 * CDR reference: CDR-HW-06, OI-3 (FSW-SDD-001), OI-3 (OBC-DES-001)
 *
 * Spec ref: ARM Cortex-M33 TRM (DDI0553) §C1.2 — DWT Program Counter Sampling
 *
 * @attention WCET measurements are valid ONLY on hardware (PICO_BUILD).
 *            On host builds, all functions are no-ops.
 */

#ifndef WCET_PROFILER_H
#define WCET_PROFILER_H

#include <stdbool.h>
#include <stdint.h>

/** CPU frequency for cycle-to-time conversion (RP2350 Core 0 @ 133 MHz). */
#define WCET_CPU_HZ 133000000U

/** Maximum number of tasks that can be instrumented simultaneously. */
#define WCET_MAX_TASKS 12U

/**
 * @brief Task identifiers — extend as needed.
 *
 * Values must be < WCET_MAX_TASKS.
 */
typedef enum
{
  WCET_TASK_SENSOR_READ = 0,
  WCET_TASK_ATTITUDE_CTRL,
  WCET_TASK_TELEMETRY,
  WCET_TASK_HEALTH_MON,
  WCET_TASK_COMMAND,
  WCET_TASK_GPS,
  WCET_TASK_PAYLOAD,
  WCET_TASK_COUNT /* sentinel — total number of registered tasks */
} wcet_task_id_t;

/**
 * @brief Result structure for a measured task.
 */
typedef struct
{
  const char *name;      /**< Human-readable task name */
  uint32_t max_cycles;   /**< Worst-case observed cycle count */
  uint32_t avg_cycles;   /**< Running average cycle count */
  uint32_t samples;      /**< Number of measurements collected */
  uint32_t wcet_us;      /**< WCET in microseconds (= max_cycles / (WCET_CPU_HZ/1e6)) */
  uint32_t cpu_load_ppt; /**< CPU load in per-mille (‰) — 1000 = 100 % */
} wcet_result_t;

/**
 * @brief Enable/disable WCET profiling at compile time.
 *
 * Uncomment the following line to enable DWT cycle-counter profiling.
 * When disabled (default), all wcet_* functions become static inline no-ops.
 *
 * The "Unknown destination type (ARM/Thumb)" linker error occurs when
 * the WCET profiler object files are not linked correctly.  Disabling
 * here is the fastest way to get a working build while debugging the
 * linker issue.
 */
/* #define WCET_ENABLED 1 */

#ifdef WCET_ENABLED

/**
 * @brief Initialise the DWT cycle counter and WCET profiler.
 *
 * Must be called once during system start-up, before the FreeRTOS scheduler
 * starts (or at any point before the first measured task).
 *
 * On RP2350: enables DWT->CYCCNT (sets DWT->CTRL |= DWT_CTRL_CYCEVTENA_Msk).
 * On host  : no-op.
 *
 * @return true  Counter enabled successfully.
 * @return false Counter could not be enabled (should never happen on RP2350).
 */
bool wcet_profiler_init(void);

/**
 * @brief Mark the start of a task's measured work section.
 *
 * Captures DWT->CYCCNT into an internal start-time register for the given
 * task. Call at the beginning of the measured section.
 *
 * ISR-safe: Yes (single read from DWT->CYCCNT).
 *
 * @param task_id  Task identifier (must be < WCET_TASK_COUNT).
 */
void wcet_task_begin(wcet_task_id_t task_id);

/**
 * @brief Mark the end of a task's measured work section.
 *
 * Computes elapsed cycles = DWT->CYCCNT - saved_start, then updates
 * running statistics (max, average, sample count) for the given task.
 *
 * ISR-safe: Yes (single read from DWT->CYCCNT).
 *
 * @param task_id  Task identifier (must be < WCET_TASK_COUNT).
 */
void wcet_task_end(wcet_task_id_t task_id);

/**
 * @brief Retrieve the current measurement results for a task.
 *
 * @param task_id  Task identifier.
 * @param out      Output structure (filled by the function).  Must not be NULL.
 *
 * @return true  Results returned successfully.
 * @return false Invalid task_id or NULL out pointer.
 */
bool wcet_get_result(wcet_task_id_t task_id, wcet_result_t *out);

/**
 * @brief Print a WCET measurement report to stdout.
 *
 * Format:
 *
 *     === WCET Report (DWT CYCCNT @ 133 MHz) ===
 *     Task             | WCET(cycles) | WCET(us) | Avg(us) | Samples | CPU Load
 *     -----------------|--------------|----------|---------|---------|----------
 *     SensorRead       |    123456    |    928   |   891   |   500   |   8.9 ‰
 *     AttitudeCtrl     |    789012    |   5932   |  5641   |   500   |  56.4 ‰
 *     ...
 *     WCET measurement complete.
 *
 * On host builds: prints "WCET profiling disabled (host build)."
 *
 * @param task_period_ms  Array of task periods (ms) indexed by wcet_task_id_t.
 *                        Used to compute CPU load. Pass NULL to use default
 *                        periods (1000 ms for all tasks).
 */
void wcet_profiler_print_report(const uint16_t task_period_ms[WCET_TASK_COUNT]);

/**
 * @brief Reset all collected WCET statistics.
 *
 * Clears max_cycles, avg_cycles, and samples for all tasks.
 * Useful after a mode change or before a new measurement window.
 *
 * On host builds: no-op.
 */
void wcet_profiler_reset(void);

#else /* WCET_ENABLED */

/* When WCET profiling is disabled, all functions become static inline no-ops.
 * This eliminates the "undefined reference" linker errors while keeping
 * the instrumentation calls in the source code (they just do nothing). */

static inline bool wcet_profiler_init(void)
{
  return true;
}
static inline void wcet_task_begin(wcet_task_id_t task_id)
{
  (void)task_id;
}
static inline void wcet_task_end(wcet_task_id_t task_id)
{
  (void)task_id;
}
static inline bool wcet_get_result(wcet_task_id_t task_id, wcet_result_t *out)
{
  (void)task_id;
  (void)out;
  return false;
}
static inline void wcet_profiler_print_report(const uint16_t task_period_ms[WCET_TASK_COUNT])
{
  (void)task_period_ms;
}
static inline void wcet_profiler_reset(void) {}

#endif /* WCET_ENABLED */

#endif /* WCET_PROFILER_H */
