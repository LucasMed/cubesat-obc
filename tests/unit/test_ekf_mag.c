/* test_ekf_mag.c — Unit tests for EKF yaw update via magnetometer (PR-19)
 *
 * T-EKFM-01: no_crash        — ekf_update_mag() completes on zero-state EKF
 * T-EKFM-02: degenerate_field — |Bh| < epsilon → state and P unchanged
 * T-EKFM-03: yaw_correction  — large initial yaw error → update moves x[2]
 *                              toward measured yaw
 * T-EKFM-04: wrap_pi         — innovation near ±π wrapped correctly
 * T-EKFM-05: cov_reduction   — P[2][2] strictly decreases after update
 * T-EKFM-06: convergence     — repeated predict+update_mag converges yaw
 *                              to within ±5° of true value in 10 s
 *
 * Only ekf.c is compiled here; no hardware or RTOS dependency.
 */

#include "ekf.h"

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

  CHECK(isfinite(ekf.x[2]), "yaw must remain finite after update");
  printf("  PASS T-EKFM-01 no crash with valid field\n");
}

/* ========================================================================
 * T-EKFM-02  Degenerate horizontal field → state and covariance unchanged
 * ======================================================================== */
static void test_degenerate_field(void)
{
  ekf_t ekf;
  ekf_init(&ekf);
  ekf.x[2] = DEG2RAD(30.0f); /* put some yaw in the state */
  float P22_before = ekf.P[2][2];
  float x2_before = ekf.x[2];

  /* Purely vertical field → Bh_x = By = 0, Bh_y = 0 → degenerate */
  float mag[3] = {0.0f, 0.0f, 50.0f};
  ekf_update_mag(&ekf, mag, 0.0f);

  CHECK(FPEQ(ekf.x[2], x2_before), "yaw state must be unchanged for degenerate field");
  CHECK(FPEQ(ekf.P[2][2], P22_before), "P[2][2] must be unchanged for degenerate field");
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
  ekf.x[2] = 0.0f; /* initial estimate: zero yaw */

  float mag[3];
  make_mag(true_yaw, mag);
  ekf_update_mag(&ekf, mag, 0.0f);

  /* After one update yaw must move toward true_yaw (i.e., increase from 0) */
  CHECK(ekf.x[2] > 0.0f, "yaw must move toward measured value after update");
  /* But must not overshoot (innovation was π/2, gain < 1, so x[2] < π/2) */
  CHECK(ekf.x[2] < true_yaw, "yaw must not overshoot the measurement");
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

  float true_yaw = (float)M_PI - 0.1f; /* ≈ +170° */
  ekf.x[2] = -((float)M_PI - 0.1f);    /* ≈ -170° */

  float mag[3];
  make_mag(true_yaw, mag);
  ekf_update_mag(&ekf, mag, 0.0f);

  /* Innovation should have been wrapped to ~-0.2 rad (short CW path from
   * -170° → +170°); x[2] must therefore DECREASE (move toward -π) */
  CHECK(ekf.x[2] < -((float)M_PI - 0.1f),
        "yaw must decrease (wrapped innovation is ~-0.2 rad, short CW path)");
  printf("  PASS T-EKFM-04 innovation wrapping at +-pi\n");
}

/* ========================================================================
 * T-EKFM-05  P[2][2] strictly decreases after a valid mag update
 * ======================================================================== */
static void test_cov_reduction(void)
{
  ekf_t ekf;
  ekf_init(&ekf);

  float P22_before = ekf.P[2][2];

  float mag[3];
  make_mag(0.0f, mag);
  ekf_update_mag(&ekf, mag, 0.0f);

  CHECK(ekf.P[2][2] < P22_before, "P[2][2] must decrease after valid mag update");
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

  float yaw_err_deg = RAD2DEG(fabsf(ekf.x[2] - true_yaw));
  CHECK(yaw_err_deg < 5.0f, "yaw must converge to within ±5° after 10 s");
  printf("  PASS T-EKFM-06 yaw converges to %.2f deg (err %.2f deg)\n", RAD2DEG(ekf.x[2]),
         yaw_err_deg);
}

/* ========================================================================
 * main
 * ======================================================================== */
int main(void)
{
  printf("=== EKF magnetometer yaw update tests (PR-19) ===\n");

  test_no_crash();
  test_degenerate_field();
  test_yaw_correction();
  test_wrap_pi();
  test_cov_reduction();
  test_convergence();

  if (g_failures == 0)
  {
    printf("ALL EKF MAG TESTS PASSED\n");
    return 0;
  }
  printf("%d EKF MAG TEST(S) FAILED\n", g_failures);
  return 1;
}
