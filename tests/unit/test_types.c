/**
 * @file test_types.c
 * @brief PR-1 compile-gate: verifies that all Foundation Types headers
 *        compile cleanly and that key constants, enum values, and struct
 *        sizes match the specification.
 *
 * This is intentionally a compile + run test.  It contains no runtime
 * logic beyond verifying invariants that MUST hold for the rest of the
 * firmware to be correct (e.g. log_event_t must be exactly 40 bytes).
 *
 * Any failure prints a descriptive message and returns 1, causing CTest
 * to report the test as failed.
 */

#include "../../include/controller_limits.h"
#include "../../include/eps.h"
#include "../../include/fault_ids.h"
#include "../../include/fault_manager.h"
#include "../../include/flight_mode.h"
#include "../../include/log_event_ids.h"
#include "../../include/logger.h"

#include <stddef.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static int g_failures = 0;

#define CHECK(cond, msg)                                                                           \
  do                                                                                               \
  {                                                                                                \
    if (!(cond))                                                                                   \
    {                                                                                              \
      printf("FAIL: %s\n", (msg));                                                                 \
      g_failures++;                                                                                \
    }                                                                                              \
  } while (0)

/* ------------------------------------------------------------------ */
/* flight_mode.h checks                                                */
/* ------------------------------------------------------------------ */

static void test_flight_mode(void)
{
  /* Enum values per SPEC-2-FMM §3.1 */
  CHECK(FM_BOOT == 0, "FM_BOOT must be 0");
  CHECK(FM_SAFE == 1, "FM_SAFE must be 1");
  CHECK(FM_DETUMBLE == 2, "FM_DETUMBLE must be 2");
  CHECK(FM_NOMINAL == 3, "FM_NOMINAL must be 3");
  CHECK(FM_DIAGNOSTIC == 4, "FM_DIAGNOSTIC must be 4");
  CHECK(FM_PAYLOAD == 5, "FM_PAYLOAD must be 5");
  CHECK(FM_COUNT == 6, "FM_COUNT must be 6");

  CHECK(FMM_OK == 0, "FMM_OK must be 0");
  CHECK(FMM_ERR_INVALID == 1, "FMM_ERR_INVALID must be 1");
  CHECK(FMM_ERR_NOT_ALLOWED == 2, "FMM_ERR_NOT_ALLOWED must be 2");
  CHECK(FMM_ERR_FAULT_BLOCK == 3, "FMM_ERR_FAULT_BLOCK must be 3");

  printf("flight_mode.h: OK\n");
}

/* ------------------------------------------------------------------ */
/* fault_ids.h checks                                                  */
/* ------------------------------------------------------------------ */

static void test_fault_ids(void)
{
  /* Subsystem base addresses must be multiples of 0x100 */
  CHECK((FAULT_SUBSYS_ESTIMATOR & 0xFFu) == 0, "Estimator base low byte must be 0");
  CHECK((FAULT_SUBSYS_CONTROLLER & 0xFFu) == 0, "Controller base low byte must be 0");
  CHECK((FAULT_SUBSYS_EPS & 0xFFu) == 0, "EPS base low byte must be 0");

  /* Spot-check specific IDs */
  CHECK(FAULT_EST_GYRO_TIMEOUT == 0x0101u, "FAULT_EST_GYRO_TIMEOUT must be 0x0101");
  CHECK(FAULT_CTRL_DEADLINE_MISS == 0x0203u, "FAULT_CTRL_DEADLINE_MISS must be 0x0203");
  CHECK(FAULT_EPS_VBATT_CRITICAL == 0x0902u, "FAULT_EPS_VBATT_CRITICAL must be 0x0902");
  CHECK(FAULT_TLM_QUEUE_OVERFLOW == 0x0A01u, "FAULT_TLM_QUEUE_OVERFLOW must be 0x0A01");

  printf("fault_ids.h: OK\n");
}

/* ------------------------------------------------------------------ */
/* fault_manager.h checks                                              */
/* ------------------------------------------------------------------ */

static void test_fault_manager(void)
{
  CHECK(FAULT_LEVEL_NONE == 0, "FAULT_LEVEL_NONE must be 0");
  CHECK(FAULT_LEVEL_WARNING == 1, "FAULT_LEVEL_WARNING must be 1");
  CHECK(FAULT_LEVEL_ERROR == 2, "FAULT_LEVEL_ERROR must be 2");
  CHECK(FAULT_LEVEL_CRITICAL == 3, "FAULT_LEVEL_CRITICAL must be 3");

  /* fault_event_t must have all required fields */
  fault_event_t ev;
  (void)ev.id;
  (void)ev.level;
  (void)ev.timestamp_ms;
  (void)ev.count;
  (void)ev.active;

  printf("fault_manager.h: OK\n");
}

/* ------------------------------------------------------------------ */
/* log_event_ids.h checks                                              */
/* ------------------------------------------------------------------ */

