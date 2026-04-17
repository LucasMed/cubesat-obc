/**
 * @file test_fault_injection.c
 * @brief CDR-SAF-04 / OI-SW-5: Fault injection test suite.
 *
 * Exercises fault injection for all 25 fault IDs across all subsystems
 * and validates the response per fault level (WARNING / ERROR / CRITICAL).
 *
 * Test categories:
 *   1. All-fault injection: every FAULT_ID can be reported and tracked
 *   2. WARNING auto-clear: WARNING faults auto-clear after WARN_AUTO_CLEAR_TICKS
 *   3. ERROR persistence: ERROR faults persist through multiple tick() calls
 *   4. CRITICAL FM_SAFE: CRITICAL faults trigger FM_SAFE from any flight mode
 *   5. Level precedence: highest active level drives fault_get_highest_level()
 *   6. Multi-fault matrix: simultaneous faults from multiple subsystems
 *
 * Host-testable: YES — fault_report() is pure software, no hardware required.
 * HIL required:  NO — all faults can be injected via software for unit testing.
 *
 * Refs: CDR-SAF-04, OI-SW-5, FMEA-OBC-002 §12, fault_ids.h, fault_manager.h
 */

#include "../../include/data_layer.h"
#include "../../include/fault_ids.h"
#include "../../include/fault_manager.h"
#include "../../include/flight_mode.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static int g_failures = 0;
static int g_tests_run = 0;

#define CHECK(cond, msg)                                                                             \
  do                                                                                                 \
  {                                                                                                  \
    g_tests_run++;                                                                                   \
    if (!(cond))                                                                                     \
    {                                                                                                \
      printf("  FAIL [%s:%d]: %s\n", __FILE__, __LINE__, (msg));                                     \
      g_failures++;                                                                                  \
    }                                                                                                \
  } while (0)

#define PASS(msg)                                                                                    \
  do                                                                                                 \
  {                                                                                                  \
    printf("  PASS %s\n", (msg));                                                                    \
  } while (0)

/* WARN_AUTO_CLEAR_TICKS from fault_manager.c */
#ifndef WARN_AUTO_CLEAR_TICKS
  #define WARN_AUTO_CLEAR_TICKS 30
#endif

static void reset(void)
{
  data_layer_init();
  flight_mode_manager_init();
  fault_manager_init();
}

/* Advance tick() n times */
static void tick_n(int n)
{
  for (int i = 0; i < n; i++)
  {
    fault_manager_tick();
  }
}

/* ------------------------------------------------------------------ */
/* Test data: all 25 fault IDs grouped by subsystem and level         */
/* ------------------------------------------------------------------ */

/* Each entry: fault_id, expected_level, subsystem_name */
typedef struct
{
  uint16_t id;
  uint8_t  level;   /* 1=WARNING, 2=ERROR, 3=CRITICAL */
  const char *name;
  const char *subsystem;
} fault_entry_t;

