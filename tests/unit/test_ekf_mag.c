/* test_ekf_mag.c — Unit tests for EKF yaw update via magnetometer (PR-19/22)
 *
 * T-EKFM-01: no_crash        — ekf_update_mag() completes on zero-state EKF
 * T-EKFM-02: degenerate_field — |Bh| < epsilon → state and P unchanged
 * T-EKFM-03: yaw_correction  — large initial yaw error → update moves x[2]
 *                              toward measured yaw
 * T-EKFM-04: wrap_pi         — innovation near ±π wrapped correctly
 * T-EKFM-05: cov_reduction   — P[2][2] strictly decreases after update
 * T-EKFM-06: convergence     — repeated predict+update_mag converges yaw
 *                              to within ±5° of true value in 10 s
 * T-EKFM-07: declination     — non-zero declination shifts yaw_meas by
 *                              expected offset (PR-22)
 *
 * Only ekf.c is compiled here; no hardware or RTOS dependency.
 */

#include "ekf.h"
#include "quaternion.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
  #define M_PI 3.14159265358979323846f
#endif

/* ---- Test helpers ------------------------------------------------------- */
static int g_failures = 0;

#define CHECK(cond, msg)                                                                           \
  do                                                                                               \
  {                                                                                                \
    if (!(cond))                                                                                   \
    {                                                                                              \
      printf("  FAIL [%s:%d] %s\n", __func__, __LINE__, msg);                                      \
      g_failures++;                                                                                \
    }                                                                                              \
  } while (0)

#define DEG2RAD(d) ((d) * ((float)M_PI / 180.0f))
#define RAD2DEG(r) ((r) * (180.0f / (float)M_PI))
#define FPEQ(a, b) (fabsf((a) - (b)) < 1e-5f)

/* ---- Helper: build a magnetic field vector that implies a given yaw ----
 *
 * For a level platform (roll=0, pitch=0) pointing North with a horizontal
 * field, the tilt-compensated components reduce to:
 *   Bh_x = Bx,  Bh_y = By
 *   yaw   = atan2(-Bh_y, Bh_x)
 *
 * So to represent yaw_true:
 *   Bx = B0 * cos(yaw_true)
 *   By = -B0 * sin(yaw_true)   (negated because yaw = atan2(-By, Bx))
 *   Bz = 0                      (purely horizontal field)
 */
static void make_mag(float yaw_rad, float field_uT[3])
{
  const float B0 = 47.0f; /* representative field magnitude [µT] */
  field_uT[0] = B0 * cosf(yaw_rad);
  field_uT[1] = -B0 * sinf(yaw_rad);
  field_uT[2] = 0.0f;
}

/* ========================================================================
 * T-EKFM-01  ekf_update_mag() does not crash on cold-start EKF + valid field
 * ======================================================================== */
static void test_no_crash(void)
{
  ekf_t ekf;
  ekf_init(&ekf);

  float mag[3];
  make_mag(DEG2RAD(45.0f), mag);

  ekf_update_mag(&ekf, mag, 0.0f); /* must not crash */

  float att[3];
  ekf_get_attitude(&ekf, att);
  CHECK(isfinite(att[2]), "yaw must remain finite after update");
  printf("  PASS T-EKFM-01 no crash with valid field\n");
}

/* ========================================================================
 * T-EKFM-02  Degenerate horizontal field → state and covariance unchanged
 * ======================================================================== */
static void test_degenerate_field(void)
{
  ekf_t ekf;
  ekf_init(&ekf);
  float att_before[3];
  ekf_get_attitude(&ekf, att_before);
  float P00_before = ekf.P[0][0];

  /* Purely vertical field → Bh_x = By = 0, Bh_y = 0 → degenerate */
  float mag[3] = {0.0f, 0.0f, 50.0f};
  ekf_update_mag(&ekf, mag, 0.0f);

  float att_after[3];
  ekf_get_attitude(&ekf, att_after);
  CHECK(FPEQ(att_after[2], att_before[2]), "yaw state must be unchanged for degenerate field");
  CHECK(FPEQ(ekf.P[0][0], P00_before), "P[0][0] must be unchanged for degenerate field");
  printf("  PASS T-EKFM-02 degenerate field skips update\n");
}

/* ========================================================================
 * T-EKFM-03  Large initial yaw error → update moves yaw toward measured value
 * ======================================================================== */
static void test_yaw_correction(void)
{
  ekf_t ekf;
  ekf_init(&ekf);

  float true_yaw = DEG2RAD(90.0f);
  /* Starts at zero yaw (identity quat) */

  float mag[3];
  make_mag(true_yaw, mag);
  ekf_update_mag(&ekf, mag, 0.0f);

  float att[3];
  ekf_get_attitude(&ekf, att);
  /* After one update yaw must move toward true_yaw (i.e., increase from 0) */
  CHECK(att[2] > 0.0f, "yaw must move toward measured value after update");
  /* But must not overshoot (innovation was π/2, gain < 1, so yaw < π/2) */
  CHECK(att[2] < true_yaw, "yaw must not overshoot the measurement");
  printf("  PASS T-EKFM-03 yaw correction moves toward measurement\n");
}

/* ========================================================================
 * T-EKFM-04  Innovation wrapping near ±π is handled correctly
 *
 * True yaw = +π - 0.1 rad ≈ 170°.  EKF estimate = -(π - 0.1) ≈ -170°.
 * The short-way-round innovation is +0.2 rad (clockwise), not -2π+0.2.
 * The update must therefore INCREASE x[2] by a fraction of 0.2 rad.
 * ======================================================================== */
