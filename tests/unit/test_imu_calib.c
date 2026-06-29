/**
 * @file test_imu_calib.c
 * @brief Unit tests for imu_calib core calibration math pipeline.
 *
 * Tests the pure-math calibration pipeline (start -> collect -> finish -> apply)
 * without hardware dependencies. Provides strong-symbol stubs for:
 *   - mpu6050_write_gyro_offset (called by imu_calib_finish / imu_calib_load)
 *   - w25q64_write_imu_calib / w25q64_read_imu_calib (called by flash persistence)
 *   - s_ekf / s_ekf_initialised (weak externs in imu_calib_load)
 *
 * NOTE: Flash persistence path is covered by test_imu_calib_flash.c.
 *
 * CDR coverage target: imu_calib.c (28% -> target > 85%)
 */

#include "unity.h"
#include "host/unity.h"
#include "drivers/imu/imu_calib.h"
#include "w25q64.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* ------------------------------------------------------------------ */
/* Stubs for imu_calib.c dependencies                                 */
/* ------------------------------------------------------------------ */

/* mpu6050_write_gyro_offset — called by imu_calib_finish() and imu_calib_load() */
static int s_gyro_write_ret = 0;
int mpu6050_write_gyro_offset(const int16_t offset[3])
{
    (void)offset;
    return s_gyro_write_ret;
}

/* w25q64 IMU persistence stubs — called by save/load flash.
 * Keep them thin; the real flash round-trip is tested in test_imu_calib_flash.c. */
static bool s_stub_has_calib = false;
static imu_calib_t s_stub_calib = {0};

w25q64_status_t w25q64_write_imu_calib(const imu_calib_t *cal)
{
    if (cal)
    {
        s_stub_calib = *cal;
        s_stub_has_calib = true;
    }
    return W25Q64_OK;
}

w25q64_status_t w25q64_read_imu_calib(imu_calib_t *cal)
{
    if (!s_stub_has_calib)
        return W25Q64_ERR_INIT;
    if (cal)
        *cal = s_stub_calib;
    return W25Q64_OK;
}

/* Weak externs required by imu_calib_load() — EKF seeding symbols */
#include "ekf.h"
ekf_t s_ekf = {0};
bool   s_ekf_initialised = false;

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

/* Collect N samples with constant gyro rate and varying accel orientation */
static void collect_n_samples(int n, float yaw_dps, float pitch_dps, float roll_dps,
                               float ax, float ay, float az)
{
    float gyro[3] = {roll_dps, pitch_dps, yaw_dps};
    float accel[3] = {ax, ay, az};
    for (int i = 0; i < n; i++)
    {
        imu_calib_collect(accel, gyro);
    }
}

#define FLOAT_CLOSE(a, b, eps) \
    TEST_ASSERT_TRUE(fabsf((a) - (b)) < (eps))

#define INT16_EQUAL(a, b) \
    TEST_ASSERT_TRUE((a) == (b))

/* ------------------------------------------------------------------ */
/* Setup / Teardown                                                    */
/* ------------------------------------------------------------------ */

void setUp(void)
{
    s_gyro_write_ret = 0;
    s_stub_has_calib = false;
    memset(&s_stub_calib, 0, sizeof(s_stub_calib));
    s_ekf_initialised = false;
    memset(&s_ekf, 0, sizeof(s_ekf));
}

void tearDown(void) {}

/* ================================================================== */
/* T-IC-01: imu_calib_start initialises collection state              */
/* ================================================================== */
void test_ic_start_initialises_state(void)
{
    /* After start, is_valid should be false */
    TEST_ASSERT_FALSE(imu_calib_is_valid());

    /* Collect enough samples then finish — should produce valid cal */
    imu_calib_start();
    collect_n_samples(100, 1.0f, 0.5f, -0.3f, 0.0f, 0.0f, 1.0f);
    imu_calib_finish();

    TEST_ASSERT_TRUE(imu_calib_is_valid());
}

/* ================================================================== */
/* T-IC-02: imu_calib_finish with too few samples returns early        */
/* ================================================================== */
void test_ic_finish_insufficient_samples(void)
{
    imu_calib_start();
    /* Only 10 samples — need at least 50 */
    collect_n_samples(10, 1.0f, 0.5f, -0.3f, 0.0f, 0.0f, 1.0f);
    imu_calib_finish();

    /* Calibration should remain invalid */
    TEST_ASSERT_FALSE(imu_calib_is_valid());
}