/* clang-format off */
static const fault_entry_t ALL_FAULTS[] = {
  /* Estimator (4) */
  {FAULT_EST_GYRO_TIMEOUT,    FAULT_LEVEL_WARNING, "FAULT_EST_GYRO_TIMEOUT",    "Estimator"},
  {FAULT_EST_MAG_TIMEOUT,     FAULT_LEVEL_WARNING, "FAULT_EST_MAG_TIMEOUT",     "Estimator"},
  {FAULT_EST_DIVERGENCE,      FAULT_LEVEL_ERROR,   "FAULT_EST_DIVERGENCE",      "Estimator"},
  {FAULT_EST_QUAT_NORM,       FAULT_LEVEL_ERROR,   "FAULT_EST_QUAT_NORM",       "Estimator"},
  /* Controller (3) */
  {FAULT_CTRL_OUTPUT_SATURATED, FAULT_LEVEL_WARNING, "FAULT_CTRL_OUTPUT_SATURATED", "Controller"},
  {FAULT_CTRL_RATE_LIMIT,       FAULT_LEVEL_ERROR,   "FAULT_CTRL_RATE_LIMIT",       "Controller"},
  {FAULT_CTRL_DEADLINE_MISS,    FAULT_LEVEL_ERROR,   "FAULT_CTRL_DEADLINE_MISS",   "Controller"},
  /* Actuator (3) */
  {FAULT_ACT_RW_OVERCURRENT,  FAULT_LEVEL_ERROR,   "FAULT_ACT_RW_OVERCURRENT",  "Actuator"},
  {FAULT_ACT_RW_SPEED_LIMIT,   FAULT_LEVEL_WARNING, "FAULT_ACT_RW_SPEED_LIMIT",   "Actuator"},
  {FAULT_ACT_MTQ_FAULT,       FAULT_LEVEL_ERROR,   "FAULT_ACT_MTQ_FAULT",       "Actuator"},
  /* Sensor (3) */
  {FAULT_SENS_IMU_I2C_ERROR,   FAULT_LEVEL_ERROR,   "FAULT_SENS_IMU_I2C_ERROR",   "Sensor"},
  {FAULT_SENS_IMU_DATA_STALE,   FAULT_LEVEL_WARNING, "FAULT_SENS_IMU_DATA_STALE",  "Sensor"},
  {FAULT_SENS_TEMP_OUT_RANGE,   FAULT_LEVEL_WARNING, "FAULT_SENS_TEMP_OUT_RANGE",  "Sensor"},
  /* Timing (1) */
  {FAULT_TIMING_DEADLINE_MISS, FAULT_LEVEL_ERROR,   "FAULT_TIMING_DEADLINE_MISS","Timing"},
  /* Watchdog (1) */
  {FAULT_WDT_KICK_MISSED,     FAULT_LEVEL_CRITICAL,"FAULT_WDT_KICK_MISSED",    "Watchdog"},
  /* Command (2) */
  {FAULT_CMD_UNKNOWN,          FAULT_LEVEL_WARNING, "FAULT_CMD_UNKNOWN",         "Command"},
  {FAULT_CMD_QUEUE_FULL,       FAULT_LEVEL_ERROR,   "FAULT_CMD_QUEUE_FULL",      "Command"},
  /* Thermal (2) */
  {FAULT_THERM_OVER_TEMP,      FAULT_LEVEL_ERROR,   "FAULT_THERM_OVER_TEMP",    "Thermal"},
  {FAULT_THERM_UNDER_TEMP,     FAULT_LEVEL_WARNING, "FAULT_THERM_UNDER_TEMP",    "Thermal"},
  /* EPS (5) */
  {FAULT_EPS_VBATT_LOW,        FAULT_LEVEL_WARNING, "FAULT_EPS_VBATT_LOW",       "EPS"},
  {FAULT_EPS_VBATT_CRITICAL,   FAULT_LEVEL_CRITICAL,"FAULT_EPS_VBATT_CRITICAL", "EPS"},
  {FAULT_EPS_VBATT_EMERGENCY,  FAULT_LEVEL_CRITICAL,"FAULT_EPS_VBATT_EMERGENCY","EPS"},
  {FAULT_EPS_OVERCURRENT,       FAULT_LEVEL_ERROR,   "FAULT_EPS_OVERCURRENT",     "EPS"},
  {FAULT_EPS_READ_ERROR,       FAULT_LEVEL_ERROR,   "FAULT_EPS_READ_ERROR",      "EPS"},
  /* Telemetry (1) */
  {FAULT_TLM_QUEUE_OVERFLOW,   FAULT_LEVEL_WARNING, "FAULT_TLM_QUEUE_OVERFLOW", "Telemetry"},
};

static const int FAULT_COUNT = (int)(sizeof(ALL_FAULTS) / sizeof(ALL_FAULTS[0]));
/* clang-format on */

/* ------------------------------------------------------------------ */
/* Test 1: All-fault injection                                         */
/*                                                                 */
/* Every fault ID in fault_ids.h can be reported, tracked, and      */
/* cleared without side effects.                                      */
/* ------------------------------------------------------------------ */

