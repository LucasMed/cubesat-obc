/**
 * @file test_deploy_monitor.c
 * @brief Unit tests for the Deploy Monitor — includes deploy_monitor.c
 *        directly so FreeRTOS mocks via #define take effect.
 *
 * Tests public API (init, is_in_progress), compile-time macros, and
 * auto-transition/timeout decision logic.
 *
 * Spec ref: FMM-SPEC-030–067, FMM-DES-001 §5
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* FreeRTOS mocks — must come before including deploy_monitor.c */
#include "FreeRTOS.h"
#include "task.h"

static uint32_t s_tick = 0;

static uint32_t mock_xTaskGetTickCount(void)
{
  return s_tick;
}
static void mock_vTaskDelayUntil(uint32_t *prev, uint32_t inc)
{
  (void)prev;
  s_tick += inc;
}

#undef xTaskGetTickCount
#define xTaskGetTickCount mock_xTaskGetTickCount

/* log_event stub (flight_mode_manager calls it) */
#include "logger.h"
void log_event(uint16_t event_id, log_class_t log_class, const void *data, uint8_t data_len)
{
  (void)event_id;
  (void)log_class;
  (void)data;
  (void)data_len;
}

/* fault_report stub (post.c calls it) */
#include "../../include/fault_manager.h"
void fault_report(uint16_t fault_id, fault_level_t level)
{
  (void)fault_id;
  (void)level;
}

/* Override weak default in flight_mode_manager.c — configurable per test */
static fault_level_t s_mock_fault_level = FAULT_LEVEL_NONE;
fault_level_t fault_get_highest_level(void)
{
  return s_mock_fault_level;
}

/* Include the unit under test directly so FreeRTOS mocks apply */
#include "../../src/services/fmm/deploy_monitor.c"
#include "../../include/data_layer.h"
#include "../../include/flight_mode.h"

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
/* Helpers for transition tests                                        */
/* ------------------------------------------------------------------ */

static void reset_transition_test(void)
{
  data_layer_init();
  deploy_monitor_init();
  s_tick = 0;
  s_mock_fault_level = FAULT_LEVEL_NONE;
  /* Set mode entry tick so elapsed time is controlled */
  data_layer_set_mode_entry_tick(0);
}

/* ------------------------------------------------------------------ */
/* Test: Auto BOOT → DETUMBLE after settling time with valid POST    */
/* ------------------------------------------------------------------ */

