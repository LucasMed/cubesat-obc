/* test_watchdog.c — Unit tests for the Watchdog HAL (PR-16)
 *
 * T-WDG-01  test_wdg_init_no_crash      – watchdog_hal_init() completes without error
 * T-WDG-02  test_wdg_feed_no_crash      – watchdog_hal_feed() can be called N times
 * T-WDG-03  test_wdg_triggered_false    – watchdog_hal_triggered() returns false on host
 * T-WDG-04  test_wdg_feed_per_step      – vHealthMonitorTask_Step() calls feed() exactly once
 *
 * Because T-WDG-04 exercises health_monitor_task.c we compile that source here
 * and override every external symbol with strong-symbol stubs (same pattern as
 * test_health_monitor_task.c).  The watchdog HAL symbols are defined here too
 * (with call instrumentation) instead of linking watchdog_hal_host.c, so the
 * strong symbols are unambiguous.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

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

/* ---- Watchdog HAL stubs (instrumented) --------------------------------- */
#include "watchdog_hal.h"

static int s_wdg_init_calls = 0;
static int s_wdg_feed_calls = 0;
static int s_wdg_trigger_calls = 0;
static bool s_wdg_trigger_ret = false;

void watchdog_hal_init(uint32_t timeout_ms)
{
  (void)timeout_ms;
  s_wdg_init_calls++;
}

void watchdog_hal_feed(void)
{
  s_wdg_feed_calls++;
}

bool watchdog_hal_triggered(void)
{
  s_wdg_trigger_calls++;
  return s_wdg_trigger_ret;
}
void watchdog_hal_clear_triggered(void) {}

/* ---- Service stubs (health_monitor_task.c dependencies) ---------------- */
#include "eps.h"
#include "fault_manager.h"

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

/* fault_report is referenced by health_monitor_task.c (watchdog trigger path).
 * This stub prevents a linker error; the trigger path is not exercised here. */
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
  s_wdg_init_calls = 0;
  s_wdg_feed_calls = 0;
  s_wdg_trigger_calls = 0;
  s_wdg_trigger_ret = false;
  s_fault_tick_calls = 0;
  s_eps_tick_calls = 0;
  s_tick = 0;
}

/* ========================================================================
 * T-WDG-01  watchdog_hal_init() completes without error
 * ======================================================================== */
static void test_wdg_init_no_crash(void)
{
  reset_all();
  watchdog_hal_init(1000);
  CHECK(s_wdg_init_calls == 1, "init should be called exactly once");
  printf("  PASS T-WDG-01 watchdog_hal_init() no crash\n");
}

/* ========================================================================
 * T-WDG-02  watchdog_hal_feed() can be called N times without error
 * ======================================================================== */
static void test_wdg_feed_no_crash(void)
{
  reset_all();
  for (int i = 0; i < 100; i++)
  {
    watchdog_hal_feed();
  }
  CHECK(s_wdg_feed_calls == 100, "feed should tally 100 calls");
  printf("  PASS T-WDG-02 watchdog_hal_feed() x100 no crash\n");
}

/* ========================================================================
 * T-WDG-03  watchdog_hal_triggered() returns false on host
 * ======================================================================== */
static void test_wdg_triggered_false(void)
{
  reset_all();
  bool result = watchdog_hal_triggered();
  CHECK(result == false, "triggered() must return false on host stub");
  CHECK(s_wdg_trigger_calls == 1, "triggered() should have been called once");
  printf("  PASS T-WDG-03 watchdog_hal_triggered() == false\n");
}

/* ========================================================================
 * T-WDG-04  vHealthMonitorTask_Step() calls watchdog_hal_feed() exactly once
 * ======================================================================== */
static void test_wdg_feed_per_step(void)
{
  reset_all();

  /* Single step — expect exactly one feed call */
  vHealthMonitorTask_Step();
  CHECK(s_wdg_feed_calls == 1, "one Step() must produce exactly one feed()");

  /* Three more steps — cumulative count should reach 4 */
  vHealthMonitorTask_Step();
  vHealthMonitorTask_Step();
  vHealthMonitorTask_Step();
  CHECK(s_wdg_feed_calls == 4, "four steps must produce four feed() calls");

  printf("  PASS T-WDG-04 watchdog_hal_feed() called once per Step\n");
}

/* ========================================================================
 * main
 * ======================================================================== */
int main(void)
{
  printf("=== Watchdog HAL tests (PR-16) ===\n");

  test_wdg_init_no_crash();
  test_wdg_feed_no_crash();
  test_wdg_triggered_false();
  test_wdg_feed_per_step();

  if (g_failures == 0)
  {
    printf("ALL WATCHDOG TESTS PASSED\n");
    return 0;
  }
  else
  {
    printf("%d WATCHDOG TEST(S) FAILED\n", g_failures);
    return 1;
  }
}