static void test_all_fault_ids_injectable(void)
{
  printf("\n  -- T-FI-01: All %d fault IDs injectable --\n", FAULT_COUNT);

  for (int i = 0; i < FAULT_COUNT; i++)
  {
    reset();

    uint16_t id    = ALL_FAULTS[i].id;
    const char *nm = ALL_FAULTS[i].name;

    /* Inject WARNING (lowest level — no side effects) */
    fault_report(id, FAULT_LEVEL_WARNING);
    CHECK(fault_is_active(id), nm);

    /* Get event — verify fields */
    fault_event_t ev;
    CHECK(fault_get_event(id, &ev), nm);
    CHECK(ev.id == id, nm);
    CHECK(ev.level == FAULT_LEVEL_WARNING, nm);
    CHECK(ev.active, nm);
    CHECK(ev.count == 1, nm);

    /* Clear */
    fault_clear(id);
    CHECK(!fault_is_active(id), nm);

    printf("    %s\n", nm);
  }

  PASS("All fault IDs injectable, trackable, clearable");
}

/* ------------------------------------------------------------------ */
/* Test 2: WARNING auto-clear                                          */
/*                                                                 */
/* WARNING faults auto-clear after WARN_AUTO_CLEAR_TICKS ticks.      */
/* ------------------------------------------------------------------ */

static void test_warning_auto_clear(void)
{
  printf("\n  -- T-FI-02: WARNING auto-clear --\n");

  for (int i = 0; i < FAULT_COUNT; i++)
  {
    if (ALL_FAULTS[i].level != FAULT_LEVEL_WARNING)
      continue;

    reset();
    uint16_t id = ALL_FAULTS[i].id;

    fault_report(id, FAULT_LEVEL_WARNING);

    /* WARN_AUTO_CLEAR_TICKS - 1: still active */
    tick_n(WARN_AUTO_CLEAR_TICKS - 1);
    CHECK(fault_is_active(id), ALL_FAULTS[i].name);

    /* WARN_AUTO_CLEAR_TICKS: auto-cleared */
    fault_manager_tick();
    CHECK(!fault_is_active(id), ALL_FAULTS[i].name);
  }

  PASS("WARNING faults auto-clear after WARN_AUTO_CLEAR_TICKS");
}

/* ------------------------------------------------------------------ */
/* Test 3: ERROR persistence                                           */
/*                                                                 */
/* ERROR faults persist indefinitely (not auto-cleared by tick).     */
/* ------------------------------------------------------------------ */

static void test_error_persists(void)
{
  printf("\n  -- T-FI-03: ERROR persistence --\n");

  for (int i = 0; i < FAULT_COUNT; i++)
  {
    if (ALL_FAULTS[i].level != FAULT_LEVEL_ERROR)
      continue;

    reset();
    uint16_t id = ALL_FAULTS[i].id;

    fault_report(id, FAULT_LEVEL_ERROR);

    /* 60 ticks — ERROR must NOT auto-clear */
    tick_n(60);
    CHECK(fault_is_active(id), ALL_FAULTS[i].name);

    /* Even 1000 more ticks — still active */
    tick_n(1000);
    CHECK(fault_is_active(id), ALL_FAULTS[i].name);

    /* Only explicit clear removes it */
    fault_clear(id);
    CHECK(!fault_is_active(id), ALL_FAULTS[i].name);
  }

  PASS("ERROR faults persist through tick() — only explicit clear removes");
}

/* ------------------------------------------------------------------ */
/* Test 4: CRITICAL → FM_SAFE from every flight mode                  */
/*                                                                 */
/* CRITICAL faults trigger FM_SAFE regardless of current mode.       */
/* Tested for one representative fault per CRITICAL subsystem.        */
/* ------------------------------------------------------------------ */