static void test_log_event_ids(void)
{
  /* Class A range: 0x0001–0x00FF */
  CHECK(LOG_EVT_SAFE_ENTRY == 0x0001u, "LOG_EVT_SAFE_ENTRY must be 0x0001");
  CHECK(LOG_EVT_LOG_CORRUPTION == 0x0006u, "LOG_EVT_LOG_CORRUPTION must be 0x0006");

  /* Class B range: 0x0101–0x01FF */
  CHECK(LOG_EVT_MODE_CHANGE == 0x0101u, "LOG_EVT_MODE_CHANGE must be 0x0101");
  CHECK(LOG_EVT_FAULT_ERROR == 0x0104u, "LOG_EVT_FAULT_ERROR must be 0x0104");

  /* Class C range: 0x0201–0x02FF */
  CHECK(LOG_EVT_SYSTEM_BOOT == 0x0201u, "LOG_EVT_SYSTEM_BOOT must be 0x0201");
  CHECK(LOG_EVT_LOG_OVERFLOW == 0x0204u, "LOG_EVT_LOG_OVERFLOW must be 0x0204");

  printf("log_event_ids.h: OK\n");
}

/* ------------------------------------------------------------------ */
/* logger.h checks                                                     */
/* ------------------------------------------------------------------ */

static void test_logger(void)
{
  CHECK(LOG_CLASS_CRITICAL == 0, "LOG_CLASS_CRITICAL must be 0");
  CHECK(LOG_CLASS_OPERATIONAL == 1, "LOG_CLASS_OPERATIONAL must be 1");
  CHECK(LOG_CLASS_INFO == 2, "LOG_CLASS_INFO must be 2");

  /* Core spec invariant: log_event_t MUST be exactly 40 bytes */
  CHECK(sizeof(log_event_t) == 40u, "log_event_t must be exactly 40 bytes");

  /* Field offset checks */
  CHECK(offsetof(log_event_t, timestamp_ms) == 0u, "timestamp_ms must be at offset 0");
  CHECK(offsetof(log_event_t, event_id) == 4u, "event_id must be at offset 4");
  CHECK(offsetof(log_event_t, severity) == 6u, "severity must be at offset 6");
  CHECK(offsetof(log_event_t, subsystem) == 7u, "subsystem must be at offset 7");
  CHECK(offsetof(log_event_t, data) == 8u, "data[] must be at offset 8");

  printf("logger.h: OK\n");
}

/* ------------------------------------------------------------------ */
/* eps.h checks                                                        */
/* ------------------------------------------------------------------ */

static void test_eps(void)
{
  CHECK(ENERGY_NOMINAL == 0, "ENERGY_NOMINAL must be 0");
  CHECK(ENERGY_LOW == 1, "ENERGY_LOW must be 1");
  CHECK(ENERGY_CRITICAL == 2, "ENERGY_CRITICAL must be 2");
  CHECK(ENERGY_EMERGENCY == 3, "ENERGY_EMERGENCY must be 3");

  CHECK(EPS_RAIL_PAYLOAD == 0, "EPS_RAIL_PAYLOAD must be 0");
  CHECK(EPS_RAIL_OBC == 3, "EPS_RAIL_OBC must be 3");
  CHECK(EPS_RAIL_COUNT == 4, "EPS_RAIL_COUNT must be 4");

  /* eps_snapshot_t must have all required fields */
  eps_snapshot_t snap = {0};
  (void)snap.vbatt;
  (void)snap.ibatt;
  (void)snap.temperature;
  (void)snap.state;
  (void)snap.timestamp_ms;
  (void)snap.rail_enabled[0];

  printf("eps.h: OK\n");
}

/* ------------------------------------------------------------------ */
/* controller_limits.h checks                                          */
/* ------------------------------------------------------------------ */

static void test_controller_limits(void)
{
  /* Bounds must be non-negative */
  CHECK(PID_KP_MIN >= 0.0f, "PID_KP_MIN must be >= 0");
  CHECK(PID_KI_MIN >= 0.0f, "PID_KI_MIN must be >= 0");
  CHECK(PID_KD_MIN >= 0.0f, "PID_KD_MIN must be >= 0");

  /* Max values per spec */
  CHECK(PID_KP_MAX == 10.0f, "PID_KP_MAX must be 10.0");
  CHECK(PID_KI_MAX == 5.0f, "PID_KI_MAX must be 5.0");
  CHECK(PID_KD_MAX == 2.0f, "PID_KD_MAX must be 2.0");

  /* Output limits */
  CHECK(U_MAX == 1.0f, "U_MAX must be 1.0");
  CHECK(U_MIN == -1.0f, "U_MIN must be -1.0");

  /* Validation macros */
  CHECK(PID_KP_VALID(5.0f), "5.0 must be valid Kp");
  CHECK(!PID_KP_VALID(11.0f), "11.0 must be invalid Kp");
  CHECK(PID_KI_VALID(0.0f), "0.0 must be valid Ki");
  CHECK(!PID_KI_VALID(-1.0f), "-1.0 must be invalid Ki");

  printf("controller_limits.h: OK\n");
}

/* ------------------------------------------------------------------ */
/* Entry point                                                         */
/* ------------------------------------------------------------------ */

int main(void)
{
  test_flight_mode();
  test_fault_ids();
  test_fault_manager();
  test_log_event_ids();
  test_logger();
  test_eps();
  test_controller_limits();

  if (g_failures == 0)
  {
    printf("All Foundation Types checks passed.\n");
    return 0;
  }
  else
  {
    printf("%d Foundation Types check(s) FAILED.\n", g_failures);
    return 1;
  }
}
