/* test_health_monitor_task.c — Unit tests for vHealthMonitorTask_Step() (PR-10)
 *
 * T-HM-01  test_hm_fault_tick_called    – fault_manager_tick() invoked once per Step
 * T-HM-02  test_hm_eps_tick_called      – eps_monitor_tick() invoked once per Step
 * T-HM-03  test_hm_ticks_scale_with_steps – N Steps → N calls to each tick
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ---- Logger stub (flight_mode_manager.c calls log_event) --------------- */
#include "logger.h"

void log_event(uint16_t event_id, log_class_t log_class, const void *data, uint8_t data_len)
{
  (void)event_id;
  (void)log_class;
  (void)data;
  (void)data_len;
}

/* ---- FreeRTOS mocks ---------------------------------------------------- */
#include "FreeRTOS.h"
#include "task.h"

static uint32_t s_tick = 0;

static uint32_t mock_xTaskGetTickCount(void)
{
  return s_tick;
}
static void mock_vTaskDelayUntil(uint32_t *prev, uint32_t inc)
{
  *prev += inc;
  s_tick += inc;
}

#undef xTaskGetTickCount
#undef vTaskDelayUntil
#define xTaskGetTickCount mock_xTaskGetTickCount
#define vTaskDelayUntil mock_vTaskDelayUntil

/* ---- Service stubs ----------------------------------------------------- */
/* These strong-symbol definitions replace the real fault_manager_tick() and
 * eps_monitor_tick() at link time, allowing call-count verification without
 * pulling in the full service stack. */

#include "eps.h"
#include "fault_manager.h"
#include "watchdog_hal.h"

static int s_fault_tick_calls = 0;
static int s_eps_tick_calls = 0;

void fault_manager_tick(void)
{
  s_fault_tick_calls++;
}
void eps_monitor_tick(void)
{
  s_eps_tick_calls++;
}
/* Watchdog is a no-op in this test; feed-per-step coverage is in test_watchdog. */
void watchdog_hal_init(uint32_t timeout_ms)
{
  (void)timeout_ms;
}
void watchdog_hal_feed(void) {}
bool watchdog_hal_triggered(void)
{
  return false;
}
void watchdog_hal_clear_triggered(void) {}

/* fault_report is never reached (watchdog_hal_triggered returns false above),
 * but we need the symbol for the linker since health_monitor_task.c now
 * references it. */
void fault_report(uint16_t id, fault_level_t level)
{
  (void)id;
  (void)level;
}

/* ---- Unit under test --------------------------------------------------- */
#include "health_monitor_task.h"

/* ---- Test helpers ------------------------------------------------------- */
static int g_failures = 0;

#define CHECK(cond, msg)                                                                           \
  do                                                                                               \
  {                                                                                                \
    if (!(cond))                                                                                   \
    {                                                                                              \
      printf("  FAIL [%s:%d] %s\n", __func__, __LINE__, msg);                                      \
      g_failures++;                                                                                \
    }                                                                                              \
  } while (0)

static void reset_all(void)
{
  s_fault_tick_calls = 0;
  s_eps_tick_calls = 0;
  s_tick = 0;
}

/* ========================================================================
 * T-HM-01  fault_manager_tick() is called exactly once per Step
 * ======================================================================== */
static void test_hm_fault_tick_called(void)
{
  int failures_before = g_failures;
  reset_all();

  vHealthMonitorTask_Step();

  CHECK(s_fault_tick_calls == 1, "fault_manager_tick called once");

  printf("[T-HM-01] test_hm_fault_tick_called: %s\n",
         g_failures == failures_before ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-HM-02  eps_monitor_tick() is called exactly once per Step
 * ======================================================================== */
static void test_hm_eps_tick_called(void)
{
  int failures_before = g_failures;
  reset_all();

  vHealthMonitorTask_Step();

  CHECK(s_eps_tick_calls == 1, "eps_monitor_tick called once");

  printf("[T-HM-02] test_hm_eps_tick_called: %s\n",
         g_failures == failures_before ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-HM-03  N successive Step calls → exactly N calls to each tick
 * ======================================================================== */
static void test_hm_ticks_scale_with_steps(void)
{
  int failures_before = g_failures;
  reset_all();

  const int N = 5;
  for (int i = 0; i < N; i++)
    vHealthMonitorTask_Step();

  CHECK(s_fault_tick_calls == N, "fault_manager_tick count equals step count");
  CHECK(s_eps_tick_calls == N, "eps_monitor_tick count equals step count");

  printf("[T-HM-03] test_hm_ticks_scale_with_steps: %s\n",
         g_failures == failures_before ? "PASS" : "FAIL");
}

/* ======================================================================== */
int main(void)
{
  printf("=== Health Monitor Task Unit Tests ===\n");
  test_hm_fault_tick_called();
  test_hm_eps_tick_called();
  test_hm_ticks_scale_with_steps();
  printf("======================================\n");
  if (g_failures == 0)
  {
    printf("All tests passed!\n");
    return 0;
  }
  else
  {
    printf("%d test(s) FAILED.\n", g_failures);
    return 1;
  }
}