static void test_auto_boot_to_detumble(void)
{
  int failures_before = g_failures;

  /* Need POST to be valid: set magic + all critical POST bits passed
   * (IMU=bit0, RTC=bit4, Flash=bit8), and IMU valid. */
  post_record_t post_rec;
  memset(&post_rec, 0, sizeof(post_rec));
  post_rec.magic = POST_MAGIC;
  post_rec.test_bitmap = (1u << 0) | (1u << 4) | (1u << 8); /* IMU+RTC+Flash */
  post_rec.boot_count = 1;
  data_layer_set_post_last(&post_rec);

  data_layer_set_sensor_avail(true, false);
  /* data_layer_write_imu sets imu_valid=true (checked by deploy_monitor_step) */
  float att[3] = {0}, rates[3] = {0};
  data_layer_write_imu(att, rates);
  data_layer_set_flight_mode(FM_BOOT);
  data_layer_set_deploy_in_progress(false);
  data_layer_set_mode_entry_tick(0);
  s_tick = pdMS_TO_TICKS(DEPLOY_BOOT_SETTLE_MS + 100); /* past settle time */

  /* Run the step */
  deploy_monitor_step();

  /* After settling, should have transitioned to DETUMBLE and set deploy flag */
  flight_mode_t mode = data_layer_get_flight_mode();
  bool deploy = data_layer_get_deploy_in_progress();
  CHECK(mode == FM_DETUMBLE, "auto BOOT → DETUMBLE transition must happen after settle time");
  CHECK(deploy == true, "deploy_in_progress must be set after auto transition");

  printf("test_auto_boot_to_detumble: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test: No transition if POST has critical fail                      */
/* ------------------------------------------------------------------ */

static void test_no_transition_on_critical_post(void)
{
  int failures_before = g_failures;

  /* Simulate critical POST failure */
  post_record_t post_rec;
  memset(&post_rec, 0, sizeof(post_rec));
  post_rec.magic = POST_MAGIC;
  post_rec.test_bitmap = 0; /* all tests failed */
  post_rec.boot_reason = POST_BOOT_WATCHDOG;
  data_layer_set_post_last(&post_rec);

  data_layer_set_flight_mode(FM_BOOT);
  data_layer_set_deploy_in_progress(false);
  data_layer_set_sensor_avail(true, false);
  data_layer_set_mode_entry_tick(0);
  s_tick = pdMS_TO_TICKS(DEPLOY_BOOT_SETTLE_MS + 100);

  deploy_monitor_step();

  flight_mode_t mode = data_layer_get_flight_mode();
  bool deploy = data_layer_get_deploy_in_progress();
  CHECK(mode == FM_BOOT, "must NOT transition from BOOT with critical POST failure");
  CHECK(deploy == false, "deploy flag must remain false");

  printf("test_no_transition_on_critical_post: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test: Mode timeout — BOOT exceeds DEPLOY_TIMEOUT_BOOT_MS → SAFE   */
/* ------------------------------------------------------------------ */

static void test_boot_timeout_to_safe(void)
{
  int failures_before = g_failures;

  data_layer_set_flight_mode(FM_BOOT);
  data_layer_set_deploy_in_progress(true);
  data_layer_set_mode_entry_tick(0);
  s_tick = pdMS_TO_TICKS(DEPLOY_TIMEOUT_BOOT_MS + 100);

  deploy_monitor_step();

  flight_mode_t mode = data_layer_get_flight_mode();
  bool deploy = data_layer_get_deploy_in_progress();
  CHECK(mode == FM_SAFE, "BOOT timeout must transition to FM_SAFE");
  CHECK(deploy == false, "deploy flag must be cleared on timeout to SAFE");

  printf("test_boot_timeout_to_safe: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test: DIAGNOSTIC timeout → NOMINAL (fallback, not SAFE)            */
/* ------------------------------------------------------------------ */

static void test_diagnostic_timeout_to_nominal(void)
{
  int failures_before = g_failures;

  data_layer_set_flight_mode(FM_DIAGNOSTIC);
  data_layer_set_deploy_in_progress(false);
  data_layer_set_mode_entry_tick(0);
  s_tick = pdMS_TO_TICKS(DEPLOY_TIMEOUT_DIAGNOSTIC_MS + 100);

  deploy_monitor_step();

  flight_mode_t mode = data_layer_get_flight_mode();
  /* DIAGNOSTIC timeout → NOMINAL */
  CHECK(mode == FM_NOMINAL, "DIAGNOSTIC timeout must transition to FM_NOMINAL");

  printf("test_diagnostic_timeout_to_nominal: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test: DETUMBLE — high omega triggers hard reset                    */
/* ------------------------------------------------------------------ */

static void test_detumble_high_omega_hard_reset(void)
{
  int failures_before = g_failures;
  data_layer_init();
  deploy_monitor_init();
  s_detumble_stable_count = 0;

  data_layer_set_flight_mode(FM_DETUMBLE);
  float att[3] = {0.0f, 0.0f, 0.0f};
  float rates[3] = {0.15f, 0.0f, 0.0f}; /* omega = 0.15 > 0.10 */
  data_layer_write_imu(att, rates);

  /* Prime counter > 0 */
  s_detumble_stable_count = 10;

  deploy_monitor_step();
  CHECK(s_detumble_stable_count == 0, "hard reset must zero the stable counter");

  printf("test_detumble_high_omega_hard_reset: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test: DETUMBLE — medium omega triggers leaky decrement             */
/* ------------------------------------------------------------------ */

static void test_detumble_medium_omega_leaky_decrement(void)
{
  int failures_before = g_failures;
  data_layer_init();
  deploy_monitor_init();
  s_detumble_stable_count = 0;

  data_layer_set_flight_mode(FM_DETUMBLE);
  float att[3] = {0.0f, 0.0f, 0.0f};
  /* omega = 0.07 → between THRESHOLD (0.05) and HARD_RESET (0.10) */
  float rates[3] = {0.07f, 0.0f, 0.0f};
  data_layer_write_imu(att, rates);

  s_detumble_stable_count = 10;

  deploy_monitor_step();
  /* Leaky decrement: 10 → 9 */
  CHECK(s_detumble_stable_count == 9, "leaky decrement must reduce counter by 1");

  printf("test_detumble_medium_omega_leaky_decrement: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test: DETUMBLE — stable omega → counter increments toward NOMINAL  */
/* ------------------------------------------------------------------ */

static void test_detumble_stable_counter_accumulates(void)
{
  int failures_before = g_failures;
  data_layer_init();
  deploy_monitor_init();
  s_detumble_stable_count = 0; /* Reset counter from previous tests */

  data_layer_set_flight_mode(FM_DETUMBLE);
  float att[3] = {0.0f, 0.0f, 0.0f};
  /* omega = 0.01 → below THRESHOLD → stable */
  float rates[3] = {0.01f, 0.0f, 0.0f};
  data_layer_write_imu(att, rates);

  CHECK(s_detumble_stable_count == 0, "counter starts at 0");

  deploy_monitor_step();
  CHECK(s_detumble_stable_count == 1, "counter incremented to 1 after stable step");

  printf("test_detumble_stable_counter_accumulates: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test: Boot→DETUMBLE transition failure when fault blocks            */
/* ------------------------------------------------------------------ */

static void test_boot_to_detumble_transition_failure(void)
{
  /* Valid POST + IMU */
  post_record_t post_rec;
  memset(&post_rec, 0, sizeof(post_rec));
  post_rec.magic = POST_MAGIC;
  post_rec.test_bitmap = (1u << 0) | (1u << 4) | (1u << 8);
  data_layer_set_post_last(&post_rec);
  float att[3] = {0}, rates[3] = {0};
  data_layer_write_imu(att, rates);
  data_layer_set_flight_mode(FM_BOOT);
  data_layer_set_deploy_in_progress(false);
  data_layer_set_mode_entry_tick(0);
  s_tick = pdMS_TO_TICKS(DEPLOY_BOOT_SETTLE_MS + 100);

  /* Block all transitions via CRITICAL fault */
  s_mock_fault_level = FAULT_LEVEL_CRITICAL;

  deploy_monitor_step();

  /* Mode should still be BOOT — transition was blocked */
  flight_mode_t mode = data_layer_get_flight_mode();
  bool deploy = data_layer_get_deploy_in_progress();
  CHECK(mode == FM_BOOT, "BOOT→DETUMBLE must be blocked by CRITICAL fault");
  CHECK(deploy == false, "deploy flag must remain false when transition is blocked");

  printf("test_boot_to_detumble_transition_failure: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test: DETUMBLE→NOMINAL transition failure when fault blocks        */
/* ------------------------------------------------------------------ */

static void test_detumble_to_nominal_transition_failure(void)
{
  data_layer_init();
  deploy_monitor_init();
  s_detumble_stable_count = 0;

  data_layer_set_flight_mode(FM_DETUMBLE);
  float att[3] = {0.0f, 0.0f, 0.0f};
  float rates[3] = {0.01f, 0.0f, 0.0f}; /* omega < threshold */
  data_layer_write_imu(att, rates);

  /* Accumulate enough stable samples to trigger transition */
  s_detumble_stable_count = DEPLOY_STABLE_SAMPLES;

  /* Block all transitions via CRITICAL fault */
  s_mock_fault_level = FAULT_LEVEL_CRITICAL;

  deploy_monitor_step();

  /* State must remain unchanged — transition was blocked */
  flight_mode_t mode = data_layer_get_flight_mode();
  CHECK(mode == FM_DETUMBLE, "DETUMBLE→NOMINAL must be blocked by CRITICAL fault");
  CHECK(s_detumble_stable_count == DEPLOY_STABLE_SAMPLES,
        "stable_count must NOT be reset on transition failure");

  printf("test_detumble_to_nominal_transition_failure: OK\n");
}

/* ------------------------------------------------------------------ */
/* Entry point                                                         */
/* ------------------------------------------------------------------ */

int main(void)
{
  test_init();
  test_flag_reflection();
  test_macros();
  test_auto_boot_to_detumble();
  test_no_transition_on_critical_post();
  test_boot_timeout_to_safe();
  test_diagnostic_timeout_to_nominal();
  test_detumble_high_omega_hard_reset();
  test_detumble_medium_omega_leaky_decrement();
  test_detumble_stable_counter_accumulates();
  test_boot_to_detumble_transition_failure();
  test_detumble_to_nominal_transition_failure();

  if (g_failures == 0)
  {
    printf("All Deploy Monitor checks passed.\n");
    return 0;
  }
  printf("%d Deploy Monitor check(s) FAILED.\n", g_failures);
  return 1;
}
