/**
 * @file test_sensor_read_task.c
 * @brief PR-7 gate: Sensor Read Task DLA-migration correctness checks.
 *
 * Driver functions (mpu6050_read_raw, temperature_read) are replaced by
 * strong-symbol stubs defined here, so no hardware or drivers_lib needed.
 *
 * Tests:
 *   1.  When imu_available == false, IMU data is NOT written to DLA
 *   2.  When imu_available == true, IMU data IS written to DLA
 *   3.  Gyroscope output is converted deg/s → rad/s (SPEC-2-DLA §2.4)
 *   4.  When temp_available == false, temperature is NOT written to DLA
 *   5.  When temp_available == true, temperature IS written to DLA
 *   6.  Both imu_available and temp_available == true: both written
 *   7.  mpu6050_read_raw failure (returns -1): imu_valid stays false
 */

#include "../../include/data_layer.h"
#include "../../include/sensor_read_task.h"
#include "../../include/system_state.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

/* ------------------------------------------------------------------ */
/* Driver stubs (strong symbols override the absent drivers_lib)       */
/* ------------------------------------------------------------------ */

static float s_accel[3] = {0.0f, 0.0f, 1.0f}; /* 1 G down */
static float s_gyro_deg[3] = {90.0f, 45.0f, 180.0f};
static int s_imu_ret = 0; /* 0 = success, -1 = failure */

int mpu6050_read_raw(float accel[3], float gyro[3])
{
  if (s_imu_ret != 0)
    return s_imu_ret;
  accel[0] = s_accel[0];
  accel[1] = s_accel[1];
  accel[2] = s_accel[2];
  gyro[0] = s_gyro_deg[0];
  gyro[1] = s_gyro_deg[1];
  gyro[2] = s_gyro_deg[2];
  return 0;
}

static float s_temp_val = 36.5f;

float temperature_read(void)
{
  return s_temp_val;
}

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static int g_failures = 0;

#define CHECK(cond, msg)                                                                           \
  do                                                                                               \
  {                                                                                                \
    if (!(cond))                                                                                   \
    {                                                                                              \
      printf("FAIL [%s:%d]: %s\n", __FILE__, __LINE__, (msg));                                     \
      g_failures++;                                                                                \
    }                                                                                              \
  } while (0)

#define RAD_EQ(a, b) (fabsf((a) - (b)) < 1e-5f)

static void reset(void)
{
  data_layer_init();
  /* Default: both sensors unavailable */
  data_layer_set_sensor_avail(false, false);
  s_imu_ret = 0;
  s_accel[0] = 0.0f;
  s_accel[1] = 0.0f;
  s_accel[2] = 1.0f;
  s_gyro_deg[0] = 90.0f;
  s_gyro_deg[1] = 45.0f;
  s_gyro_deg[2] = 180.0f;
  s_temp_val = 36.5f;
}

/* ------------------------------------------------------------------ */
/* Test 1: IMU not written when imu_available == false                 */
/* ------------------------------------------------------------------ */