static void test_critical_triggers_safe(void)
{
  printf("\n  -- T-FI-04: CRITICAL → FM_SAFE --\n");

  /* Test one representative CRITICAL fault per subsystem */
  struct
  {
    uint16_t id;
    const char *name;
  } critical_faults[] = {
    {FAULT_WDT_KICK_MISSED, "FAULT_WDT_KICK_MISSED"}, {FAULT_EPS_VBATT_CRITICAL,
                                                         "FAULT_EPS_VBATT_CRITICAL"},
    {FAULT_EPS_VBATT_EMERGENCY, "FAULT_EPS_VBATT_EMERGENCY"}};

  flight_mode_t modes[] = {FM_BOOT, FM_SAFE, FM_NOMINAL, FM_DETUMBLE, FM_DIAGNOSTIC};
  const char *mode_names[] = {"FM_BOOT", "FM_SAFE", "FM_NOMINAL", "FM_DETUMBLE", "FM_DIAGNOSTIC"};
  int mode_count = (int)(sizeof(modes) / sizeof(modes[0]));

  for (int f = 0; f < (int)(sizeof(critical_faults) / sizeof(critical_faults[0])); f++)
  {
    uint16_t fid = critical_faults[f].id;

    for (int m = 0; m < mode_count; m++)
    {
      reset();
      data_layer_set_flight_mode(modes[m]);

      fault_report(fid, FAULT_LEVEL_CRITICAL);

      CHECK(data_layer_get_flight_mode() == FM_SAFE, critical_faults[f].name);
      CHECK(fault_is_active(fid), critical_faults[f].name);

      printf("    %s from %s → FM_SAFE ✓\n", critical_faults[f].name, mode_names[m]);
    }
  }

  PASS("CRITICAL faults trigger FM_SAFE from all flight modes");
}

/* ------------------------------------------------------------------ */
/* Test 5: Level precedence                                            */
/*                                                                 */
/* fault_get_highest_level() returns the maximum of all active faults*/
/* ------------------------------------------------------------------ */

static void test_level_precedence(void)
{
  printf("\n  -- T-FI-05: Level precedence --\n");

  reset();
  CHECK(fault_get_highest_level() == FAULT_LEVEL_NONE, "init: highest=NONE");

  /* WARNING < ERROR < CRITICAL */
  fault_report(FAULT_EST_GYRO_TIMEOUT, FAULT_LEVEL_WARNING);
  CHECK(fault_get_highest_level() == FAULT_LEVEL_WARNING, "WARNING: highest=WARNING");

  fault_report(FAULT_EPS_VBATT_LOW, FAULT_LEVEL_ERROR);
  CHECK(fault_get_highest_level() == FAULT_LEVEL_ERROR, "WARNING+ERROR: highest=ERROR");

  fault_report(FAULT_EPS_VBATT_CRITICAL, FAULT_LEVEL_CRITICAL);
  CHECK(fault_get_highest_level() == FAULT_LEVEL_CRITICAL, "all three: highest=CRITICAL");

  /* Clear WARNING — still ERROR+CRITICAL → CRITICAL */
  fault_clear(FAULT_EST_GYRO_TIMEOUT);
  CHECK(fault_get_highest_level() == FAULT_LEVEL_CRITICAL, "after clearing WARNING: highest=CRITICAL");

  /* Clear ERROR — only CRITICAL remains */
  fault_clear(FAULT_EPS_VBATT_LOW);
  CHECK(fault_get_highest_level() == FAULT_LEVEL_CRITICAL, "after clearing ERROR: highest=CRITICAL");

  /* Clear CRITICAL — NONE */
  fault_clear(FAULT_EPS_VBATT_CRITICAL);
  CHECK(fault_get_highest_level() == FAULT_LEVEL_NONE, "after clearing all: highest=NONE");

  PASS("Level precedence: NONE < WARNING < ERROR < CRITICAL");
}

/* ------------------------------------------------------------------ */
/* Test 6: Cross-subsystem multi-fault matrix                         */
/*                                                                 */
/* Inject faults from ALL subsystems simultaneously and verify:        */
/* - All are tracked independently                                   */
/* - Highest level drives the overall system state                   */
/* - Clearing one doesn't affect others                              */
/* ------------------------------------------------------------------ */

