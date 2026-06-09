/* test_gps_task.c — Unit tests for vGpsTask_Step() (real GPS task logic)
 *
 * Exercises the real vGpsTask_Step() function by including gps_task.c
 * directly with mocked FreeRTOS, GPS driver, and fault manager.
 *
 * T-GPS-01  Valid fix → data_layer_set_gps_fix called, no fault
 * T-GPS-02  Stale/no-data fix → increments no_data_count
 * T-GPS-03  Repeated stale → counter accumulates
 * T-GPS-04  30 consecutive no-data → FAULT_GPS_TIMEOUT WARNING
 * T-GPS-05  Recovery: valid fix after timeout → clears fault, resets count
 * T-GPS-06  Valid fix with backlog counter → fault_clear called
 * T-GPS-07  New data with non-zero timestamp → counter reset (new_data path)
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

/* ---- GPS stub --------------------------------------------------------- */
#include "gps_driver.h"
#include "gps_stub.h"

/* ---- DLA mocks -------------------------------------------------------- */
#include "data_layer.h"

static GpsFix_t s_dla_fix = {0};
static int s_dla_set_gps_fix_calls = 0;

void data_layer_set_gps_fix(const GpsFix_t *fix)
{
  s_dla_set_gps_fix_calls++;
  if (fix)
    memcpy(&s_dla_fix, fix, sizeof(GpsFix_t));
}

/* ---- Fault Manager mocks --------------------------------------------- */
#include "fault_ids.h"
#include "fault_manager.h"

static int s_fault_report_calls = 0;
static uint16_t s_fault_report_last_id = 0;
static fault_level_t s_fault_report_last_level = 0;
static int s_fault_clear_calls = 0;
static uint16_t s_fault_clear_last_id = 0;

void fault_report(uint16_t id, fault_level_t level)
{
  s_fault_report_calls++;
  s_fault_report_last_id = id;
  s_fault_report_last_level = level;
}

void fault_clear(uint16_t id)
{
  s_fault_clear_calls++;
  s_fault_clear_last_id = id;
}

/* ---- GPS task (unit under test) — included after all mocks ------------ */
#include "../../src/tasks/gps_task.c"

/* ---- Test helpers ----------------------------------------------------- */
static int g_failures = 0;

#define CHECK(cond, msg) \
  do \
  { \
    if (!(cond)) \
    { \
      printf("  FAIL [%s:%d] %s\n", __func__, __LINE__, msg); \
      g_failures++; \
    } \
  } while (0)

static void reset_all(void)
{
  s_tick = 0;
  s_fault_report_calls = 0;
  s_fault_report_last_id = 0;
  s_fault_report_last_level = 0;
  s_fault_clear_calls = 0;
  s_fault_clear_last_id = 0;
  s_dla_set_gps_fix_calls = 0;
  memset(&s_dla_fix, 0, sizeof(s_dla_fix));

  gps_mock_reset_counters();
  gps_mock_set_mode(GPS_MOCK_OK);
}

/* ========================================================================
 * T-GPS-01  Valid fix → data_layer_set_gps_fix called, no fault.
 *           Stub returns timestamp_ms=0, so new_data=false, prev_ts unchanged.
 * ======================================================================== */
