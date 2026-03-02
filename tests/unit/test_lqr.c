/**
 * @file test_lqr.c
 * @brief Unit tests for the LQR attitude controller (PR-13).
 *
 * Tests
 *  T-LQR-01  Gains — default K matrix correct (spot-check diagonal entries)
 *  T-LQR-02  Zero error — zero error in produces zero torque out
 *  T-LQR-03  Sign — positive attitude error produces negative torque (restoring)
 *  T-LQR-04  Axis isolation — error on one axis only affects its torque output
 *  T-LQR-05  Closed-loop stability — 30° initial error settles to <1° in 30 s
 *  T-LQR-06  Disturbance rejection — SS error <1° for constant 1e-4 N·m dist.
 *  T-LQR-07  Custom gains — lqr_set_gains() correctly replaces K
 */

#include "../../include/attitude_dynamics.h"
#include "../../include/lqr.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define DEG2RAD(d) ((d) * (3.14159265358979f / 180.0f))
#define RAD2DEG(r) ((r) * (180.0f / 3.14159265358979f))

#define PASS(msg) printf("[PASS] %s\n", msg)
#define FAIL(msg)                                                                                  \
  do                                                                                               \
  {                                                                                                \
    printf("[FAIL] %s\n", msg);                                                                    \
    return 1;                                                                                      \
  } while (0)

/* ================================================================== */
/* T-LQR-01: Default gain matrix — diagonal entries correct           */
/* ================================================================== */
static int test_default_gains(void)
{
  lqr_t lqr;
  lqr_init(&lqr);

  /* Roll: k_att=1.0, k_rate=0.20 */
  if (fabsf(lqr.K[0][0] - 1.00f) > 1e-6f)
  {
    FAIL("T-LQR-01: K[roll][e_roll] != 1.00");
  }
  if (fabsf(lqr.K[0][3] - 0.20f) > 1e-6f)
  {
    FAIL("T-LQR-01: K[roll][e_droll] != 0.20");
  }

  /* Pitch: identical to roll */
  if (fabsf(lqr.K[1][1] - 1.00f) > 1e-6f)
  {
    FAIL("T-LQR-01: K[pitch][e_pitch] != 1.00");
  }
  if (fabsf(lqr.K[1][4] - 0.20f) > 1e-6f)
  {
    FAIL("T-LQR-01: K[pitch][e_dpitch] != 0.20");
  }

  /* Yaw: k_att=0.50, k_rate=0.10 */
  if (fabsf(lqr.K[2][2] - 0.50f) > 1e-6f)
  {
    FAIL("T-LQR-01: K[yaw][e_yaw] != 0.50");
  }
  if (fabsf(lqr.K[2][5] - 0.10f) > 1e-6f)
  {
    FAIL("T-LQR-01: K[yaw][e_dyaw] != 0.10");
  }

  /* Off-diagonal cross-coupling must be zero */
  if (fabsf(lqr.K[0][1]) > 1e-9f || fabsf(lqr.K[0][2]) > 1e-9f)
  {
    FAIL("T-LQR-01: unexpected cross-coupling in K[roll]");
  }

  PASS("T-LQR-01: default gain matrix correct");
  return 0;
}

/* ================================================================== */
/* T-LQR-02: Zero error → zero torque                                 */
/* ================================================================== */
static int test_zero_error(void)
{
  lqr_t lqr;
  lqr_init(&lqr);

  float att_err[3] = {0.0f, 0.0f, 0.0f};
  float rate_err[3] = {0.0f, 0.0f, 0.0f};
  float torque[3];

  lqr_compute(&lqr, att_err, rate_err, torque);

  for (int i = 0; i < 3; i++)
  {
    if (fabsf(torque[i]) > 1e-9f)
    {
      printf("  torque[%d] = %e (expected 0)\n", i, torque[i]);
      FAIL("T-LQR-02: non-zero torque for zero error");
    }
  }

  PASS("T-LQR-02: zero error produces zero torque");
  return 0;
}

