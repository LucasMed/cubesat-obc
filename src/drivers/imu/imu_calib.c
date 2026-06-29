/**
 * @file imu_calib.c
 * @brief MPU-6050 accelerometer + gyroscope calibration implementation.
 *
 * Follows mag_calib.c pattern: start → collect → finish → apply.
 * Gyroscope: static bias averaging (stationary).
 * Accelerometer: 6-point gravity calibration (GROUND ONLY).
 */

#include "drivers/imu/imu_calib.h"

#include "drivers/imu/mpu6050.h"
#include "ekf.h"
#include "w25q64.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* Internal state */
static imu_calib_t g_calib = {0};
static bool s_collecting = false;
static uint32_t s_gyro_count = 0;
static float s_gyro_sum[3] = {0};
static float s_accel_min[3] = {0};
static float s_accel_max[3] = {0};

void imu_calib_start(void)
{
  g_calib.calibrated = false;
  s_gyro_count = 0;
  memset(&s_gyro_sum, 0, sizeof(s_gyro_sum));

  for (int i = 0; i < 3; i++)
  {
    s_accel_min[i] = INFINITY;
    s_accel_max[i] = -INFINITY;
  }

  s_collecting = true;
  printf(
      "[imu_calib] Started collection — keep stationary for gyro, then 6 orientations for accel\n");
}

void imu_calib_collect(const float accel_g[3], const float gyro_dps[3])
{
  if (!s_collecting)
  {
    return;
  }

  /* Gyro: accumulate for bias averaging */
  for (int i = 0; i < 3; i++)
  {
    s_gyro_sum[i] += gyro_dps[i];
  }
  s_gyro_count++;

  /* Accel: track min/max for 6-point calibration */
  for (int i = 0; i < 3; i++)
  {
    if (accel_g[i] < s_accel_min[i])
    {
      s_accel_min[i] = accel_g[i];
    }
    if (accel_g[i] > s_accel_max[i])
    {
      s_accel_max[i] = accel_g[i];
    }
  }

  if ((s_gyro_count % 100) == 0)
  {
    printf("[imu_calib] Collected %lu gyro samples\n", (unsigned long)s_gyro_count);
  }
}

void imu_calib_finish(void)
{
  s_collecting = false;

  if (s_gyro_count < 50)
  {
    printf("[imu_calib] ERROR: too few gyro samples (%lu), need at least 50\n",
           (unsigned long)s_gyro_count);
    return;
  }

  /* Compute gyro bias (average) in deg/s */
  float gyro_bias_dps[3];
  for (int i = 0; i < 3; i++)
  {
    gyro_bias_dps[i] = s_gyro_sum[i] / (float)s_gyro_count;
    g_calib.gyro_bias_rads[i] = gyro_bias_dps[i] * (float)(M_PI / 180.0);
  }

  /* Compute gyro offset for hardware registers (int16_t, scale: 32.8 LSB/(°/s)) */
  for (int i = 0; i < 3; i++)
  {
    float offset_lsb = -gyro_bias_dps[i] * 32.8f;  // Negative because register subtracts offset
    if (offset_lsb > 32767.0f)
    {
      offset_lsb = 32767.0f;
    }
    if (offset_lsb < -32768.0f)
    {
      offset_lsb = -32768.0f;
    }
    g_calib.gyro_offset_raw[i] = (int16_t)offset_lsb;
  }

  /* Compute accel offsets (6-point: offset = (max + min) / 2, scale = 2g / (max - min)) */
  for (int i = 0; i < 3; i++)
  {
    g_calib.accel_offset[i] = (s_accel_max[i] + s_accel_min[i]) / 2.0f;
    float range = s_accel_max[i] - s_accel_min[i];
    if (range > 0.0f)
    {
      g_calib.accel_scale[i] = 2.0f / range;  // Expect 2g total range
    }
    else
    {
      printf("[imu_calib] WARNING: axis %d has zero range (min=%f, max=%f), "
             "skipping calibration for this axis\n",
             i, (double)s_accel_min[i], (double)s_accel_max[i]);
      g_calib.accel_scale[i] = 1.0f;
      g_calib.accel_offset[i] = 0.0f;
    }
  }

  g_calib.calibrated = true;

  /* Write gyro offsets to hardware registers */
  if (mpu6050_write_gyro_offset(g_calib.gyro_offset_raw) == 0)
  {
    printf("[imu_calib] Gyro offsets written to HW registers\n");
  }
  else
  {
    printf("[imu_calib] WARNING: failed to write gyro offsets to HW\n");
  }

  /* Seed EKF bias states if EKF is initialized */
  /* NOTE: EKF seeding is done in sensor_read_task.c after calling
   * imu_calib_finish(). This avoids accessing extern variables
   * from drivers/ and allows proper integration. */
  // __attribute__((weak)) extern ekf_t s_ekf;  // From sensor_read_task.c
  // __attribute__((weak)) extern bool s_ekf_initialised;
  // if (s_ekf_initialised)
  // {
  //   for (int i = 0; i < 3; i++)
  //   {
  //     s_ekf.x[4 + i] = g_calib.gyro_bias_rads[i];  // bx, by, bz states
  //   }
  //   printf("[imu_calib] EKF bias states seeded with gyro bias\n");
  // }

  printf("[imu_calib] Calibration complete (%lu samples):\n", (unsigned long)s_gyro_count);
  printf("  Gyro bias (rad/s): x=%.4f y=%.4f z=%.4f\n", g_calib.gyro_bias_rads[0],
         g_calib.gyro_bias_rads[1], g_calib.gyro_bias_rads[2]);
  printf("  Accel offsets (g): x=%.3f y=%.3f z=%.3f\n", g_calib.accel_offset[0],
         g_calib.accel_offset[1], g_calib.accel_offset[2]);
  printf("  Accel scales: x=%.3f y=%.3f z=%.3f\n", g_calib.accel_scale[0], g_calib.accel_scale[1],
         g_calib.accel_scale[2]);
}