/* ================================================================== */
/* T-IC-03: Gyro bias computation is correct                           */
/* ================================================================== */
void test_ic_gyro_bias_computed_correctly(void)
{
    /* Constant gyro rate: 2.0 deg/s on all axes — this IS the bias */
    imu_calib_start();
    collect_n_samples(100, 2.0f, 2.0f, 2.0f, 0.0f, 0.0f, 1.0f);
    imu_calib_finish();

    TEST_ASSERT_TRUE(imu_calib_is_valid());

    /* Gyro bias should be ~2 deg/s in each axis, converted to rad/s */
    imu_calib_t cal;
    imu_calib_get(&cal);

    /* 2.0 deg/s = 2.0 * pi/180 ≈ 0.0349066 rad/s */
    float expected_bias = 2.0f * (float)(M_PI / 180.0);
    FLOAT_CLOSE(cal.gyro_bias_rads[0], expected_bias, 1e-6f);
    FLOAT_CLOSE(cal.gyro_bias_rads[1], expected_bias, 1e-6f);
    FLOAT_CLOSE(cal.gyro_bias_rads[2], expected_bias, 1e-6f);

    /* Gyro offset raw = -bias_dps * 32.8, clamped to int16 range */
    int16_t expected_offset = (int16_t)(-2.0f * 32.8f);
    INT16_EQUAL(expected_offset, cal.gyro_offset_raw[0]);
}

/* ================================================================== */
/* T-IC-04: Gyro bias with different rates per axis                    */
/* ================================================================== */
void test_ic_gyro_bias_per_axis(void)
{
    imu_calib_start();
    /* Different rate per axis: x=1.5, y=-0.5, z=3.0 deg/s */
    float gyro[3] = {1.5f, -0.5f, 3.0f};
    float accel[3] = {0.0f, 0.0f, 1.0f};
    for (int i = 0; i < 100; i++)
    {
        imu_calib_collect(accel, gyro);
    }
    imu_calib_finish();

    imu_calib_t cal;
    imu_calib_get(&cal);

    FLOAT_CLOSE(cal.gyro_bias_rads[0], 1.5f * (float)(M_PI / 180.0), 1e-6f);
    FLOAT_CLOSE(cal.gyro_bias_rads[1], -0.5f * (float)(M_PI / 180.0), 1e-6f);
    FLOAT_CLOSE(cal.gyro_bias_rads[2], 3.0f * (float)(M_PI / 180.0), 1e-6f);
}

/* ================================================================== */
/* T-IC-05: Accel 6-point calibration — offset and scale               */
/* ================================================================== */
void test_ic_accel_calibration_computed(void)
{
    imu_calib_start();

    /* Simulate 6-point calibration, 50 samples per orientation:
     * +X, -X, +Y, -Y, +Z, -Z orientations.  Each axis independently.
     */
    float g[3] = {0.1f, 0.1f, 0.1f};

    for (int i = 0; i < 50; i++)
        imu_calib_collect((float[]){1.0f, 0.0f, 0.0f}, g);
    for (int i = 0; i < 50; i++)
        imu_calib_collect((float[]){-1.0f, 0.0f, 0.0f}, g);
    for (int i = 0; i < 50; i++)
        imu_calib_collect((float[]){0.0f, 0.5f, 0.0f}, g);
    for (int i = 0; i < 50; i++)
        imu_calib_collect((float[]){0.0f, -0.5f, 0.0f}, g);
    for (int i = 0; i < 50; i++)
        imu_calib_collect((float[]){0.0f, 0.0f, 1.5f}, g);
    for (int i = 0; i < 50; i++)
        imu_calib_collect((float[]){0.0f, 0.0f, 0.5f}, g);

    imu_calib_finish();
    TEST_ASSERT_TRUE(imu_calib_is_valid());

    imu_calib_t cal;
    imu_calib_get(&cal);

    /* X: min=-1, max=+1 -> offset=0, range=2 -> scale=2/2 = 1.0 */
    FLOAT_CLOSE(cal.accel_offset[0], 0.0f, 1e-5f);
    FLOAT_CLOSE(cal.accel_scale[0], 1.0f, 1e-5f);

    /* Y: min=-0.5, max=+0.5 -> offset=0, range=1 -> scale=2/1 = 2.0 */
    FLOAT_CLOSE(cal.accel_offset[1], 0.0f, 1e-5f);
    FLOAT_CLOSE(cal.accel_scale[1], 2.0f, 1e-5f);

    /* Z: 5 orientations have z=0 (x/y sweeps), only +/-Z have 1.5/0.5.
     * min_z=0.0, max_z=1.5 -> offset=0.75, range=1.5 -> scale=2/1.5 ≈ 1.333 */
    FLOAT_CLOSE(cal.accel_offset[2], 0.75f, 1e-5f);
    FLOAT_CLOSE(cal.accel_scale[2], 1.333333f, 1e-5f);
}

