/**
 * @file mag_calib.c
 * @brief Magnetometer hard/soft iron calibration implementation.
 *
 * Simple hard-iron calibration: collects min/max during rotation,
 * then computes offsets to center the measurement sphere.
 *
 * Soft-iron (ellipsoid fitting) is NOT implemented here — that
 * requires matrix inversion and is left for advanced users.
 */
#include "mag_calib.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* Internal state */
static mag_calib_t g_calib = {0};
static float s_min[3] = {0};
static float s_max[3] = {0};
static bool s_collecting = false;
static uint32_t s_samples = 0;

void mag_calib_start(void)
{
  g_calib.calibrated = false;
  s_samples = 0;

  /* Initialize min/max with extreme values */
  for (int i = 0; i < 3; i++)
  {
    s_min[i] = INFINITY;
    s_max[i] = -INFINITY;
  }

  s_collecting = true;
  printf("[mag_calib] Started collection — rotate CubeSat in all axes\n");
}

void mag_calib_collect(const float mag_raw[3])
{
  if (!s_collecting)
  {
    return;
  }

  for (int i = 0; i < 3; i++)
  {
    if (mag_raw[i] < s_min[i])
      s_min[i] = mag_raw[i];
    if (mag_raw[i] > s_max[i])
      s_max[i] = mag_raw[i];
  }
  s_samples++;

  if ((s_samples % 100) == 0)
  {
    printf("[mag_calib] Collected %lu samples, min=[%.1f,%.1f,%.1f] max=[%.1f,%.1f,%.1f]\n",
           (unsigned long)s_samples, s_min[0], s_min[1], s_min[2], s_max[0], s_max[1], s_max[2]);
  }
}

void mag_calib_finish(void)
{
  s_collecting = false;

  if (s_samples < 50)
  {
    printf("[mag_calib] ERROR: too few samples (%lu), need at least 50\n",
           (unsigned long)s_samples);
    return;
  }

  /* Compute hard-iron offsets: center of the min/max box */
  for (int i = 0; i < 3; i++)
  {
    g_calib.offset[i] = (s_max[i] + s_min[i]) / 2.0f;
    g_calib.scale[i] = 1.0f; /* Simplified: no soft-iron */
  }

  g_calib.calibrated = true;

  printf("[mag_calib] Calibration complete (%lu samples):\n", (unsigned long)s_samples);
  printf("  Offsets: x=%.2f y=%.2f z=%.2f µT\n", g_calib.offset[0], g_calib.offset[1],
         g_calib.offset[2]);
  printf("  Now apply with mag_calib_apply()\n");
}

void mag_calib_apply(const float mag_raw[3], float mag_cal[3])
{
  if (!g_calib.calibrated)
  {
    /* No calibration: pass through raw values */
    (void)memcpy(mag_cal, mag_raw, 3 * sizeof(float));
    return;
  }

  for (int i = 0; i < 3; i++)
  {
    mag_cal[i] = mag_raw[i] - g_calib.offset[i];
  }
}

bool mag_calib_is_valid(void)
{
  return g_calib.calibrated;
}

void mag_calib_get(mag_calib_t *out)
{
  if (out)
    *out = g_calib;
}

void mag_calib_load(const mag_calib_t *in)
{
  if (in && in->calibrated)
  {
    g_calib = *in;
    printf("[mag_calib] Loaded calibration: x=%.2f y=%.2f z=%.2f µT\n", g_calib.offset[0],
           g_calib.offset[1], g_calib.offset[2]);
  }
}
