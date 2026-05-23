/**
 * @file test_eps_monitor.c
 * @brief PR-5 gate: EPS Monitor correctness checks.
 *
 * Thresholds calibrated for a 1S LiPo battery (3.0-4.2 V):
 *   NOMINAL   : V_batt >= 3.6 V
 *   LOW       : 3.3 V <= V_batt < 3.6 V
 *   CRITICAL  : 3.0 V <= V_batt < 3.3 V
 *   EMERGENCY : V_batt < 3.0 V
 *
 * Tests:
 *   1.  eps_snapshot_get() returns -1 before eps_monitor_init()
 *   2.  eps_monitor_init() returns 0; default voltage (3.8V) → NOMINAL
 *   3.  eps_monitor_tick() with 3.8 V → ENERGY_NOMINAL, no VBATT fault
 *   4.  Voltage drop to 3.45 V → ENERGY_LOW, FAULT_EPS_VBATT_LOW WARNING
 *   5.  Voltage drop to 3.15 V → ENERGY_CRITICAL, FAULT_EPS_VBATT_CRITICAL CRITICAL
 *   6.  Voltage drop to 2.8 V → ENERGY_EMERGENCY, FAULT_EPS_VBATT_EMERGENCY CRITICAL
 *   7.  Hysteresis: LOW state, inject 3.65 V → stays ENERGY_LOW
 *   8.  Hysteresis: LOW state, inject 3.75 V → recovers to ENERGY_NOMINAL
 *   9.  HAL read failure → FAULT_EPS_READ_ERROR active, state unchanged
 *   10. eps_set_power() enables / disables a rail; reflected in snapshot
 *   11. OBC rail cannot be disabled via eps_set_power()
 *   12. eps_get_energy_state() always matches snapshot.state
 */

#include "../../include/data_layer.h"
#include "../../include/eps.h"
#include "../../include/fault_ids.h"
#include "../../include/fault_manager.h"
#include "../../include/flight_mode.h"

#include <stdbool.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* Mock stub for log_event                                             */
/* ------------------------------------------------------------------ */
/* flight_mode_manager.c calls log_event() after every transition.
 * Since the deploy-automation FMM change added this call, provide a
 * no-op stub for test targets that link flight_mode_manager.c. */

#include "logger.h"

void log_event(uint16_t event_id, log_class_t log_class, const void *data, uint8_t data_len)
{
  (void)event_id;
  (void)log_class;
  (void)data;
  (void)data_len;
}

/* ------------------------------------------------------------------ */
/* HAL stub — strong symbol overrides weak default in eps_monitor.c   */
/* ------------------------------------------------------------------ */

static float s_vbatt = 3.8f;
static float s_ibatt = 0.5f;
static float s_temp = 25.0f;
static bool s_read_ok = true;