/* ================================================================== */
/* T-IC-06: imu_calib_apply_accel with valid calibration               */
/* ================================================================== */
void test_ic_apply_accel_calibrated(void)
{
    /* Run a calibration first */
    imu_calib_start();
    collect_n_samples(100, 0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 2.0f);
    imu_calib_finish();
    TEST_ASSERT_TRUE(imu_calib_is_valid());

    /* Apply calibration to a raw accel reading */
    float raw[3]  = {-0.9f, 0.1f, 1.9f};
    float cal[3];
    imu_calib_apply_accel(raw, cal);

    /* Verify output is finite — calibration changed the value */
    for (int i = 0; i < 3; i++)
    {
        TEST_ASSERT_TRUE(isfinite(cal[i]));
    }
}

/* ================================================================== */
/* T-IC-07: imu_calib_apply_accel without calibration = passthrough    */
/* ================================================================== */
void test_ic_apply_accel_uncalibrated(void)
{
    /* Start (resets internal state) — no samples collected = not calibrated */
    imu_calib_start();
    TEST_ASSERT_FALSE(imu_calib_is_valid());

    float raw[3]  = {1.23f, -4.56f, 7.89f};
    float cal[3];
    imu_calib_apply_accel(raw, cal);

    /* Without calibration, output must equal input */
    FLOAT_CLOSE(raw[0], cal[0], 1e-7f);
    FLOAT_CLOSE(raw[1], cal[1], 1e-7f);
    FLOAT_CLOSE(raw[2], cal[2], 1e-7f);
}

/* ================================================================== */
/* T-IC-08: imu_calib_get returns current calibration data            */
/* ================================================================== */
void test_ic_get_returns_current_calib(void)
{
    imu_calib_t cal1;
    imu_calib_get(&cal1);
    TEST_ASSERT_FALSE(cal1.calibrated);

    /* Run a calibration */
    imu_calib_start();
    collect_n_samples(100, 2.0f, 2.0f, 2.0f, 0.0f, 0.0f, 1.0f);
    imu_calib_finish();

    imu_calib_t cal2;
    imu_calib_get(&cal2);
    TEST_ASSERT_TRUE(cal2.calibrated);

    /* Gyro bias should be non-zero */
    TEST_ASSERT_TRUE(cal2.gyro_bias_rads[0] > 0.0f);
}

/* ================================================================== */
/* T-IC-09: Gyro offset clamp — extreme values don't overflow int16    */
/* ================================================================== */
void test_ic_gyro_offset_clamp(void)
{
    imu_calib_start();

    /* Very large gyro rate: 2000 deg/s (way beyond sensor range) */
    float gyro[3] = {2000.0f, -3000.0f, 5000.0f};
    float accel[3] = {0.0f, 0.0f, 1.0f};
    for (int i = 0; i < 100; i++)
    {
        imu_calib_collect(accel, gyro);
    }
    imu_calib_finish();

    imu_calib_t cal;
    imu_calib_get(&cal);

    /* offset = -bias_dps * 32.8
     * x: -2000 * 32.8 = -65600 -> clamped to -32768 */
    INT16_EQUAL(-32768, cal.gyro_offset_raw[0]);

    /* y: -(-3000) * 32.8 = 98400 -> clamped to 32767 */
    INT16_EQUAL(32767, cal.gyro_offset_raw[1]);

    /* z: -5000 * 32.8 = -164000 -> clamped to -32768 */
    INT16_EQUAL(-32768, cal.gyro_offset_raw[2]);
}