void imu_calib_apply_accel(const float accel_raw[3], float accel_cal[3])
{
  if (!g_calib.calibrated)
  {
    /* No calibration: pass through raw values */
    (void)memcpy(accel_cal, accel_raw, 3 * sizeof(float));
    return;
  }

  for (int i = 0; i < 3; i++)
  {
    accel_cal[i] = (accel_raw[i] - g_calib.accel_offset[i]) * g_calib.accel_scale[i];
  }
}

bool imu_calib_is_valid(void)
{
  return g_calib.calibrated;
}

void imu_calib_get(imu_calib_t *out)
{
  if (out)
  {
    *out = g_calib;
  }
}

void imu_calib_load(const imu_calib_t *in)
{
  if (in && in->calibrated)
  {
    g_calib = *in;

    /* Write gyro offsets to hardware registers */
    if (mpu6050_write_gyro_offset(g_calib.gyro_offset_raw) == 0)
    {
      printf("[imu_calib] Loaded calibration: gyro offsets written to HW\n");
    }

    /* Seed EKF bias states if initialized */
    __attribute__((weak)) extern ekf_t s_ekf;
    __attribute__((weak)) extern bool s_ekf_initialised;
    if (s_ekf_initialised)
    {
      for (int i = 0; i < 3; i++)
      {
        s_ekf.x[4 + i] = g_calib.gyro_bias_rads[i];
      }
    }

    printf("[imu_calib] Loaded calibration: gyro_bias=%.4f,%.4f,%.4f rad/s\n",
           g_calib.gyro_bias_rads[0], g_calib.gyro_bias_rads[1], g_calib.gyro_bias_rads[2]);
  }
}

void imu_calib_save_to_flash(void)
{
  w25q64_status_t status = w25q64_write_imu_calib(&g_calib);
  if (status == W25Q64_OK)
  {
    printf("[imu_calib] Calibration saved to W25Q64 flash\n");
  }
  else
  {
    printf("[imu_calib] ERROR: failed to save calibration to flash (%d)\n", (int)status);
  }
}

void imu_calib_load_from_flash(void)
{
  imu_calib_t loaded;
  memset(&loaded, 0, sizeof(loaded));

  w25q64_status_t status = w25q64_read_imu_calib(&loaded);
  if (status == W25Q64_OK)
  {
    imu_calib_load(&loaded);
    printf("[imu_calib] Calibration loaded from W25Q64 flash\n");
  }
  else
  {
    printf("[imu_calib] No valid calibration in flash (status=%d)\n", (int)status);
  }
}