/* ================================================================== */
/* T-LQR-03: Restoring sign — positive roll error → negative torque  */
/* ================================================================== */
static int test_restoring_sign(void)
{
  lqr_t lqr;
  lqr_init(&lqr);

  float att_err[3] = {0.1f, 0.0f, 0.0f}; /* positive roll error */
  float rate_err[3] = {0.0f, 0.0f, 0.0f};
  float torque[3];

  lqr_compute(&lqr, att_err, rate_err, torque);

  if (torque[0] >= 0.0f)
  {
    FAIL("T-LQR-03: positive roll error should produce negative restoring torque");
  }
  if (fabsf(torque[1]) > 1e-9f || fabsf(torque[2]) > 1e-9f)
  {
    FAIL("T-LQR-03: roll error spuriously affects pitch/yaw torque");
  }

  PASS("T-LQR-03: restoring sign correct");
  return 0;
}

/* ================================================================== */
/* T-LQR-04: Axis isolation — error on single axis only              */
/* ================================================================== */
static int test_axis_isolation(void)
{
  lqr_t lqr;
  lqr_init(&lqr);

  /* Yaw only */
  float att_err[3] = {0.0f, 0.0f, 0.2f};
  float rate_err[3] = {0.0f, 0.0f, 0.05f};
  float torque[3];

  lqr_compute(&lqr, att_err, rate_err, torque);

  if (fabsf(torque[0]) > 1e-9f || fabsf(torque[1]) > 1e-9f)
  {
    FAIL("T-LQR-04: yaw error/rate spuriously affects roll/pitch torque");
  }
  if (fabsf(torque[2]) < 1e-6f)
  {
    FAIL("T-LQR-04: yaw torque is zero for non-zero yaw error");
  }

  PASS("T-LQR-04: axis isolation correct");
  return 0;
}

/* ================================================================== */
/* T-LQR-05: Closed-loop stability — 30° → <1° within 30 s           */
/*                                                                    */
/* Simulate closed-loop dynamics per axis:                            */
/*   I * e_ddot = u + disturbance   u = -k_att*e - k_rate*e_dot      */
/* Using RK2 integration at 20 Hz.                                    */
/* ================================================================== */
static int test_closed_loop_stability(void)
{
  lqr_t lqr;
  lqr_init(&lqr);

  /* Simulate roll axis only (I = 0.01 kg·m²) */
  const float I = 0.01f;
  const float dt = 0.05f; /* 20 Hz */
  const int steps = (int)(30.0f / dt);

  float e = DEG2RAD(30.0f); /* 30° initial error */
  float de = 0.0f;          /* zero initial rate */
  float att_err[3];
  float rate_err[3];
  float torque[3];

  for (int i = 0; i < steps; i++)
  {
    att_err[0] = e;
    att_err[1] = 0.0f;
    att_err[2] = 0.0f;
    rate_err[0] = de;
    rate_err[1] = 0.0f;
    rate_err[2] = 0.0f;

    lqr_compute(&lqr, att_err, rate_err, torque);

    /* RK2 integration of e_ddot = torque / I */
    float accel = torque[0] / I;
    float de_mid = de + 0.5f * dt * accel;
    float e_mid = e + 0.5f * dt * de;

    /* recompute torque at midpoint */
    att_err[0] = e_mid;
    rate_err[0] = de_mid;
    lqr_compute(&lqr, att_err, rate_err, torque);

    float accel_mid = torque[0] / I;
    e = e + dt * (de + 0.5f * dt * accel_mid);
    de = de + dt * accel_mid;
  }

  if (fabsf(e) > DEG2RAD(1.0f))
  {
    printf("  final roll error = %.3f° (limit 1°)\n", RAD2DEG(fabsf(e)));
    FAIL("T-LQR-05: 30° error did not settle to <1° within 30 s");
  }

  PASS("T-LQR-05: 30° initial error settles to <1° within 30 s");
  return 0;
}