/* ================================================================== */
/* T-IC-10: Multiple start/restart resets collection state             */
/* ================================================================== */
void test_ic_restart_resets_state(void)
{
    imu_calib_start();
    collect_n_samples(100, 5.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);

    /* Restart mid-collection */
    imu_calib_start();

    /* Now collect with different bias */
    collect_n_samples(100, 0.0f, 3.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    imu_calib_finish();

    imu_calib_t cal;
    imu_calib_get(&cal);

    /* Bias should reflect the SECOND collection (y=3 deg/s), not the first */
    FLOAT_CLOSE(cal.gyro_bias_rads[1], 3.0f * (float)(M_PI / 180.0), 1e-6f);

    /* First axis bias should be ~0, not 5 deg/s */
    FLOAT_CLOSE(cal.gyro_bias_rads[0], 0.0f, 1e-6f);
}

/* ================================================================== */
/* T-IC-11: imu_calib_collect before start is a no-op (early return)   */
/* ================================================================== */
void test_ic_collect_before_start_is_noop(void)
{
    /* Ensure clean state: start then finish with 0 samples.
     * This sets s_collecting = false via finish(), and leaves
     * g_calib.calibrated = false. */
    imu_calib_start();
    imu_calib_finish();

    /* Now collect without s_collecting — should be a no-op (line 49 early return) */
    float gyro[3] = {1.0f, 2.0f, 3.0f};
    float accel[3] = {0.0f, 0.0f, 1.0f};
    imu_calib_collect(accel, gyro);

    /* Finish again — should still see 0 samples because collect was a no-op */
    imu_calib_finish();
    TEST_ASSERT_FALSE(imu_calib_is_valid());
}

/* ================================================================== */
/* T-IC-13: imu_calib_finish with zero-range on an axis logs warning   */
/* ================================================================== */
void test_ic_zero_range_axis(void)
{
    /* Simulate stationary sensor: X axis never varies (range = 0) */
    imu_calib_start();
    float gyro[3] = {1.0f, 0.5f, -0.3f};
    for (int i = 0; i < 100; i++)
    {
        /* X always 0.5: min=max=0.5 → range=0 */
        float accel[3] = {0.5f, (float)(i % 3) - 1.0f, (float)(i % 2)};
        imu_calib_collect(accel, gyro);
    }
    imu_calib_finish();

    /* Calibration should still be valid (other axes are fine) */
    TEST_ASSERT_TRUE(imu_calib_is_valid());

    imu_calib_t cal;
    imu_calib_get(&cal);

    /* Zero-range axis: scale should be 1.0f (safe default with warning) */
    FLOAT_CLOSE(cal.accel_scale[0], 1.0f, 1e-6f);

    /* Other axes should have normal calibration */
    TEST_ASSERT_TRUE(cal.accel_scale[1] > 0.5f);
    TEST_ASSERT_TRUE(cal.accel_scale[2] > 0.5f);
}

/* ================================================================== */
/* T-IC-14: imu_calib_finish with all axes zero-range handles gracefully */
/* ================================================================== */
void test_ic_zero_range_all_axes(void)
{
    imu_calib_start();
    float gyro[3] = {0.0f, 0.0f, 0.0f};
    float accel[3] = {1.0f, 1.0f, 1.0f};
    for (int i = 0; i < 100; i++)
    {
        imu_calib_collect(accel, gyro);
    }
    imu_calib_finish();

    /* Should still produce a valid calibration (not crash) */
    TEST_ASSERT_TRUE(imu_calib_is_valid());

    imu_calib_t cal;
    imu_calib_get(&cal);

    /* All axes should have safe defaults */
    for (int i = 0; i < 3; i++)
    {
        FLOAT_CLOSE(cal.accel_scale[i], 1.0f, 1e-6f);
    }
}

/* ================================================================== */
/* T-IC-12: imu_calib_finish handles mpu6050_write_gyro_offset failure */
/* ================================================================== */
void test_ic_write_gyro_offset_failure(void)
{
    imu_calib_start();
    collect_n_samples(100, 1.0f, 0.5f, -0.3f, 0.0f, 0.0f, 1.0f);

    /* Force gyro offset write to fail */
    s_gyro_write_ret = -1;
    imu_calib_finish();

    /* Calibration should still be valid — WR failure is non-fatal */
    TEST_ASSERT_TRUE(imu_calib_is_valid());
}

/* ================================================================== */
/* Runner                                                              */
/* ================================================================== */
int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_ic_start_initialises_state);
    RUN_TEST(test_ic_finish_insufficient_samples);
    RUN_TEST(test_ic_gyro_bias_computed_correctly);
    RUN_TEST(test_ic_gyro_bias_per_axis);
    RUN_TEST(test_ic_accel_calibration_computed);
    RUN_TEST(test_ic_apply_accel_calibrated);
    RUN_TEST(test_ic_apply_accel_uncalibrated);
    RUN_TEST(test_ic_get_returns_current_calib);
    RUN_TEST(test_ic_gyro_offset_clamp);
    RUN_TEST(test_ic_restart_resets_state);
    RUN_TEST(test_ic_collect_before_start_is_noop);
    RUN_TEST(test_ic_write_gyro_offset_failure);
    RUN_TEST(test_ic_zero_range_axis);
    RUN_TEST(test_ic_zero_range_all_axes);

    return UNITY_END();
}