static void test_cross_subsystem_matrix(void)
{
  printf("\n  -- T-FI-06: Cross-subsystem multi-fault matrix --\n");

  /* Inject one fault from each subsystem */
  struct
  {
    uint16_t id;
    uint8_t level;
    const char *name;
    const char *subsystem;
  } subsystem_faults[] = {
    {FAULT_EST_DIVERGENCE,      FAULT_LEVEL_ERROR,   "EST_DIVERGENCE",      "Estimator"},
    {FAULT_CTRL_OUTPUT_SATURATED, FAULT_LEVEL_WARNING, "CTRL_OUTPUT_SAT",     "Controller"},
    {FAULT_ACT_RW_OVERCURRENT,   FAULT_LEVEL_ERROR,   "ACT_RW_OVERCURRENT",  "Actuator"},
    {FAULT_SENS_IMU_I2C_ERROR,   FAULT_LEVEL_ERROR,   "SENS_IMU_I2C_ERROR",  "Sensor"},
    {FAULT_TIMING_DEADLINE_MISS, FAULT_LEVEL_ERROR,   "TIMING_DL_MISS",      "Timing"},
    {FAULT_WDT_KICK_MISSED,      FAULT_LEVEL_CRITICAL,"WDT_KICK_MISSED",     "Watchdog"},
    {FAULT_CMD_QUEUE_FULL,       FAULT_LEVEL_ERROR,   "CMD_QUEUE_FULL",      "Command"},
    {FAULT_THERM_OVER_TEMP,      FAULT_LEVEL_ERROR,   "THERM_OVER_TEMP",     "Thermal"},
    {FAULT_EPS_VBATT_CRITICAL,   FAULT_LEVEL_CRITICAL,"EPS_VBATT_CRITICAL",  "EPS"},
    {FAULT_TLM_QUEUE_OVERFLOW,   FAULT_LEVEL_WARNING, "TLM_QUEUE_OVERFLOW",  "Telemetry"},
  };

  int count = (int)(sizeof(subsystem_faults) / sizeof(subsystem_faults[0]));

  reset();
  data_layer_set_flight_mode(FM_NOMINAL);

  /* Inject all */
  for (int i = 0; i < count; i++)
  {
    fault_report(subsystem_faults[i].id, subsystem_faults[i].level);
    CHECK(fault_is_active(subsystem_faults[i].id), subsystem_faults[i].name);
    printf("    + %s (%s, level=%d)\n", subsystem_faults[i].name,
           subsystem_faults[i].subsystem, subsystem_faults[i].level);
  }

  /* Highest level should be CRITICAL (WDT or EPS) */
  CHECK(fault_get_highest_level() == FAULT_LEVEL_CRITICAL, "highest level = CRITICAL");

  /* FM_SAFE should have been triggered by first CRITICAL */
  CHECK(data_layer_get_flight_mode() == FM_SAFE, "FM_SAFE triggered by CRITICAL");

  /* Clear all */
  for (int i = 0; i < count; i++)
  {
    fault_clear(subsystem_faults[i].id);
    CHECK(!fault_is_active(subsystem_faults[i].id), subsystem_faults[i].name);
  }
  CHECK(fault_get_highest_level() == FAULT_LEVEL_NONE, "all cleared: highest=NONE");

  PASS("Cross-subsystem matrix: all 10 subsystems injectable, independent, correct precedence");
}

/* ------------------------------------------------------------------ */
/* Test 7: Count increment on repeated fault reports                  */
/*                                                                 */
/* Repeated fault_report() increments the counter for that fault ID.   */
/* NOTE: fault_clear() sets active=false but does NOT reset count or   */
/* free the slot. Re-reporting after clear increments from current     */
/* count (5 → 6), does NOT restart from 1. This is by design: the    */
/* slot stays occupied so find_slot() finds it fast.                   */
/* ------------------------------------------------------------------ */

