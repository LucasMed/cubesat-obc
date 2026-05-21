/* test_fault_safe.c — Integration test: Fault Manager → FM_SAFE pipeline (PR-25)
 *
 * T-FMS-01a: CRITICAL fault injected via fault_report() immediately forces
 *            data_layer flight-mode to FM_SAFE (via fmm_force_safe).
 * T-FMS-01b: ERROR fault does NOT force FM_SAFE.
 * T-FMS-01c: WARNING fault does NOT force FM_SAFE.
 * T-FMS-01d: FM_SAFE is sticky — clearing the fault does not auto-restore
 *            the flight mode; that requires an explicit fmm_request_transition.
 *
 * Compiled with real fault_manager.c + flight_mode_manager.c + data_layer.c
 * + system_state.c.  No FreeRTOS task calls; the host header macros in
 * include/host/ satisfy all preprocessor dependencies.
 *
 * Spec ref: PHASE6_PLAN.md PR-25, SYS-REQ-4
 */

#include "data_layer.h"
#include "fault_ids.h"
#include "fault_manager.h"
#include "flight_mode.h"

#include <stdbool.h>
#include <stdio.h>

/* ---- Test helpers ------------------------------------------------------- */
static int g_failures = 0;

#define CHECK(cond, msg)                                                                           \
  do                                                                                               \
  {                                                                                                \
    if (!(cond))                                                                                   \
    {                                                                                              \
      printf("  FAIL [%s:%d] %s\n", __func__, __LINE__, (msg));                                    \
      g_failures++;                                                                                \
    }                                                                                              \
  } while (0)

/* ---- Logger stub (flight_mode_manager.c calls log_event) --------------- */
void log_event(uint16_t event_id, uint8_t log_class, const void *data, uint8_t data_len)
{
  (void)event_id;
  (void)log_class;
  (void)data;
  (void)data_len;
}

static void reset_all(void)
{
  data_layer_init();
  flight_mode_manager_init(); /* sets FM_BOOT in data_layer */
  fault_manager_init();
}

/* ========================================================================
 * T-FMS-01a  CRITICAL fault → FM_SAFE immediate (fmm_force_safe side-effect)
 * ======================================================================== */
static void test_critical_forces_safe(void)
{
  reset_all();
  CHECK(data_layer_get_flight_mode() == FM_BOOT, "pre: mode must be FM_BOOT");

  fault_report(FAULT_EST_GYRO_TIMEOUT, FAULT_LEVEL_CRITICAL);

  CHECK(data_layer_get_flight_mode() == FM_SAFE, "T-FMS-01a: CRITICAL must force FM_SAFE");
  CHECK(fault_is_active(FAULT_EST_GYRO_TIMEOUT), "T-FMS-01a: fault must remain active");
  CHECK(fault_get_highest_level() == FAULT_LEVEL_CRITICAL,
        "T-FMS-01a: highest level must be CRITICAL");

  printf("  PASS T-FMS-01a CRITICAL fault forces FM_SAFE immediately\n");
}

/* ========================================================================
 * T-FMS-01b  ERROR fault does NOT force FM_SAFE
 * ======================================================================== */
static void test_error_no_safe(void)
{
  reset_all();
  fault_report(FAULT_CTRL_DEADLINE_MISS, FAULT_LEVEL_ERROR);

  CHECK(data_layer_get_flight_mode() != FM_SAFE, "T-FMS-01b: ERROR fault must not force FM_SAFE");
  CHECK(fault_is_active(FAULT_CTRL_DEADLINE_MISS), "T-FMS-01b: error fault must be active");
  CHECK(fault_get_highest_level() == FAULT_LEVEL_ERROR, "T-FMS-01b: highest level must be ERROR");

  printf("  PASS T-FMS-01b ERROR fault does not force FM_SAFE\n");
}

/* ========================================================================
 * T-FMS-01c  WARNING fault does NOT force FM_SAFE
 * ======================================================================== */
static void test_warning_no_safe(void)
{
  reset_all();
  fault_report(FAULT_SENS_IMU_DATA_STALE, FAULT_LEVEL_WARNING);

  CHECK(data_layer_get_flight_mode() != FM_SAFE, "T-FMS-01c: WARNING fault must not force FM_SAFE");
  CHECK(fault_is_active(FAULT_SENS_IMU_DATA_STALE), "T-FMS-01c: warning fault must be active");

  printf("  PASS T-FMS-01c WARNING fault does not force FM_SAFE\n");
}

/* ========================================================================
 * T-FMS-01d  FM_SAFE is sticky — clearing the fault preserves FM_SAFE
 *
 * The flight-mode manager does not auto-restore from FM_SAFE when faults
 * are cleared.  An explicit fmm_request_transition() from ground control
 * is required.
 * ======================================================================== */
static void test_safe_sticky_after_clear(void)
{
  reset_all();
  fault_report(FAULT_ACT_RW_OVERCURRENT, FAULT_LEVEL_CRITICAL);
  CHECK(data_layer_get_flight_mode() == FM_SAFE, "T-FMS-01d pre: mode must be FM_SAFE");

  fault_clear(FAULT_ACT_RW_OVERCURRENT);
  CHECK(!fault_is_active(FAULT_ACT_RW_OVERCURRENT), "T-FMS-01d: cleared fault must be inactive");

  /* Mode must remain FM_SAFE even though no active critical fault exists */
  CHECK(data_layer_get_flight_mode() == FM_SAFE,
        "T-FMS-01d: FM_SAFE must be sticky after fault clear");

  printf("  PASS T-FMS-01d FM_SAFE sticky after fault_clear()\n");
}

/* ========================================================================
 * main
 * ======================================================================== */
int main(void)
{
  printf("=== Fault → FM_SAFE integration tests (PR-25, T-FMS-01) ===\n");

  test_critical_forces_safe();
  test_error_no_safe();
  test_warning_no_safe();
  test_safe_sticky_after_clear();

  if (g_failures == 0)
  {
    printf("ALL T-FMS-01 TESTS PASSED\n");
    return 0;
  }

  printf("%d TEST(S) FAILED\n", g_failures);
  return 1;
}