/* ================================================================== */
/* T-LQR-06: Disturbance rejection — SS error <1° for 1e-4 N·m dist. */
/*                                                                    */
/* Simulate roll axis from zero with constant disturbance 1e-4 N·m.  */
/* With k_att = 1.0:  θ_ss = d / k_att = 1e-4 rad ≈ 0.006°          */
/* ================================================================== */
static int test_disturbance_rejection(void)
{
  lqr_t lqr;
  lqr_init(&lqr);

  const float I = 0.01f;
  const float disturbance = 1e-4f; /* N·m */
  const float dt = 0.05f;
  const int steps = (int)(30.0f / dt); /* run to SS */

  float e = 0.0f;
  float de = 0.0f;
  float att_err[3];
  float rate_err[3];
  float torque[3];

  for (int i = 0; i < steps; i++)
  {
    att_err[0] = e;
    att_err[1] = 0.0f;
    att_err[2] = 0.0f;
    rate_err[0] = de;
    rate_err[1] = 0.0f;
    rate_err[2] = 0.0f;

    lqr_compute(&lqr, att_err, rate_err, torque);

    float accel = (torque[0] + disturbance) / I;
    float de_mid = de + 0.5f * dt * accel;
    float e_mid = e + 0.5f * dt * de;

    att_err[0] = e_mid;
    rate_err[0] = de_mid;
    lqr_compute(&lqr, att_err, rate_err, torque);

    float accel_mid = (torque[0] + disturbance) / I;
    e = e + dt * (de + 0.5f * dt * accel_mid);
    de = de + dt * accel_mid;
  }

  if (fabsf(e) > DEG2RAD(1.0f))
  {
    printf("  SS roll error = %.4f° (limit 1°)\n", RAD2DEG(fabsf(e)));
    FAIL("T-LQR-06: SS error exceeds 1° for 1e-4 N·m disturbance");
  }

  PASS("T-LQR-06: SS error <1° for constant 1e-4 N·m disturbance");
  return 0;
}

/* ================================================================== */
/* T-LQR-07: Custom gains — lqr_set_gains() replaces K correctly     */
/* ================================================================== */
static int test_custom_gains(void)
{
  lqr_t lqr;
  lqr_init(&lqr);

  /* Set a custom scalar gain: all attitude gains = 2.0, rate gains = 0.5 */
  const float K_custom[LQR_M][LQR_N] = {
      {2.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f},
      {0.0f, 2.0f, 0.0f, 0.0f, 0.5f, 0.0f},
      {0.0f, 0.0f, 2.0f, 0.0f, 0.0f, 0.5f},
  };
  lqr_set_gains(&lqr, K_custom);

  float att_err[3] = {1.0f, 0.0f, 0.0f};
  float rate_err[3] = {0.0f, 0.0f, 0.0f};
  float torque[3];

  lqr_compute(&lqr, att_err, rate_err, torque);

  /* Expected: torque[0] = -(2.0*1.0 + 0.5*0.0) = -2.0 */
  if (fabsf(torque[0] - (-2.0f)) > 1e-6f)
  {
    printf("  torque[0] = %f (expected -2.0)\n", torque[0]);
    FAIL("T-LQR-07: custom gain not applied correctly");
  }

  PASS("T-LQR-07: lqr_set_gains() replaces K correctly");
  return 0;
}

/* ================================================================== */
int main(void)
{
  int result = 0;
  result |= test_default_gains();
  result |= test_zero_error();
  result |= test_restoring_sign();
  result |= test_axis_isolation();
  result |= test_closed_loop_stability();
  result |= test_disturbance_rejection();
  result |= test_custom_gains();

  if (result == 0)
  {
    printf("All LQR tests passed.\n");
  }
  return result;
}
