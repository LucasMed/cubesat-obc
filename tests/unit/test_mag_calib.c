/* test_mag_calib.c — Unit tests for mag_calib hard-iron calibration
 *
 * T-MAG-CAL-01  test_mc_start_resets_state   – start() initialises min/max to ±∞
 * T-MAG-CAL-02  test_mc_too_few_samples      – finish() with <50 samples returns error (calibrated=false)
 * T-MAG-CAL-03  test_mc_collect_minmax        – collect() tracks min/max correctly
 * T-MAG-CAL-04  test_mc_apply_uncalibrated    – apply() passes through before calibration
 * T-MAG-CAL-05  test_mc_apply_calibrated      – apply() subtracts offsets after calibration
 * T-MAG-CAL-06  test_mc_is_valid              – is_valid() reflects calibrated flag
 * T-MAG-CAL-07  test_mc_get                   – get() copies calibration data
 * T-MAG-CAL-08  test_mc_load_ok               – load() with calibrated=true restores cal
 * T-MAG-CAL-09  test_mc_load_nop              – load() with calibrated=false is a no-op
 * T-MAG-CAL-10  test_mc_100_samples_log       – every 100 samples triggers progress log
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "mag_calib.h"

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

#define FLOAT_NEAR(a, b, tol, msg)                                                                 \
  do                                                                                               \
  {                                                                                                \
    float _a = (a), _b = (b);                                                                      \
    if (fabsf(_a - _b) > (tol))                                                                    \
    {                                                                                              \
      printf("  FAIL [%s:%d] %s: expected %.4f got %.4f (tol %.4f)\n",                             \
             __func__, __LINE__, msg, (double)_b, (double)_a, (double)(tol));                      \
      g_failures++;                                                                                \
    }                                                                                              \
  } while (0)

/* ========================================================================
 * T-MAG-CAL-01  start() resets state and sets min/max to ±INFINITY
 * ======================================================================== */
