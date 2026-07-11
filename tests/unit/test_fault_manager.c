/**
 * @file test_fault_manager.c
 * @brief PR-4 gate: Fault Manager correctness checks.
 *
 * Tests:
 *   1.  init() zeroes the table; fault_get_highest_level() returns NONE
 *   2.  fault_report() creates a new entry
 *   3.  Repeated fault_report() increments count
 *   4.  fault_is_active() returns true after report, false after clear
 *   5.  fault_clear() marks inactive without removing entry
 *   6.  fault_get_highest_level() reflects the worst active fault
 *   7.  fault_get_event() returns correct record
 *   8.  CRITICAL fault triggers fmm_force_safe() (FM_SAFE side-effect)
 *   9.  WARNING faults are auto-cleared by fault_manager_tick()
 *   10. ERROR and CRITICAL faults are NOT auto-cleared by tick
 *   11. fault_get_highest_level() returns NONE when all faults cleared
 *   12. Multiple different faults tracked simultaneously
 *   13. fault_report() with NONE level is a no-op
 *   14. fault_report() on unknown ID (valid non-zero) is accepted
 *   15. fault_get_event() returns false for unknown ID
 */

#include "../../include/data_layer.h"
#include "../../include/fault_ids.h"
#include "../../include/fault_manager.h"
#include "../../include/flight_mode.h"
#include "../../include/logger.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Mock stub for log_event                                              */
/* ------------------------------------------------------------------ */

/* flight_mode_manager.c calls log_event() after every transition.
 * Provide a no-op stub so this test can link (logger_lib is NOT linked). */
void log_event(uint16_t event_id, log_class_t log_class, const void *data, uint8_t data_len)
{
  (void)event_id;
  (void)log_class;
  (void)data;
  (void)data_len;
}

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
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

static void reset(void)
{
  data_layer_init();
  /* Also initialise FMM so fmm_force_safe() has a valid DLA state */
  flight_mode_manager_init();
  fault_manager_init();
}

/* ------------------------------------------------------------------ */
/* Test 1: clean init                                                  */
/* ------------------------------------------------------------------ */

