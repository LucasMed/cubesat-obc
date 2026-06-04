/**
 * @file test_imu_calib_flash.c
 * @brief End-to-end test: imu_calib save/load through W25Q64 flash.
 *
 * Verifies that imu_calib_save_to_flash() and imu_calib_load_from_flash()
 * correctly persist and restore calibration data via the w25q64 host stubs.
 *
 * Depends on imu_calib.c (save/load wiring) + w25q64.c host stubs (buffer).
 */

#include "unity.h"
#include "drivers/imu/imu_calib.h"
#include "w25q64.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/* Stubs for imu_calib.c dependencies                                  */
/* ------------------------------------------------------------------ */

/* imu_calib_load() calls mpu6050_write_gyro_offset to program HW      *
 * registers. Stub it — we only test the flash persistence path here.   */
int mpu6050_write_gyro_offset(const int16_t offset[3])
{
    (void)offset;
    return 0; /* success */
}

/* imu_calib_load() references extern EKF symbols via __attribute__((weak)).
 * On macOS/Clang, weak variable externs need actual definitions.
 * Provide zero-initialised stubs — the seed-s_ekf_initialised check
 * will be false, so the EKF seeding branch is correctly skipped.       */
#include "ekf.h"
ekf_t s_ekf = {0};
bool s_ekf_initialised = false;

/* ------------------------------------------------------------------ */
/* Tests                                                               */
/* ------------------------------------------------------------------ */

void setUp(void) {}
void tearDown(void) {}

void test_imu_calib_flash_save_then_load_restores_data(void)
{
    /* Create known calibration data */
    imu_calib_t cal_original;
    memset(&cal_original, 0, sizeof(cal_original));
    cal_original.accel_offset[0] = 0.10f;
    cal_original.accel_offset[1] = -0.05f;
    cal_original.accel_offset[2] = 0.20f;
    cal_original.accel_scale[0] = 1.0f;
    cal_original.accel_scale[1] = 1.02f;
    cal_original.accel_scale[2] = 0.97f;
    cal_original.gyro_offset_raw[0] = 10;
    cal_original.gyro_offset_raw[1] = -8;
    cal_original.gyro_offset_raw[2] = 4;
    cal_original.gyro_bias_rads[0] = 0.002f;
    cal_original.gyro_bias_rads[1] = -0.001f;
    cal_original.gyro_bias_rads[2] = 0.003f;
    cal_original.calibrated = true;

    /* Load into internal state (sets g_calib, calls stub mpu6050) */
    imu_calib_load(&cal_original);

    /* Persist to flash via imu_calib_save_to_flash */
    imu_calib_save_to_flash();

    /* Overwrite internal state with different data */
    imu_calib_t cal_diff;
    memset(&cal_diff, 0, sizeof(cal_diff));
    cal_diff.accel_offset[0] = 99.0f;
    cal_diff.calibrated = true;
    imu_calib_load(&cal_diff);

    /* Restore from flash */
    imu_calib_load_from_flash();

    /* Verify restored data matches original */
    imu_calib_t cal_restored;
    imu_calib_get(&cal_restored);

    TEST_ASSERT_TRUE(cal_restored.accel_offset[0] == 0.10f);
    TEST_ASSERT_TRUE(cal_restored.accel_offset[1] == -0.05f);
    TEST_ASSERT_TRUE(cal_restored.accel_offset[2] == 0.20f);
    TEST_ASSERT_TRUE(cal_restored.accel_scale[0] == 1.0f);
    TEST_ASSERT_TRUE(cal_restored.accel_scale[1] == 1.02f);
    TEST_ASSERT_TRUE(cal_restored.accel_scale[2] == 0.97f);
    TEST_ASSERT_TRUE(cal_restored.gyro_offset_raw[0] == 10);
    TEST_ASSERT_TRUE(cal_restored.gyro_offset_raw[1] == -8);
    TEST_ASSERT_TRUE(cal_restored.gyro_offset_raw[2] == 4);
    TEST_ASSERT_TRUE(cal_restored.gyro_bias_rads[0] == 0.002f);
    TEST_ASSERT_TRUE(cal_restored.gyro_bias_rads[1] == -0.001f);
    TEST_ASSERT_TRUE(cal_restored.gyro_bias_rads[2] == 0.003f);
    TEST_ASSERT_TRUE(cal_restored.calibrated);
}

void test_imu_calib_flash_load_from_empty(void)
{
    /* Reset flash state by clearing the w25q64 host buffer.
     * The host stub's s_imu_calib_valid is already false after init,
     * but test_w25q64 may have written before us. Reload to verify.
     * Since w25q64 is statically initialised, a fresh read without prior
     * write should fail — imu_calib_load_from_flash must handle this. */

    /* Call load_from_flash on what should be empty (no prior write
     * in this test). The function must not crash. */
    imu_calib_load_from_flash();

    /* Calibration should remain invalid */
    TEST_ASSERT_FALSE(imu_calib_is_valid());
}

int main(void)
{
    UNITY_BEGIN();

    /* Read-empty first, before any writes */
    RUN_TEST(test_imu_calib_flash_load_from_empty);
    RUN_TEST(test_imu_calib_flash_save_then_load_restores_data);

    return UNITY_END();
}