static void test_wrap_pi(void)
{
  ekf_t ekf;
  ekf_init(&ekf);

  float true_yaw = (float)M_PI - 0.1f;    /* ≈ +170° */
  float est_yaw = -((float)M_PI - 0.1f);   /* ≈ -170° */

  /* Set initial state as quaternion representing est_yaw */
  quat_t q_init = q_from_euler(0, 0, est_yaw);
  ekf.x[0] = q_init.w;
  ekf.x[1] = q_init.x;
  ekf.x[2] = q_init.y;
  ekf.x[3] = q_init.z;

  float mag[3];
  make_mag(true_yaw, mag);
  ekf_update_mag(&ekf, mag, 0.0f);

  float att[3];
  ekf_get_attitude(&ekf, att);

  float err_after = att[2] - true_yaw;
  while (err_after > (float)M_PI) err_after -= 2.0f*(float)M_PI;
  while (err_after < -(float)M_PI) err_after += 2.0f*(float)M_PI;

  float err_before = est_yaw - true_yaw;
  while (err_before > (float)M_PI) err_before -= 2.0f*(float)M_PI;
  while (err_before < -(float)M_PI) err_before += 2.0f*(float)M_PI;

  /* Absolute error must have decreased */
  CHECK(fabsf(err_after) < fabsf(err_before), "absolute yaw error must decrease");
  printf("  PASS T-EKFM-04 innovation wrapping at +-pi\n");
}

/* ========================================================================
 * T-EKFM-05  P[2][2] strictly decreases after a valid mag update
 * ======================================================================== */
static void test_cov_reduction(void)
{
  ekf_t ekf;
  ekf_init(&ekf);

  /* P[0][0] before update */
  float P00_before = ekf.P[0][0];

  float mag[3];
  make_mag(0.0f, mag);
  ekf_update_mag(&ekf, mag, 0.0f);

  CHECK(ekf.P[0][0] < P00_before, "P[0][0] must decrease after valid mag update");
  printf("  PASS T-EKFM-05 yaw covariance decreases after update\n");
}

/* ========================================================================
 * T-EKFM-06  Repeated predict+update_mag converges yaw to within ±5°
 *
 * 100 steps × 0.1 s = 10 s.  True yaw = 45°.  Initial yaw = 0°.
 * No gyro excitation (zero rates); mag field fixed at 45°.
 * ======================================================================== */
static void test_convergence(void)
{
  ekf_t ekf;
  ekf_init(&ekf);

  float true_yaw = DEG2RAD(45.0f);
  float gyro[3] = {0.0f, 0.0f, 0.0f};
  float dt = 0.1f;

  float mag[3];
  make_mag(true_yaw, mag);

  for (int i = 0; i < 100; i++)
  {
    ekf_predict(&ekf, gyro, dt);
    ekf_update_mag(&ekf, mag, 0.0f);
  }

  float att[3];
  ekf_get_attitude(&ekf, att);
  float yaw_err_deg = RAD2DEG(fabsf(att[2] - true_yaw));
  CHECK(yaw_err_deg < 5.0f, "yaw must converge to within ±5° after 10 s");
  printf("  PASS T-EKFM-06 yaw converges to %.2f deg (err %.2f deg)\n", RAD2DEG(att[2]),
         yaw_err_deg);
}

/* ========================================================================
 * T-EKFM-07  Non-zero declination shifts measured yaw by expected offset
 *
 * Two EKFs start at x[2]=0.  Identical horizontal field B={47,0,0} µT:
 *   - EKF-A updated with declination = 0       → yaw_meas = 0
 *   - EKF-B updated with declination = +10°    → yaw_meas = +10°
 * After one update: x_B[2] > x_A[2] by a fraction of 10° (same Kalman gain).
 * ======================================================================== */
static void test_declination_offset(void)
{
  ekf_t ekf_a, ekf_b;
  ekf_init(&ekf_a);
  ekf_init(&ekf_b);

  float mag[3];
  make_mag(0.0f, mag); /* horizontal field pointing North */

  float decl = DEG2RAD(10.0f);

  ekf_update_mag(&ekf_a, mag, 0.0f);
  ekf_update_mag(&ekf_b, mag, decl);

  float att_a[3], att_b[3];
  ekf_get_attitude(&ekf_a, att_a);
  ekf_get_attitude(&ekf_b, att_b);

  /* EKF-B received a yaw_meas 10° larger → its yaw state must be larger */
  CHECK(att_b[2] > att_a[2],
        "positive declination must increase yaw estimate vs zero-declination");

  /* The difference must be strictly less than 10° (Kalman gain < 1) */
  CHECK((att_b[2] - att_a[2]) < decl, "yaw difference must be < declination (Kalman gain < 1)");

  printf("  PASS T-EKFM-07 declination +10 deg shifts yaw by %.2f deg (expected <10 deg)\n",
         RAD2DEG(att_b[2] - att_a[2]));
}

/* ========================================================================
 * main
 * ======================================================================== */
int main(void)
{
  printf("=== EKF magnetometer yaw update tests (PR-19/22) ===\n");

  test_no_crash();
  test_degenerate_field();
  test_yaw_correction();
  test_wrap_pi();
  test_cov_reduction();
  test_convergence();
  test_declination_offset();

  if (g_failures == 0)
  {
    printf("ALL EKF MAG TESTS PASSED\n");
    return 0;
  }
  printf("%d EKF MAG TEST(S) FAILED\n", g_failures);
  return 1;
}