static void test_imu_unavailable(void)
{
  reset();
  data_layer_set_sensor_avail(false, false);
  vSensorReadTask_Step();

  dl_snapshot_t snap = {0};
  data_layer_read(&snap);
  CHECK(!snap.state.imu_valid, "imu_valid must remain false when imu_available is false");
  printf("test_imu_unavailable: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 2: IMU IS written when imu_available == true                   */
/* ------------------------------------------------------------------ */

static void test_imu_available(void)
{
  reset();
  data_layer_set_sensor_avail(true, false);
  vSensorReadTask_Step();

  dl_snapshot_t snap = {0};
  data_layer_read(&snap);
  CHECK(snap.state.imu_valid, "imu_valid must be true after successful read");
  printf("test_imu_available: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 3: gyroscope converted deg/s → rad/s                           */
/* ------------------------------------------------------------------ */

static void test_gyro_deg_to_rad_conversion(void)
{
  reset();
  s_gyro_deg[0] = 90.0f;  /* expected: π/2 rad/s */
  s_gyro_deg[1] = 45.0f;  /* expected: π/4 rad/s */
  s_gyro_deg[2] = 180.0f; /* expected: π   rad/s */
  data_layer_set_sensor_avail(true, false);
  vSensorReadTask_Step();

  dl_snapshot_t snap = {0};
  data_layer_read(&snap);

  float expected0 = 90.0f * (float)(M_PI / 180.0);
  float expected1 = 45.0f * (float)(M_PI / 180.0);
  float expected2 = 180.0f * (float)(M_PI / 180.0);

  CHECK(RAD_EQ(snap.state.rates[0], expected0), "rates[0] must equal 90 deg/s converted to rad/s");
  CHECK(RAD_EQ(snap.state.rates[1], expected1), "rates[1] must equal 45 deg/s converted to rad/s");
  CHECK(RAD_EQ(snap.state.rates[2], expected2), "rates[2] must equal 180 deg/s converted to rad/s");
  printf("test_gyro_deg_to_rad_conversion: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 4: temperature not written when temp_available == false        */
/* ------------------------------------------------------------------ */

static void test_temp_unavailable(void)
{
  reset();
  data_layer_set_sensor_avail(false, false);
  vSensorReadTask_Step();

  dl_snapshot_t snap = {0};
  data_layer_read(&snap);
  CHECK(!snap.state.temp_valid, "temp_valid must remain false when temp_available is false");
  printf("test_temp_unavailable: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 5: temperature IS written when temp_available == true          */
/* ------------------------------------------------------------------ */

static void test_temp_available(void)
{
  reset();
  s_temp_val = 42.0f;
  data_layer_set_sensor_avail(false, true);
  vSensorReadTask_Step();

  dl_snapshot_t snap = {0};
  data_layer_read(&snap);
  CHECK(snap.state.temp_valid, "temp_valid must be true after successful read");
  CHECK(RAD_EQ(snap.state.temp, 42.0f), "temperature must match injected stub value");
  printf("test_temp_available: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 6: both sensors available and updated in one step              */
/* ------------------------------------------------------------------ */

static void test_both_sensors(void)
{
  reset();
  s_temp_val = 20.0f;
  data_layer_set_sensor_avail(true, true);
  vSensorReadTask_Step();

  dl_snapshot_t snap = {0};
  data_layer_read(&snap);
  CHECK(snap.state.imu_valid, "imu_valid must be true");
  CHECK(snap.state.temp_valid, "temp_valid must be true");
  CHECK(RAD_EQ(snap.state.temp, 20.0f), "temperature must be 20.0");
  printf("test_both_sensors: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 7: mpu6050_read_raw failure leaves imu_valid false             */
/* ------------------------------------------------------------------ */

static void test_imu_read_failure(void)
{
  reset();
  data_layer_set_sensor_avail(true, false);
  /* Force a successful first tick to set imu_valid */
  vSensorReadTask_Step();

  /* Reset validity then force driver failure */
  data_layer_init();
  data_layer_set_sensor_avail(true, false);
  s_imu_ret = -1; /* Driver will return error */
  vSensorReadTask_Step();

  dl_snapshot_t snap = {0};
  data_layer_read(&snap);
  CHECK(!snap.state.imu_valid, "imu_valid must remain false when mpu6050_read_raw returns -1");
  printf("test_imu_read_failure: OK\n");
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(void)
{
  test_imu_unavailable();
  test_imu_available();
  test_gyro_deg_to_rad_conversion();
  test_temp_unavailable();
  test_temp_available();
  test_both_sensors();
  test_imu_read_failure();

  if (g_failures == 0)
  {
    printf("All Sensor Read Task DLA checks PASSED.\n");
    return 0;
  }
  printf("%d Sensor Read Task check(s) FAILED.\n", g_failures);
  return 1;
}