static void test_gps_valid_fix(void)
{
  int failures_before = g_failures;
  reset_all();

  uint32_t no_data = 0;
  uint32_t prev_ts = 0;

  vGpsTask_Step(&no_data, &prev_ts);

  CHECK(s_dla_set_gps_fix_calls == 1, "data_layer_set_gps_fix called once");
  CHECK(s_fault_report_calls == 0, "no fault reported for valid fix");
  CHECK(s_fault_clear_calls == 0, "fault_clear not called (counter was 0)");
  CHECK(prev_ts == 0, "prev_timestamp unchanged (stub timestamp_ms=0)");

  printf("[T-GPS-01] test_gps_valid_fix: %s\n",
         g_failures == failures_before ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-GPS-02  Stale fix (valid=false, timestamp_ms=0) → no_data_count++
 * ======================================================================== */
static void test_gps_stale_fix_increments(void)
{
  int failures_before = g_failures;
  reset_all();

  uint32_t no_data = 0;
  uint32_t prev_ts = 0;

  gps_mock_set_mode(GPS_MOCK_FAULT_TIMEOUT);
  vGpsTask_Step(&no_data, &prev_ts);

  CHECK(no_data == 1, "no_data_count incremented to 1 for stale data");
  CHECK(s_dla_set_gps_fix_calls == 0, "data_layer NOT called (fix invalid)");
  CHECK(s_fault_report_calls == 0, "no fault yet (only 1 stale)");
  CHECK(prev_ts == 0, "prev_timestamp unchanged");

  printf("[T-GPS-02] test_gps_stale_fix_increments: %s\n",
         g_failures == failures_before ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-GPS-03  Repeated stale → counter accumulates
 * ======================================================================== */
static void test_gps_repeated_stale(void)
{
  int failures_before = g_failures;
  reset_all();

  uint32_t no_data = 0;
  uint32_t prev_ts = 0;

  gps_mock_set_mode(GPS_MOCK_FAULT_TIMEOUT);
  for (int i = 0; i < 5; i++)
    vGpsTask_Step(&no_data, &prev_ts);

  CHECK(no_data == 5, "no_data_count = 5 after 5 stale steps");
  CHECK(s_fault_report_calls == 0, "no fault yet (only 5 stale)");

  printf("[T-GPS-03] test_gps_repeated_stale: %s\n",
         g_failures == failures_before ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-GPS-04  30 consecutive no-data → FAULT_GPS_TIMEOUT WARNING
 * ======================================================================== */
static void test_gps_timeout_fault(void)
{
  int failures_before = g_failures;
  reset_all();

  uint32_t no_data = 0;
  uint32_t prev_ts = 0;

  gps_mock_set_mode(GPS_MOCK_FAULT_TIMEOUT);

  /* Run 30 iterations — fault triggers on step where count >= GPS_TIMEOUT_TICKS */
  for (int i = 0; i < 30; i++)
    vGpsTask_Step(&no_data, &prev_ts);

  CHECK(no_data == 30, "no_data_count = 30 after 30 stale steps");
  CHECK(s_fault_report_calls >= 1, "fault_report called at least once");
  CHECK(s_fault_report_last_id == FAULT_GPS_TIMEOUT, "fault ID must be GPS_TIMEOUT");
  CHECK(s_fault_report_last_level == FAULT_LEVEL_WARNING, "fault level must be WARNING");

  printf("[T-GPS-04] test_gps_timeout_fault: %s\n",
         g_failures == failures_before ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-GPS-05  Recovery: valid fix after timeout → clears fault, resets count
 * ======================================================================== */
static void test_gps_recovery_after_timeout(void)
{
  int failures_before = g_failures;
  reset_all();

  uint32_t no_data = 0;
  uint32_t prev_ts = 0;

  /* Phase 1: trigger timeout (30 stale steps) */
  gps_mock_set_mode(GPS_MOCK_FAULT_TIMEOUT);
  for (int i = 0; i < 30; i++)
    vGpsTask_Step(&no_data, &prev_ts);
  CHECK(no_data >= 30, "timeout triggered");

  /* Phase 2: valid fix arrives */
  gps_mock_set_mode(GPS_MOCK_OK);
  vGpsTask_Step(&no_data, &prev_ts);

  CHECK(no_data == 0, "no_data_count reset to 0 after valid fix");
  CHECK(s_fault_clear_calls == 1, "fault_clear called once");
  CHECK(s_fault_clear_last_id == FAULT_GPS_TIMEOUT, "cleared FAULT_GPS_TIMEOUT");
  CHECK(s_dla_set_gps_fix_calls == 1, "data_layer_set_gps_fix called for valid fix");

  printf("[T-GPS-05] test_gps_recovery_after_timeout: %s\n",
         g_failures == failures_before ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-GPS-06  Valid fix with backlog counter (>0) → fault_clear + reset
 * ======================================================================== */
static void test_gps_recovery_with_backlog(void)
{
  int failures_before = g_failures;
  reset_all();

  uint32_t no_data = 5;  /* simulate 5 previous stale polls */
  uint32_t prev_ts = 0;

  gps_mock_set_mode(GPS_MOCK_OK);
  vGpsTask_Step(&no_data, &prev_ts);

  CHECK(s_fault_clear_calls == 1, "fault_clear called (had backlog)");
  CHECK(s_fault_clear_last_id == FAULT_GPS_TIMEOUT, "cleared FAULT_GPS_TIMEOUT");
  CHECK(no_data == 0, "no_data_count reset to 0");
  CHECK(s_dla_set_gps_fix_calls == 1, "data_layer_set_gps_fix called");

  printf("[T-GPS-06] test_gps_recovery_with_backlog: %s\n",
         g_failures == failures_before ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-GPS-07  New data with non-zero timestamp → counter reset
 *           (exercises the new_data=true path inside vGpsTask_Step)
 * ======================================================================== */
static void test_gps_new_data_resets_counter(void)
{
  int failures_before = g_failures;
  reset_all();

  uint32_t no_data = 3;  /* existing backlog */
  uint32_t prev_ts = 0;

  /* Set a fix with valid=false but non-zero timestamp → new_data=true */
  GpsFix_t fix;
  memset(&fix, 0, sizeof(fix));
  fix.valid = false;
  fix.satellites = 4;
  fix.timestamp_ms = 1000;
  gps_mock_set_data(&fix);

  vGpsTask_Step(&no_data, &prev_ts);

  /* new_data = (1000 != 0) && (1000 > 0) = true
   * valid = false, new_data = true
   * → "else if (new_data)" → printf + no_data = 0 */
  CHECK(no_data == 0, "no_data_count reset to 0 (new data resets counter)");
  CHECK(s_dla_set_gps_fix_calls == 0, "data_layer NOT called (fix invalid)");
  CHECK(s_fault_clear_calls == 0, "fault_clear NOT called (no valid fix)");
  CHECK(prev_ts == 1000, "prev_timestamp updated to 1000");

  printf("[T-GPS-07] test_gps_new_data_resets_counter: %s\n",
         g_failures == failures_before ? "PASS" : "FAIL");
}

/* ======================================================================== */
int main(void)
{
  printf("=== GPS Task Unit Tests ===\n");
  test_gps_valid_fix();
  test_gps_stale_fix_increments();
  test_gps_repeated_stale();
  test_gps_timeout_fault();
  test_gps_recovery_after_timeout();
  test_gps_recovery_with_backlog();
  test_gps_new_data_resets_counter();
  printf("===========================\n");
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
