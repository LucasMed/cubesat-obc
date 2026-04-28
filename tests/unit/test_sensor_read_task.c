/**
 * @file test_sensor_read_task.c
 * @brief PR-7 / PR-14 gate: Sensor Read Task DLA-migration and EKF fusion checks.
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
 *   8.  (T-SRF-08) imu_ekf_valid is set after a successful IMU read
 *   9.  (T-SRF-09) EKF attitude, bias, uncertainty written to DLA are finite
 *   10. (T-SRF-10) imu_ekf_valid stays false when mpu6050_read_raw fails
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

/* Magnetometer stub — no-op init, fixed {25,0,42} read.
 * These strong symbols prevent drivers_lib from being needed. */
#include "drivers/mag/hmc5883l.h"

int hmc5883l_init(void)
{
  return 0;
}

int hmc5883l_read(float field_uT[3])
{
  field_uT[0] = 25.0f;
  field_uT[1] = 0.0f;
  field_uT[2] = 42.0f;
  return 0;
}

/* BH1750 stub — returns false so lux_available is not set */
// NOLINTNEXTLINE(readability-non-const-parameter)
bool bh1750_read(float *lux)
{
  (void)lux;
  return false;
}

/* INA219 stub — returns false so power_available is not set */
#include "ina219.h"
// NOLINTNEXTLINE(readability-non-const-parameter)
bool ina219_read_power(ina219_data_t *data)
{
  (void)data;
  return false;
}

/* IMU calibration stubs — no-op for host tests */
#include "drivers/imu/imu_calib.h"
void imu_calib_collect(const float accel_g[3], const float gyro_dps[3])
{
  (void)accel_g;
  (void)gyro_dps;
}

void imu_calib_apply_accel(const float accel_raw[3], float accel_cal[3])
{
  accel_cal[0] = accel_raw[0];
  accel_cal[1] = accel_raw[1];
  accel_cal[2] = accel_raw[2];
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
/* Test 8 (T-SRF-08): imu_ekf_valid set after successful IMU read    */
/* ------------------------------------------------------------------ */

static void test_ekf_valid_flag_set(void)
{
  reset();
  data_layer_set_sensor_avail(true, false);
  vSensorReadTask_Step();

  dl_snapshot_t snap = {0};
  data_layer_read(&snap);
  CHECK(snap.state.imu_ekf_valid, "imu_ekf_valid must be true after successful IMU read");
  printf("test_ekf_valid_flag_set: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 9 (T-SRF-09): EKF outputs stored in DLA are finite values    */
/* ------------------------------------------------------------------ */

static void test_ekf_outputs_finite(void)
{
  reset();
  data_layer_set_sensor_avail(true, false);
  vSensorReadTask_Step();

  dl_snapshot_t snap = {0};
  data_layer_read(&snap);
  CHECK(isfinite(snap.state.attitude[0]), "EKF attitude[0] must be finite");
  CHECK(isfinite(snap.state.attitude[1]), "EKF attitude[1] must be finite");
  CHECK(isfinite(snap.state.attitude[2]), "EKF attitude[2] must be finite");
  CHECK(isfinite(snap.state.gyro_bias[0]), "EKF gyro_bias[0] must be finite");
  CHECK(isfinite(snap.state.gyro_bias[1]), "EKF gyro_bias[1] must be finite");
  CHECK(isfinite(snap.state.gyro_bias[2]), "EKF gyro_bias[2] must be finite");
  CHECK(isfinite(snap.state.att_uncertainty[0]), "EKF att_uncertainty[0] must be finite");
  CHECK(isfinite(snap.state.att_uncertainty[1]), "EKF att_uncertainty[1] must be finite");
  CHECK(isfinite(snap.state.att_uncertainty[2]), "EKF att_uncertainty[2] must be finite");
  printf("test_ekf_outputs_finite: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 10 (T-SRF-10): imu_ekf_valid stays false on driver failure   */
/* ------------------------------------------------------------------ */

static void test_ekf_valid_not_set_on_failure(void)
{
  reset();
  data_layer_set_sensor_avail(true, false);
  s_imu_ret = -1; /* force driver failure */
  vSensorReadTask_Step();

  dl_snapshot_t snap = {0};
  data_layer_read(&snap);
  CHECK(!snap.state.imu_ekf_valid,
        "imu_ekf_valid must remain false when mpu6050_read_raw returns -1");
  printf("test_ekf_valid_not_set_on_failure: OK\n");
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
  test_ekf_valid_flag_set();
  test_ekf_outputs_finite();
  test_ekf_valid_not_set_on_failure();

  if (g_failures == 0)
  {
    printf("All Sensor Read Task DLA checks PASSED.\n");
    return 0;
  }
  printf("%d Sensor Read Task check(s) FAILED.\n", g_failures);
  return 1;
}
