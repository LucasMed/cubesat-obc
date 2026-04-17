/**
 * @file wcet_profiler_host.c
 * @brief Host build WCET profiler stub (no-ops).
 *
 * On host builds (unit tests, host firmware), the DWT hardware is not
 * available.  All WCET profiler functions are no-ops.  The host build
 * unit tests should not attempt to include this file — they use the
 * regular host compilation path (no PICO_BUILD).
 *
 * See wcet_profiler.h for the public API documentation.
 */

#include "wcet_profiler.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Stub state (kept minimal — no real measurements on host)            */
/* ------------------------------------------------------------------ */

static bool g_host_initialised = false;
static const char *const g_host_names[WCET_TASK_COUNT] = {
    "SensorRead", "AttitudeCtrl", "Telemetry", "HealthMon", "Command", "GPS", "Payload",
};

static uint32_t g_host_max[WCET_TASK_COUNT];
static uint32_t g_host_samples[WCET_TASK_COUNT];

/* ------------------------------------------------------------------ */
/* Stub implementations                                                 */
/* ------------------------------------------------------------------ */

bool wcet_profiler_init(void)
{
  if (g_host_initialised)
  {
    return true;
  }
  g_host_initialised = true;
  /* No hardware — nothing to initialise */
  return true;
}

void wcet_task_begin(wcet_task_id_t task_id)
{
  (void)task_id;
  /* no-op on host */
}

void wcet_task_end(wcet_task_id_t task_id)
{
  (void)task_id;
  /* no-op on host — cycle counting is not available */
}

bool wcet_get_result(wcet_task_id_t task_id, wcet_result_t *out)
{
  if (!out || task_id >= WCET_TASK_COUNT)
  {
    return false;
  }
  (void)memset(out, 0, sizeof(wcet_result_t));
  out->name = g_host_names[task_id];
  out->max_cycles = g_host_max[task_id];
  out->samples = g_host_samples[task_id];
  return true;
}

void wcet_profiler_print_report(const uint16_t task_period_ms[WCET_TASK_COUNT])
{
  (void)task_period_ms;
  printf("[wcet_profiler] WCET profiling disabled (host build).\n");
  printf("[wcet_profiler] Run on RP2350 hardware for DWT cycle counter measurements.\n");
  (void)fflush(stdout);
}

void wcet_profiler_reset(void)
{
  (void)memset(g_host_max, 0, sizeof(g_host_max));
  (void)memset(g_host_samples, 0, sizeof(g_host_samples));
}
