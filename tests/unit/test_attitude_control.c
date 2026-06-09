/* test_attitude_control.c — Unit tests for attitude_control.c (PID dispatch)
 *
 * Tests the real attitude_ctrl_init() and attitude_ctrl_update() functions.
 * These functions are NOT called by test_attitude_control_task.c because that
 * file replaces them with strong-symbol stubs. This test compiles the real
 * source directly.
 *
 * T-ACTL-01  attitude_ctrl_init zeroes all PID controllers
 * T-ACTL-02  attitude_ctrl_update computes non-zero torque for non-zero error
 * T-ACTL-03  attitude_ctrl_update zero torque when target == current
 * T-ACTL-04  attitude_ctrl_update decoupled response (roll error → roll only)
 */

#include "attitude_control.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Test helpers                                                        */
/* ------------------------------------------------------------------ */

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

#define FPEQ(a, b) (fabsf((a) - (b)) < 1e-5f)

/* ========================================================================
 * T-ACTL-01  attitude_ctrl_init must zero all PID state
 * ======================================================================== */

static void test_init_zeroes_state(void)
{
  int failures_before = g_failures;

  attitude_ctrl_t ac;
  /* Fill with known garbage */
  memset(&ac, 0xFF, sizeof(ac));
  attitude_ctrl_init(&ac);

  /* After init, PID outputs for zero error should be zero */
  float target[3] = {0.0f, 0.0f, 0.0f};
  float current[3] = {0.0f, 0.0f, 0.0f};
  float rates[3] = {0.0f, 0.0f, 0.0f};
  float outputs[3] = {999.0f, 999.0f, 999.0f};
  attitude_ctrl_update(&ac, target, current, rates, outputs, 0.01f);

  CHECK(FPEQ(outputs[0], 0.0f), "roll output must be zero for zero error");
  CHECK(FPEQ(outputs[1], 0.0f), "pitch output must be zero for zero error");
  CHECK(FPEQ(outputs[2], 0.0f), "yaw output must be zero for zero error");

  printf("[T-ACTL-01] test_init_zeroes_state: %s\n",
         g_failures == failures_before ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-ACTL-02  attitude_ctrl_update must produce non-zero torque for error
 * ======================================================================== */

static void test_update_computes_nonzero(void)
{
  int failures_before = g_failures;

  attitude_ctrl_t ac;
  attitude_ctrl_init(&ac);

  /* 10-degree error on roll axis */
  float target[3] = {0.0f, 0.0f, 0.0f};
  float current[3] = {0.175f, 0.0f, 0.0f};  // ~10° in radians
  float rates[3] = {0.0f, 0.0f, 0.0f};
  float outputs[3] = {0.0f, 0.0f, 0.0f};
  attitude_ctrl_update(&ac, target, current, rates, outputs, 0.01f);

  CHECK(!FPEQ(outputs[0], 0.0f), "roll output must be non-zero for roll error");
  CHECK(FPEQ(outputs[1], 0.0f), "pitch output must be zero (no pitch error)");
  CHECK(FPEQ(outputs[2], 0.0f), "yaw output must be zero (no yaw error)");

  printf("[T-ACTL-02] test_update_computes_nonzero: %s\n",
         g_failures == failures_before ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-ACTL-03  attitude_ctrl_update: zero torque when target == current
 * ======================================================================== */

static void test_update_zero_when_aligned(void)
{
  int failures_before = g_failures;

  attitude_ctrl_t ac;
  attitude_ctrl_init(&ac);

  float target[3] = {0.5f, -0.3f, 1.2f};
  float current[3] = {0.5f, -0.3f, 1.2f};  // same as target
  float rates[3] = {0.0f, 0.0f, 0.0f};
  float outputs[3] = {999.0f, 999.0f, 999.0f};
  attitude_ctrl_update(&ac, target, current, rates, outputs, 0.01f);

  CHECK(FPEQ(outputs[0], 0.0f), "roll output must be zero when aligned");
  CHECK(FPEQ(outputs[1], 0.0f), "pitch output must be zero when aligned");
  CHECK(FPEQ(outputs[2], 0.0f), "yaw output must be zero when aligned");

  printf("[T-ACTL-03] test_update_zero_when_aligned: %s\n",
         g_failures == failures_before ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-ACTL-04  attitude_ctrl_update: decoupled axes
 * ======================================================================== */

static void test_update_decoupled_axes(void)
{
  int failures_before = g_failures;

  attitude_ctrl_t ac;
  attitude_ctrl_init(&ac);

  /* Error on yaw only — roll/pitch should be zero */
  float target[3] = {0.0f, 0.0f, 0.0f};
  float current[3] = {0.0f, 0.0f, 0.3f};  // yaw error only
  float rates[3] = {0.0f, 0.0f, 0.0f};
  float outputs[3] = {0.0f, 0.0f, 0.0f};
  attitude_ctrl_update(&ac, target, current, rates, outputs, 0.01f);

  CHECK(FPEQ(outputs[0], 0.0f), "roll must be zero (no roll error)");
  CHECK(FPEQ(outputs[1], 0.0f), "pitch must be zero (no pitch error)");
  CHECK(!FPEQ(outputs[2], 0.0f), "yaw must be non-zero (yaw error present)");

  printf("[T-ACTL-04] test_update_decoupled_axes: %s\n",
         g_failures == failures_before ? "PASS" : "FAIL");
}

/* ======================================================================== */
int main(void)
{
  printf("=== Attitude Control Unit Tests ===\n");
  test_init_zeroes_state();
  test_update_computes_nonzero();
  test_update_zero_when_aligned();
  test_update_decoupled_axes();
  printf("==================================\n");
  if (g_failures == 0)
  {
    printf("All tests passed!\n");
    return 0;
  }
  printf("%d test(s) FAILED.\n", g_failures);
  return 1;
}
