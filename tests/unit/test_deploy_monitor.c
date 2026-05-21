/**
 * @file test_deploy_monitor.c
 * @brief Unit tests for the Deploy Monitor.
 *
 * Tests the public API (init, is_in_progress) and verifies that the
 * deploy monitor module compiles and links correctly on host.
 *
 * Note: The core auto-transition logic lives inside
 * vDeployMonitorTask() which runs an infinite FreeRTOS loop.  Testing
 * the loop logic requires either (a) a thread-capable test harness or
 * (b) extracting decision functions into separate testable units.
 * That refactoring is deferred to a follow-up change.
 *
 * Spec ref: FMM-SPEC-030–067, FMM-DES-001 §5
 */

#include "../../include/deploy_monitor.h"
#include "../../include/data_layer.h"
#include "../../include/flight_mode.h"
#include "../../include/logger.h"

#include <stdio.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Mock stubs for external dependencies                                 */
/* ------------------------------------------------------------------ */

/* flight_mode_manager.c calls log_event() after every transition.
 * Provide a no-op stub so this test can link. */
void log_event(uint16_t event_id, log_class_t log_class, const void *data, uint8_t data_len)
{
  (void)event_id;
  (void)log_class;
  (void)data;
  (void)data_len;
}

/* post.c calls fault_report() on critical POST failure.
 * The real signature uses fault_level_t (uint8_t enum). */
#include "../../include/fault_manager.h"
void fault_report(uint16_t fault_id, fault_level_t level)
{
  (void)fault_id;
  (void)level;
}

/* ------------------------------------------------------------------ */
/* Test helpers                                                        */
/* ------------------------------------------------------------------ */

static int g_failures = 0;

#define CHECK(cond, msg)                                                                           \
  do                                                                                               \
  {                                                                                                \
    if (!(cond))                                                                                   \
    {                                                                                              \
      printf("FAIL [%s:%d]: %s\n", __FILE__, __LINE__, (msg));                                     \
      g_failures++;                                                                                \
    }                                                                                              \
  } while (0)

/* ------------------------------------------------------------------ */
/* Test: deploy_monitor_init and is_in_progress                        */
/* ------------------------------------------------------------------ */

static void test_init(void)
{
  data_layer_init();

  /* Before init, deploy should not be in progress */
  deploy_monitor_init();
  CHECK(!deploy_is_in_progress(), "after init, deploy not in progress");

  printf("test_init: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test: deploy_is_in_progress reflects data layer flag                */
/* ------------------------------------------------------------------ */

static void test_flag_reflection(void)
{
  data_layer_init();

  deploy_monitor_init();
  CHECK(!deploy_is_in_progress(), "initially false");

  /* Set flag via data layer */
  data_layer_set_deploy_in_progress(true);
  CHECK(deploy_is_in_progress(), "true after data_layer_set(1)");

  /* Clear flag */
  data_layer_set_deploy_in_progress(false);
  CHECK(!deploy_is_in_progress(), "false after data_layer_set(0)");

  printf("test_flag_reflection: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test: compile-time macros are sane                                  */
/* ------------------------------------------------------------------ */

static void test_macros(void)
{
  CHECK(DEPLOY_BOOT_SETTLE_MS == 5000u, "BOOT_SETTLE must be 5s");
  CHECK(DEPLOY_DETUMBLE_STABLE_MS == 5000u, "DETUMBLE_STABLE must be 5s");
  CHECK(DEPLOY_TIMEOUT_BOOT_MS == 300000u, "BOOT timeout must be 5 min");
  CHECK(DEPLOY_TIMEOUT_DETUMBLE_MS == 1200000u, "DETUMBLE timeout must be 20 min");
  CHECK(DEPLOY_TIMEOUT_DIAGNOSTIC_MS == 1800000u, "DIAGNOSTIC timeout must be 30 min");
  CHECK(DEPLOY_STABLE_SAMPLES == 50, "STABLE_SAMPLES must be 50 (5000ms / 100ms)");

  printf("test_macros: OK\n");
}

/* ------------------------------------------------------------------ */
/* Entry point                                                         */
/* ------------------------------------------------------------------ */

int main(void)
{
  test_init();
  test_flag_reflection();
  test_macros();

  if (g_failures == 0)
  {
    printf("All Deploy Monitor checks passed.\n");
    return 0;
  }
  printf("%d Deploy Monitor check(s) FAILED.\n", g_failures);
  return 1;
}