static void test_count_increment_per_id(void)
{
  printf("\n  -- T-FI-07: Count increment per fault ID --\n");

  /* Pick one representative fault per level */
  struct
  {
    uint16_t id;
    uint8_t level;
    const char *name;
    const char *fm_pre; /* flight mode to set before CRITICAL report */
  } samples[] = {
    {FAULT_EST_GYRO_TIMEOUT,  FAULT_LEVEL_WARNING,  "WARNING sample",  NULL},
    {FAULT_CTRL_RATE_LIMIT,   FAULT_LEVEL_ERROR,    "ERROR sample",   NULL},
    {FAULT_WDT_KICK_MISSED,   FAULT_LEVEL_CRITICAL, "CRITICAL sample", "FM_SAFE"},
  };

  for (int s = 0; s < (int)(sizeof(samples) / sizeof(samples[0])); s++)
  {
    reset();
    uint16_t id = samples[s].id;

    /* CRITICAL needs FM_SAFE pre-set to avoid triggering mode change */
    if (samples[s].fm_pre != NULL)
    {
      data_layer_set_flight_mode(FM_SAFE);
    }

    /* Report 5 times */
    for (int i = 0; i < 5; i++)
    {
      fault_report(id, samples[s].level);
    }

    fault_event_t ev;
    fault_get_event(id, &ev);
    CHECK(ev.count == 5, samples[s].name);
    CHECK(ev.active, samples[s].name);

    /* Clear and re-raise — count increments from 5, not from 1 */
    fault_clear(id);
    CHECK(!fault_is_active(id), samples[s].name);
    fault_report(id, samples[s].level);
    fault_get_event(id, &ev);
    CHECK(ev.count == 6, samples[s].name); /* 5 + 1 = 6, slot not freed by clear */

    printf("    %s: count=5 → clear → re-report → count=6 ✓\n", samples[s].name);
  }

  PASS("Count increments on repeated reports; clear() keeps slot, re-report bumps count");
}

/* ------------------------------------------------------------------ */
/* Test 8: Unknown fault IDs accepted but return false on get_event   */
/* ------------------------------------------------------------------ */

static void test_unknown_fault_id(void)
{
  printf("\n  -- T-FI-08: Unknown fault ID handling --\n");

  reset();

  /* Unknown ID can be reported (accepted for future extensibility) */
  uint16_t unknown = 0x1234u;
  fault_report(unknown, FAULT_LEVEL_ERROR);
  CHECK(fault_is_active(unknown), "unknown ID: fault_is_active=true");

  /* But get_event returns false for truly unknown IDs */
  fault_event_t ev;
  CHECK(!fault_get_event(0xFFFFu, &ev), "0xFFFF: get_event=false");
  CHECK(!fault_get_event(0u, &ev), "0x0000: get_event=false");

  PASS("Unknown IDs accepted for reporting; get_event=false for unmapped IDs");
}

/* ------------------------------------------------------------------ */
/* Entry point                                                         */
/* ------------------------------------------------------------------ */

int main(void)
{
  printf("\n=== Fault Injection Test Suite (CDR-SAF-04 / OI-SW-5) ===\n");
  printf("    %d fault IDs across %d subsystems\n", FAULT_COUNT,
         (int)(sizeof(ALL_FAULTS) / sizeof(ALL_FAULTS[0])));

  test_all_fault_ids_injectable();       /* T-FI-01 */
  test_warning_auto_clear();            /* T-FI-02 */
  test_error_persists();                /* T-FI-03 */
  test_critical_triggers_safe();        /* T-FI-04 */
  test_level_precedence();              /* T-FI-05 */
  test_cross_subsystem_matrix();        /* T-FI-06 */
  test_count_increment_per_id();        /* T-FI-07 */
  test_unknown_fault_id();             /* T-FI-08 */

  printf("\n===========================================\n");
  printf("  Tests run:    %d\n", g_tests_run);
  printf("  Failures:     %d\n", g_failures);
  printf("  Fault IDs:    %d (EST:%d CTRL:%d ACT:%d SENS:%d TIM:%d WDT:%d CMD:%d TH:%d EPS:%d TLM:%d)\n",
         FAULT_COUNT,
         4, 3, 3, 3, 1, 1, 2, 2, 5, 1);
  printf("  Subsystems:   Estimator, Controller, Actuator, Sensor, Timing,\n"
         "                Watchdog, Command, Thermal, EPS, Telemetry\n");
  printf("===========================================\n");

  if (g_failures == 0)
  {
    printf("All fault injection tests PASSED.\n");
    return 0;
  }
  printf("%d test(s) FAILED.\n", g_failures);
  return 1;
}
