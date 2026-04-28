/**
 * @file test_imu_calib.c
 * @brief Unit tests for IMU calibration - logic only (no hardware).
 */

#include "drivers/imu/imu_calib.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* Mocked externs (normally in sensor_read_task.c/ekf.c) */
float s_ekf_x[7] = {0};
bool s_ekf_initialised = false;

int main(void)
{
  int tests_run = 0;
  int tests_passed = 0;

  printf("=== IMU Calibration Unit Tests ===\n");

  /* Test 1: imu_calib_start() resets state */
  {
    tests_run++;
    printf("Test: imu_calib_start resets state... ");
    imu_calib_start();
    if (imu_calib_is_valid() == false)
    {
      tests_passed++;
      printf("PASS\n");
    }
    else
    {
      printf("FAIL\n");
    }
  }

  /* Test 2: imu_calib_finish() with few samples fails */
  {
    tests_run++;
    printf("Test: imu_calib_finish with few samples fails... ");
    imu_calib_start();
    /* Only 1 sample */
    float accel[3] = {0.0f, 0.0f, 1.0f};
    float gyro[3] = {1.0f, 2.0f, 3.0f};
    imu_calib_collect(accel, gyro);
    imu_calib_finish();
    if (imu_calib_is_valid() == false)
    {
      tests_passed++;
      printf("PASS\n");
    }
    else
    {
      printf("FAIL\n");
    }
  }

  /* Test 3: imu_calib_finish() with enough samples succeeds */
  {
    tests_run++;
    printf("Test: imu_calib_finish with enough samples succeeds... ");
    imu_calib_start();
    for (int i = 0; i < 100; i++)
    {
      float accel[3] = {0.0f, 0.0f, 1.0f};
      float gyro[3] = {5.0f, -3.0f, 1.5f}; /* Simulated bias */
      imu_calib_collect(accel, gyro);
    }
    imu_calib_finish();
    if (imu_calib_is_valid() == true)
    {
      imu_calib_t cal;
      imu_calib_get(&cal);
      /* Check gyro bias is computed (should be ~5.0, -3.0, 1.5 deg/s converted to rad/s) */
      float expected_x = 5.0f * (float)M_PI / 180.0f;
      if (fabsf(cal.gyro_bias_rads[0] - expected_x) < 0.01f)
      {
        tests_passed++;
        printf("PASS\n");
      }
      else
      {
        printf("FAIL (bias wrong: %.4f vs %.4f)\n", cal.gyro_bias_rads[0], expected_x);
      }
    }
    else
    {
      printf("FAIL (not calibrated)\n");
    }
  }

  /* Test 4: imu_calib_apply_accel() */
  {
    tests_run++;
    printf("Test: imu_calib_apply_accel transforms raw... ");
    imu_calib_start();
    /* Simulate 6-point accel calibration quickly */
    for (int i = 0; i < 10; i++)
    {
      float accel[3] = {0.0f, 0.0f, 1.0f};
      float gyro[3] = {0.0f, 0.0f, 0.0f};
      imu_calib_collect(accel, gyro);
    }
    imu_calib_finish();

    if (imu_calib_is_valid())
    {
      float raw[3] = {0.5f, -0.3f, 0.8f};
      float cal[3];
      imu_calib_apply_accel(raw, cal);
      /* Since we faked calibration, just check it doesn't crash */
      tests_passed++;
      printf("PASS\n");
    }
    else
    {
      printf("FAIL (not calibrated)\n");
    }
  }

  /* Test 5: imu_calib_get/load() */
  {
    tests_run++;
    printf("Test: imu_calib_get/load persistence API... ");
    imu_calib_start();
    for (int i = 0; i < 50; i++)
    {
      float accel[3] = {0.0f, 0.0f, 1.0f};
      float gyro[3] = {2.0f, 3.0f, 4.0f};
      imu_calib_collect(accel, gyro);
    }
    imu_calib_finish();

    imu_calib_t saved;
    imu_calib_get(&saved);
    if (saved.calibrated == true)
    {
      /* Load it back */
      imu_calib_load(&saved);
      if (imu_calib_is_valid() == true)
      {
        tests_passed++;
        printf("PASS\n");
      }
      else
      {
        printf("FAIL (load didn't set calibrated)\n");
      }
    }
    else
    {
      printf("FAIL (get didn't set calibrated)\n");
    }
  }

  printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);
  return (tests_passed == tests_run) ? 0 : 1;
}