bool eps_hal_read(float *vbatt, float *ibatt, float *temp)
{
  if (vbatt)
    *vbatt = s_vbatt;
  if (ibatt)
    *ibatt = s_ibatt;
  if (temp)
    *temp = s_temp;
  return s_read_ok;
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

/** Full subsystem reset: DLA → FMM → Fault Manager → EPS Monitor. */
static void reset_all(void)
{
  s_vbatt = 3.8f;
  s_ibatt = 0.5f;
  s_temp = 25.0f;
  s_read_ok = true;

  data_layer_init();
  flight_mode_manager_init();
  fault_manager_init();
  eps_monitor_init();
}

/* ------------------------------------------------------------------ */
/* Test 1: snapshot unavailable before init                            */
/* ------------------------------------------------------------------ */

static void test_snapshot_before_init(void)
{
  /* NOTE: intentionally called BEFORE any reset_all() / eps_monitor_init().
   * g_initialised is false at program start (zero-initialised static). */
  eps_snapshot_t snap = {0};
  CHECK(eps_snapshot_get(&snap) == -1, "snapshot_get must return -1 before init");
  printf("test_snapshot_before_init: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 2: init returns 0 and sets NOMINAL on default voltage          */
/* ------------------------------------------------------------------ */

static void test_init_ok(void)
{
  reset_all(); /* s_vbatt = 3.8 V → NOMINAL (≥ 3.6 V) */
  eps_snapshot_t snap = {0};
  CHECK(eps_snapshot_get(&snap) == 0, "snapshot_get must succeed after init");
  CHECK(snap.state == ENERGY_NOMINAL, "3.8 V must classify as ENERGY_NOMINAL");
  CHECK(snap.rail_enabled[EPS_RAIL_OBC] == true, "OBC rail must be enabled after init");
  printf("test_init_ok: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 3: tick at nominal voltage — no VBATT fault                   */
/* ------------------------------------------------------------------ */

static void test_nominal_voltage(void)
{
  reset_all(); /* NOMINAL */
  s_vbatt = 3.8f;
  eps_monitor_tick();

  eps_snapshot_t snap = {0};
  eps_snapshot_get(&snap);
  CHECK(snap.state == ENERGY_NOMINAL, "3.8 V must remain ENERGY_NOMINAL after tick");

  /* Neither VBATT fault should be active */
  CHECK(!fault_is_active(FAULT_EPS_VBATT_LOW), "VBATT_LOW must not be active at nominal voltage");
  CHECK(!fault_is_active(FAULT_EPS_VBATT_CRITICAL),
        "VBATT_CRITICAL must not be active at nominal voltage");
  CHECK(!fault_is_active(FAULT_EPS_VBATT_EMERGENCY),
        "VBATT_EMERGENCY must not be active at nominal voltage");
  printf("test_nominal_voltage: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 4: voltage drop to LOW range                                   */
/* ------------------------------------------------------------------ */

static void test_low_voltage(void)
{
  reset_all(); /* NOMINAL at 3.8 V */
  s_vbatt = 3.45f;
  eps_monitor_tick();

  eps_snapshot_t snap = {0};
  eps_snapshot_get(&snap);
  CHECK(snap.state == ENERGY_LOW, "3.45 V must transition to ENERGY_LOW");
  CHECK(snap.vbatt == 3.45f, "snapshot vbatt must reflect injected value");

  CHECK(fault_is_active(FAULT_EPS_VBATT_LOW), "FAULT_EPS_VBATT_LOW must be active");
  fault_event_t ev = {0};
  CHECK(fault_get_event(FAULT_EPS_VBATT_LOW, &ev), "must retrieve VBATT_LOW event");
  CHECK(ev.level == FAULT_LEVEL_WARNING, "ENERGY_LOW must report WARNING severity");
  CHECK(!fault_is_active(FAULT_EPS_VBATT_CRITICAL),
        "VBATT_CRITICAL must not be active in LOW state");

  /* DLA must also be updated */
  CHECK(data_layer_get_energy_state() == ENERGY_LOW, "DLA energy state must be LOW");
  printf("test_low_voltage: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 5: voltage drop to CRITICAL range                              */
/* ------------------------------------------------------------------ */

static void test_critical_voltage(void)
{
  reset_all();
  s_vbatt = 3.15f;
  eps_monitor_tick();

  eps_snapshot_t snap = {0};
  eps_snapshot_get(&snap);
  CHECK(snap.state == ENERGY_CRITICAL, "3.15 V must transition to ENERGY_CRITICAL");

  CHECK(fault_is_active(FAULT_EPS_VBATT_CRITICAL), "FAULT_EPS_VBATT_CRITICAL must be active");
  fault_event_t ev = {0};
  CHECK(fault_get_event(FAULT_EPS_VBATT_CRITICAL, &ev), "must retrieve VBATT_CRITICAL event");
  CHECK(ev.level == FAULT_LEVEL_CRITICAL,
        "ENERGY_CRITICAL must report CRITICAL severity (single FDIR path)");
  CHECK(!fault_is_active(FAULT_EPS_VBATT_LOW), "VBATT_LOW must be cleared when entering CRITICAL");

  /* FMM must be FM_SAFE via fault_manager → fmm_force_safe() (canonical path) */
  dl_snapshot_t dl = {0};
  data_layer_read(&dl);
  CHECK(dl.mode == FM_SAFE, "FMM must be FM_SAFE after ENERGY_CRITICAL");
  printf("test_critical_voltage: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 6: voltage drop to EMERGENCY range                             */
/* ------------------------------------------------------------------ */

static void test_emergency_voltage(void)
{
  reset_all();
  s_vbatt = 2.8f;
  eps_monitor_tick();

  eps_snapshot_t snap = {0};
  eps_snapshot_get(&snap);
  CHECK(snap.state == ENERGY_EMERGENCY, "2.8 V must transition to ENERGY_EMERGENCY");

  fault_event_t ev = {0};
  CHECK(fault_is_active(FAULT_EPS_VBATT_EMERGENCY),
        "FAULT_EPS_VBATT_EMERGENCY must be active in EMERGENCY state");
  CHECK(fault_get_event(FAULT_EPS_VBATT_EMERGENCY, &ev), "must retrieve VBATT_EMERGENCY event");
  CHECK(ev.level == FAULT_LEVEL_CRITICAL, "ENERGY_EMERGENCY must report CRITICAL severity");
  CHECK(!fault_is_active(FAULT_EPS_VBATT_CRITICAL),
        "VBATT_CRITICAL must NOT be active in EMERGENCY state (separate fault ID)");
  CHECK(!fault_is_active(FAULT_EPS_VBATT_LOW), "VBATT_LOW must be cleared when entering EMERGENCY");

  /* fault_manager calls fmm_force_safe() for CRITICAL-level faults */
  dl_snapshot_t dl = {0};
  data_layer_read(&dl);
  CHECK(dl.mode == FM_SAFE, "FMM must be FM_SAFE after ENERGY_EMERGENCY");
  printf("test_emergency_voltage: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 7: hysteresis — voltage slightly above LOW threshold           */
/* ------------------------------------------------------------------ */

static void test_hysteresis_no_recovery(void)
{
  /* Drive to LOW state */
  reset_all();
  s_vbatt = 3.45f;
  eps_monitor_tick(); /* → LOW */

  /* 3.65 V is above the 3.6 V base threshold but below 3.7 V hysteresis */
  s_vbatt = 3.65f;
  eps_monitor_tick();

  eps_snapshot_t snap = {0};
  eps_snapshot_get(&snap);
  CHECK(snap.state == ENERGY_LOW, "3.65 V from LOW must NOT recover to NOMINAL (hysteresis)");
  printf("test_hysteresis_no_recovery: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 8: hysteresis — voltage clears the hysteresis band            */
/* ------------------------------------------------------------------ */

static void test_hysteresis_recovery(void)
{
  /* Drive to LOW state */
  reset_all();
  s_vbatt = 3.45f;
  eps_monitor_tick(); /* → LOW */

  /* 3.75 V exceeds 3.6 V + 0.1 V hysteresis → should recover */
  s_vbatt = 3.75f;
  eps_monitor_tick();

  eps_snapshot_t snap = {0};
  eps_snapshot_get(&snap);
  CHECK(snap.state == ENERGY_NOMINAL, "3.75 V from LOW must recover to ENERGY_NOMINAL");
  CHECK(!fault_is_active(FAULT_EPS_VBATT_LOW), "VBATT_LOW must be cleared on recovery");
  printf("test_hysteresis_recovery: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 9: HAL read failure                                            */
/* ------------------------------------------------------------------ */

static void test_read_error(void)
{
  reset_all();     /* NOMINAL */
  s_vbatt = 2.8f; /* would be EMERGENCY, but read fails */
  s_read_ok = false;
  eps_monitor_tick();

  eps_snapshot_t snap = {0};
  eps_snapshot_get(&snap);
  /* State must be unchanged (still NOMINAL from init) */
  CHECK(snap.state == ENERGY_NOMINAL, "State must not change when HAL read fails");
  CHECK(fault_is_active(FAULT_EPS_READ_ERROR), "FAULT_EPS_READ_ERROR must be active");
  printf("test_read_error: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 10: rail control                                               */
/* ------------------------------------------------------------------ */

static void test_rail_control(void)
{
  reset_all();
  /* Disable payload rail */
  eps_set_power(EPS_RAIL_PAYLOAD, false);

  eps_snapshot_t snap = {0};
  eps_snapshot_get(&snap);
  CHECK(!snap.rail_enabled[EPS_RAIL_PAYLOAD], "PAYLOAD rail must be disabled after eps_set_power");
  CHECK(snap.rail_enabled[EPS_RAIL_COMMS], "COMMS rail must remain enabled");

  /* Re-enable */
  eps_set_power(EPS_RAIL_PAYLOAD, true);
  eps_snapshot_get(&snap);
  CHECK(snap.rail_enabled[EPS_RAIL_PAYLOAD], "PAYLOAD rail must re-enable");
  printf("test_rail_control: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 11: OBC rail is protected                                      */
/* ------------------------------------------------------------------ */

static void test_obc_protected(void)
{
  reset_all();
  eps_set_power(EPS_RAIL_OBC, false); /* must be silently ignored */

  eps_snapshot_t snap = {0};
  eps_snapshot_get(&snap);
  CHECK(snap.rail_enabled[EPS_RAIL_OBC], "OBC rail must remain enabled regardless of set_power");
  printf("test_obc_protected: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 12: eps_get_energy_state() matches snapshot                    */
/* ------------------------------------------------------------------ */

static void test_energy_accessor(void)
{
  reset_all();
  s_vbatt = 3.15f;
  eps_monitor_tick(); /* → CRITICAL */

  eps_snapshot_t snap = {0};
  eps_snapshot_get(&snap);
  CHECK(eps_get_energy_state() == snap.state, "eps_get_energy_state() must equal snapshot.state");
  CHECK(eps_get_energy_state() == ENERGY_CRITICAL,
        "eps_get_energy_state() must return ENERGY_CRITICAL after 3.15 V tick");
  printf("test_energy_accessor: OK\n");
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(void)
{
  /* Test 1 MUST run first — checks uninitialized state */
  test_snapshot_before_init();

  /* All remaining tests use reset_all() for a clean slate */
  test_init_ok();
  test_nominal_voltage();
  test_low_voltage();
  test_critical_voltage();
  test_emergency_voltage();
  test_hysteresis_no_recovery();
  test_hysteresis_recovery();
  test_read_error();
  test_rail_control();
  test_obc_protected();
  test_energy_accessor();

  if (g_failures == 0)
  {
    printf("All EPS Monitor checks PASSED.\n");
    return 0;
  }
  printf("%d EPS Monitor check(s) FAILED.\n", g_failures);
  return 1;
}