static void test_mc_start_resets_state(void)
{
  printf("[T-MAG-CAL-01] test_mc_start_resets_state\n");
  int failures_before = g_failures;

  /* Run a prior calibration so state is dirty */
  float samples[60][3];
  for (int i = 0; i < 60; i++)
  {
    samples[i][0] = (float)(i - 30); /* x: -30 .. 29 */
    samples[i][1] = (float)(2 * (i - 30)); /* y: -60 .. 58 */
    samples[i][2] = (float)(10 * (i - 30)); /* z: -300 .. 290 */
  }
  float out[3];

  mag_calib_start();
  for (int i = 0; i < 60; i++)
    mag_calib_collect(samples[i]);
  mag_calib_finish();
  mag_calib_apply(samples[0], out);

  /* Now restart — should wipe everything */
  mag_calib_start();
  CHECK(!mag_calib_is_valid(), "not calibrated after restart");
  /* Collect one far-off sample and check finish fails (only 1 sample) */
  {
    float single[3] = {999.0f, 999.0f, 999.0f};
    mag_calib_collect(single);
  }
  mag_calib_finish();
  CHECK(!mag_calib_is_valid(), "still not calibrated after single sample finish");

  printf("  %s\n", g_failures == failures_before ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-MAG-CAL-02  finish() with <50 samples → calibrated=false
 * ======================================================================== */
static void test_mc_too_few_samples(void)
{
  printf("[T-MAG-CAL-02] test_mc_too_few_samples\n");
  int failures_before = g_failures;

  mag_calib_start();
  for (int i = 0; i < 49; i++)
  {
    float v[3] = {(float)i, (float)i, (float)i};
    mag_calib_collect(v);
  }
  mag_calib_finish();
  CHECK(!mag_calib_is_valid(), "calibrated is false with 49 samples");

  printf("  %s\n", g_failures == failures_before ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-MAG-CAL-03  collect() tracks min/max correctly
 * ======================================================================== */
static void test_mc_collect_minmax(void)
{
  printf("[T-MAG-CAL-03] test_mc_collect_minmax\n");
  int failures_before = g_failures;

  mag_calib_start();

  /* Feed samples where each axis has a known min and max */
  float samples[][3] = {
      {-10.0f, -20.0f, -30.0f},
      { 40.0f,  50.0f,  60.0f},
      {  5.0f,   0.0f,  15.0f},
      { -5.0f,  10.0f, -10.0f},
  };
  int n = sizeof(samples) / sizeof(samples[0]);

  for (int i = 0; i < 50; i++)
    mag_calib_collect(samples[i % n]);

  mag_calib_finish();
  CHECK(mag_calib_is_valid(), "calibrated is true with 50 samples");

  /* Expected offsets: midpoint of min/max
   * x: min=-10, max=40  -> offset = (-10+40)/2 = 15
   * y: min=-20, max=50  -> offset = (-20+50)/2 = 15
   * z: min=-30, max=60  -> offset = (-30+60)/2 = 15 */
  mag_calib_t cal;
  mag_calib_get(&cal);
  FLOAT_NEAR(cal.offset[0], 15.0f, 0.01f, "x offset = 15");
  FLOAT_NEAR(cal.offset[1], 15.0f, 0.01f, "y offset = 15");
  FLOAT_NEAR(cal.offset[2], 15.0f, 0.01f, "z offset = 15");

  /* Scale should be 1.0 (no soft-iron) */
  FLOAT_NEAR(cal.scale[0], 1.0f, 0.0f, "x scale = 1.0");
  FLOAT_NEAR(cal.scale[1], 1.0f, 0.0f, "y scale = 1.0");
  FLOAT_NEAR(cal.scale[2], 1.0f, 0.0f, "z scale = 1.0");

  printf("  %s\n", g_failures == failures_before ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-MAG-CAL-04  apply() before calibration → pass-through
 * ======================================================================== */
static void test_mc_apply_uncalibrated(void)
{
  printf("[T-MAG-CAL-04] test_mc_apply_uncalibrated\n");
  int failures_before = g_failures;

  /* Start resets the internal calibrated flag */
  mag_calib_start();

  float raw[3] = {12.34f, -56.78f, 90.12f};
  float out[3] = {0};

  mag_calib_apply(raw, out);

  CHECK(out[0] == 12.34f, "x passed through");
  CHECK(out[1] == -56.78f, "y passed through");
  CHECK(out[2] == 90.12f, "z passed through");

  printf("  %s\n", g_failures == failures_before ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-MAG-CAL-05  apply() after calibration → raw - offset
 * ======================================================================== */
static void test_mc_apply_calibrated(void)
{
  printf("[T-MAG-CAL-05] test_mc_apply_calibrated\n");
  int failures_before = g_failures;

  /* Calibrate with asymmetric bounds */
  mag_calib_start();
  for (int i = 0; i < 50; i++)
  {
    float v[3] = {(float)i, (float)(i + 100), (float)(i - 200)};
    mag_calib_collect(v);
  }
  mag_calib_finish();
  CHECK(mag_calib_is_valid(), "calibrated after 50 samples");

  /* offsets: x=(0+49)/2=24.5, y=(100+149)/2=124.5, z=(-200+-151)/2=-175.5 */
  float raw[3] = {100.0f, 200.0f, -100.0f};
  float out[3] = {0};
  mag_calib_apply(raw, out);

  FLOAT_NEAR(out[0], 100.0f - 24.5f, 0.01f, "x = raw - offset");
  FLOAT_NEAR(out[1], 200.0f - 124.5f, 0.01f, "y = raw - offset");
  FLOAT_NEAR(out[2], -100.0f - (-175.5f), 0.01f, "z = raw - offset");

  printf("  %s\n", g_failures == failures_before ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-MAG-CAL-06  is_valid() reflects internal calibrated flag
 * ======================================================================== */
static void test_mc_is_valid(void)
{
  printf("[T-MAG-CAL-06] test_mc_is_valid\n");
  int failures_before = g_failures;

  /* Ensure clean uncalibrated state */
  mag_calib_start();
  CHECK(!mag_calib_is_valid(), "invalid before start");

  mag_calib_start();
  CHECK(!mag_calib_is_valid(), "invalid during collection");

  for (int i = 0; i < 50; i++)
  {
    float v[3] = {1.0f, 2.0f, 3.0f};
    mag_calib_collect(v);
  }
  mag_calib_finish();
  CHECK(mag_calib_is_valid(), "valid after finish");

  printf("  %s\n", g_failures == failures_before ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-MAG-CAL-07  get() copies calibration struct
 * ======================================================================== */
static void test_mc_get(void)
{
  printf("[T-MAG-CAL-07] test_mc_get\n");
  int failures_before = g_failures;

  /* get() on uncalibrated state */
  mag_calib_start();
  mag_calib_t c1;
  memset(&c1, 0xAA, sizeof(c1)); /* fill with garbage */
  mag_calib_get(&c1);
  CHECK(!c1.calibrated, "uncalibrated get returns calibrated=false");

  /* Calibrate then get */
  mag_calib_start();
  for (int i = 0; i < 50; i++)
  {
    float v[3] = {0.0f, (float)i, (float)(2 * i)};
    mag_calib_collect(v);
  }
  mag_calib_finish();

  mag_calib_t c2;
  memset(&c2, 0, sizeof(c2));
  mag_calib_get(&c2);
  CHECK(c2.calibrated, "calibrated get returns true");
  FLOAT_NEAR(c2.offset[1], 24.5f, 0.01f, "y offset = (0+49)/2");

  /* NULL pointer is safe */
  mag_calib_get(NULL);

  printf("  %s\n", g_failures == failures_before ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-MAG-CAL-08  load() with calibrated=true restores state
 * ======================================================================== */
static void test_mc_load_ok(void)
{
  printf("[T-MAG-CAL-08] test_mc_load_ok\n");
  int failures_before = g_failures;

  mag_calib_t cal = {
      .offset = {12.5f, -34.7f, 99.9f},
      .scale = {1.0f, 1.0f, 1.0f},
      .calibrated = true,
  };
  mag_calib_load(&cal);
  CHECK(mag_calib_is_valid(), "valid after load");

  float raw[3] = {50.0f, 50.0f, 50.0f};
  float out[3] = {0};
  mag_calib_apply(raw, out);

  FLOAT_NEAR(out[0], 50.0f - 12.5f, 0.01f, "x = raw - 12.5");
  FLOAT_NEAR(out[1], 50.0f - (-34.7f), 0.01f, "y = raw - (-34.7)");
  FLOAT_NEAR(out[2], 50.0f - 99.9f, 0.01f, "z = raw - 99.9");

  printf("  %s\n", g_failures == failures_before ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-MAG-CAL-09  load() with calibrated=false → no-op
 * ======================================================================== */
static void test_mc_load_nop(void)
{
  printf("[T-MAG-CAL-09] test_mc_load_nop\n");
  int failures_before = g_failures;

  /* Set state to calibrated first */
  mag_calib_start();
  for (int i = 0; i < 50; i++)
  {
    float v[3] = {0.0f, 0.0f, 0.0f};
    mag_calib_collect(v);
  }
  mag_calib_finish();
  CHECK(mag_calib_is_valid(), "calibrated before load-nop");

  /* Now load with calibrated=false */
  mag_calib_t cal = {.offset = {1.0f, 2.0f, 3.0f}, .calibrated = false};
  mag_calib_load(&cal);

  /* Should still be calibrated with OLD offsets */
  float raw[3] = {10.0f, 20.0f, 30.0f};
  float out[3] = {0};
  mag_calib_apply(raw, out);

  /* offsets from the zero-collect: all (0+0)/2 = 0 */
  FLOAT_NEAR(out[0], 10.0f, 0.01f, "x unchanged (old offset 0)");
  FLOAT_NEAR(out[1], 20.0f, 0.01f, "y unchanged (old offset 0)");
  FLOAT_NEAR(out[2], 30.0f, 0.01f, "z unchanged (old offset 0)");

  printf("  %s\n", g_failures == failures_before ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-MAG-CAL-10  collect() with 100 samples triggers progress log
 * ======================================================================== */
static void test_mc_100_samples_log(void)
{
  printf("[T-MAG-CAL-10] test_mc_100_samples_log\n");
  int failures_before = g_failures;

  mag_calib_start();
  for (int i = 0; i < 150; i++)
  {
    float v[3] = {(float)i, (float)i, (float)i};
    mag_calib_collect(v);
  }
  mag_calib_finish();
  CHECK(mag_calib_is_valid(), "valid after 150 samples");

  /* Offsets: x=(0+149)/2=74.5 */
  mag_calib_t cal;
  mag_calib_get(&cal);
  FLOAT_NEAR(cal.offset[0], 74.5f, 0.01f, "x offset = 74.5 after 150 samples");

  printf("  %s\n", g_failures == failures_before ? "PASS" : "FAIL");
}

/* ======================================================================== */
int main(void)
{
  printf("=== Magnetometer Calibration Unit Tests ===\n");
  test_mc_start_resets_state();
  test_mc_too_few_samples();
  test_mc_collect_minmax();
  test_mc_apply_uncalibrated();
  test_mc_apply_calibrated();
  test_mc_is_valid();
  test_mc_get();
  test_mc_load_ok();
  test_mc_load_nop();
  test_mc_100_samples_log();
  printf("============================================\n");
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