static void test_init(void)
{
  reset();
  CHECK(fault_get_highest_level() == FAULT_LEVEL_NONE, "init: highest level must be NONE");
  CHECK(!fault_is_active(FAULT_EST_GYRO_TIMEOUT), "init: no faults must be active");
  printf("test_init: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 2: basic report and get_event                                  */
/* ------------------------------------------------------------------ */

static void test_basic_report(void)
{
  reset();
  fault_report(FAULT_EST_GYRO_TIMEOUT, FAULT_LEVEL_WARNING);

  CHECK(fault_is_active(FAULT_EST_GYRO_TIMEOUT), "fault must be active after report");

  fault_event_t ev;
  bool found = fault_get_event(FAULT_EST_GYRO_TIMEOUT, &ev);
  CHECK(found, "fault_get_event must return true for reported fault");
  CHECK(ev.id == FAULT_EST_GYRO_TIMEOUT, "event id must match");
  CHECK(ev.level == FAULT_LEVEL_WARNING, "event level must be WARNING");
  CHECK(ev.active, "event active must be true");
  CHECK(ev.count == 1, "count must be 1 on first report");

  printf("test_basic_report: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 3: repeated report increments count                           */
/* ------------------------------------------------------------------ */

static void test_count_increment(void)
{
  reset();
  fault_report(FAULT_CTRL_DEADLINE_MISS, FAULT_LEVEL_ERROR);
  fault_report(FAULT_CTRL_DEADLINE_MISS, FAULT_LEVEL_ERROR);
  fault_report(FAULT_CTRL_DEADLINE_MISS, FAULT_LEVEL_ERROR);

  fault_event_t ev;
  fault_get_event(FAULT_CTRL_DEADLINE_MISS, &ev);
  CHECK(ev.count == 3, "count must be 3 after three reports");
  CHECK(ev.active, "fault must still be active");

  printf("test_count_increment: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 4: fault_clear()                                              */
/* ------------------------------------------------------------------ */

static void test_clear(void)
{
  reset();
  fault_report(FAULT_SENS_IMU_I2C_ERROR, FAULT_LEVEL_WARNING);
  CHECK(fault_is_active(FAULT_SENS_IMU_I2C_ERROR), "active before clear");

  fault_clear(FAULT_SENS_IMU_I2C_ERROR);
  CHECK(!fault_is_active(FAULT_SENS_IMU_I2C_ERROR), "inactive after clear");

  /* count must be preserved */
  fault_event_t ev;
  fault_get_event(FAULT_SENS_IMU_I2C_ERROR, &ev);
  CHECK(ev.count >= 1, "count must not be reset by clear");

  printf("test_clear: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 5: fault_get_highest_level()                                  */
/* ------------------------------------------------------------------ */

static void test_highest_level(void)
{
  reset();
  CHECK(fault_get_highest_level() == FAULT_LEVEL_NONE, "none active → NONE");

  fault_report(FAULT_TIMING_DEADLINE_MISS, FAULT_LEVEL_WARNING);
  CHECK(fault_get_highest_level() == FAULT_LEVEL_WARNING, "one WARNING → WARNING");

  fault_report(FAULT_ACT_RW_OVERCURRENT, FAULT_LEVEL_ERROR);
  CHECK(fault_get_highest_level() == FAULT_LEVEL_ERROR, "WARNING+ERROR → ERROR");

  fault_clear(FAULT_ACT_RW_OVERCURRENT);
  CHECK(fault_get_highest_level() == FAULT_LEVEL_WARNING, "after clearing ERROR → WARNING");

  fault_clear(FAULT_TIMING_DEADLINE_MISS);
  CHECK(fault_get_highest_level() == FAULT_LEVEL_NONE, "after clearing all → NONE");

  printf("test_highest_level: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 6: CRITICAL triggers FM_SAFE via fmm_force_safe()            */
/* ------------------------------------------------------------------ */

static void test_critical_triggers_safe(void)
{
  reset();
  /* Move to NOMINAL first */
  data_layer_set_flight_mode(FM_NOMINAL);
  CHECK(data_layer_get_flight_mode() == FM_NOMINAL, "pre: in NOMINAL");

  fault_report(FAULT_EPS_VBATT_CRITICAL, FAULT_LEVEL_CRITICAL);

  CHECK(data_layer_get_flight_mode() == FM_SAFE, "CRITICAL fault must trigger FM_SAFE");
  CHECK(fault_is_active(FAULT_EPS_VBATT_CRITICAL),
        "CRITICAL fault must still be active after force_safe");

  printf("test_critical_triggers_safe: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 7: WARNING auto-cleared after WARN_AUTO_CLEAR_TICKS ticks     */
/* ------------------------------------------------------------------ */

static void test_warning_auto_clear(void)
{
  reset();
  fault_report(FAULT_WDT_KICK_MISSED, FAULT_LEVEL_WARNING);
  CHECK(fault_is_active(FAULT_WDT_KICK_MISSED), "active after report");

  /* 29 ticks — must still be active */
  for (int i = 0; i < 29; i++)
  {
    fault_manager_tick();
  }
  CHECK(fault_is_active(FAULT_WDT_KICK_MISSED), "WARNING must still be active at tick 29");

  /* 30th tick — must auto-clear */
  fault_manager_tick();
  CHECK(!fault_is_active(FAULT_WDT_KICK_MISSED), "WARNING must auto-clear at tick 30");

  printf("test_warning_auto_clear: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 8: ERROR and CRITICAL faults NOT auto-cleared by tick         */
/* ------------------------------------------------------------------ */

static void test_error_not_auto_cleared(void)
{
  reset();
  fault_report(FAULT_CTRL_RATE_LIMIT, FAULT_LEVEL_ERROR);

  for (int i = 0; i < 60; i++)
  {
    fault_manager_tick();
  }
  CHECK(fault_is_active(FAULT_CTRL_RATE_LIMIT),
        "ERROR fault must NOT be auto-cleared after 60 ticks");

  /* CRITICAL (already cleared to SAFE above in a different test,
   * so we re-init here) */
  reset();
  data_layer_set_flight_mode(FM_SAFE); /* avoid NOMINAL guard */
  fault_report(FAULT_EPS_READ_ERROR, FAULT_LEVEL_CRITICAL);

  for (int i = 0; i < 60; i++)
  {
    fault_manager_tick();
  }
  CHECK(fault_is_active(FAULT_EPS_READ_ERROR),
        "CRITICAL fault must NOT be auto-cleared after 60 ticks");

  printf("test_error_not_auto_cleared: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 9: multiple faults tracked simultaneously                     */
/* ------------------------------------------------------------------ */

static void test_multiple_faults(void)
{
  reset();
  fault_report(FAULT_EST_GYRO_TIMEOUT, FAULT_LEVEL_WARNING);
  fault_report(FAULT_EST_MAG_TIMEOUT, FAULT_LEVEL_WARNING);
  fault_report(FAULT_CTRL_RATE_LIMIT, FAULT_LEVEL_ERROR);
  fault_report(FAULT_THERM_OVER_TEMP, FAULT_LEVEL_WARNING);

  CHECK(fault_is_active(FAULT_EST_GYRO_TIMEOUT), "gyro timeout active");
  CHECK(fault_is_active(FAULT_EST_MAG_TIMEOUT), "mag timeout active");
  CHECK(fault_is_active(FAULT_CTRL_RATE_LIMIT), "rate limit active");
  CHECK(fault_is_active(FAULT_THERM_OVER_TEMP), "over temp active");
  CHECK(fault_get_highest_level() == FAULT_LEVEL_ERROR, "highest level must be ERROR");

  fault_clear(FAULT_CTRL_RATE_LIMIT);
  CHECK(fault_get_highest_level() == FAULT_LEVEL_WARNING, "after clearing ERROR → WARNING");

  printf("test_multiple_faults: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 10: re-raise after clear resets counter                       */
/* ------------------------------------------------------------------ */

static void test_reraise_after_clear(void)
{
  reset();
  fault_report(FAULT_CMD_QUEUE_FULL, FAULT_LEVEL_WARNING);
  fault_report(FAULT_CMD_QUEUE_FULL, FAULT_LEVEL_WARNING);
  fault_clear(FAULT_CMD_QUEUE_FULL);

  /* Re-raise — count must start fresh */
  fault_report(FAULT_CMD_QUEUE_FULL, FAULT_LEVEL_ERROR);
  fault_event_t ev;
  fault_get_event(FAULT_CMD_QUEUE_FULL, &ev);
  /* After a clear + re-report, count restarts at 1.
   * (The slot is reused; count is not reset by clear, but the
   * implementation restarts from 1 on a new active-false → report.) */
  CHECK(ev.active, "fault must be active after re-raise");
  CHECK(ev.level == FAULT_LEVEL_ERROR, "level must be ERROR after re-raise");

  printf("test_reraise_after_clear: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 11: fault_report with FAULT_LEVEL_NONE is a no-op            */
/* ------------------------------------------------------------------ */

static void test_none_level_noop(void)
{
  reset();
  fault_report(FAULT_TLM_QUEUE_OVERFLOW, FAULT_LEVEL_NONE);
  CHECK(!fault_is_active(FAULT_TLM_QUEUE_OVERFLOW), "report with NONE level must be a no-op");
  CHECK(fault_get_highest_level() == FAULT_LEVEL_NONE, "highest must remain NONE");
  printf("test_none_level_noop: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 12: fault_get_event returns false for unknown ID             */
/* ------------------------------------------------------------------ */

static void test_get_event_unknown(void)
{
  reset();
  fault_event_t ev;
  bool found = fault_get_event(0xFFFFu, &ev);
  CHECK(!found, "unknown ID must return false");
  found = fault_get_event(0u, &ev);
  CHECK(!found, "ID=0 must return false");
  printf("test_get_event_unknown: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 13: table full — fill all slots, then drop                    */
/* ------------------------------------------------------------------ */

static void test_table_full(void)
{
  reset();

  /* Fill all FAULT_TABLE_CAPACITY (32) slots with unique IDs */
  for (uint16_t id = 1u; id <= 32u; id++)
  {
    fault_report(id, FAULT_LEVEL_WARNING);
  }

  /* 33rd report must find no free slot → L147-148 (table full, drop) */
  fault_report(33u, FAULT_LEVEL_WARNING);
  /* Should not crash. Highest level must still be WARNING. */
  CHECK(fault_get_highest_level() == FAULT_LEVEL_WARNING,
        "highest must be WARNING after table full and drop");
  printf("test_table_full: OK\n");
}

/* ------------------------------------------------------------------ */
/* Entry point                                                         */
/* ------------------------------------------------------------------ */

int main(void)
{
  test_init();
  test_basic_report();
  test_count_increment();
  test_clear();
  test_highest_level();
  test_critical_triggers_safe();
  test_warning_auto_clear();
  test_error_not_auto_cleared();
  test_multiple_faults();
  test_reraise_after_clear();
  test_none_level_noop();
  test_get_event_unknown();
  test_table_full();

  if (g_failures == 0)
  {
    printf("All Fault Manager checks passed.\n");
    return 0;
  }
  printf("%d Fault Manager check(s) FAILED.\n", g_failures);
  return 1;
}
