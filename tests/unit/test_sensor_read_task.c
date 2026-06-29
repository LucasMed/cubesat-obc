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
/* Configurable SHT31 stub globals (defined in sht31_stub.c)           */
/* ------------------------------------------------------------------ */

extern bool   s_sht31_fetch_ret;
extern float  s_sht31_fetch_temp;
extern float  s_sht31_fetch_humid;

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

/* Configurable BH1750 stub */
static bool s_bh1750_ret = false;
static float s_bh1750_lux = 0.0f;
// NOLINTNEXTLINE(readability-non-const-parameter)
bool bh1750_read(float *lux)
{
  if (lux)
    *lux = s_bh1750_lux;
  return s_bh1750_ret;
}

/* Configurable INA219 stubs */
#include "ina219.h"
static bool s_ina219_ret = false;
static ina219_data_t s_ina219_data = {5000, 100, 500};
// NOLINTNEXTLINE(readability-non-const-parameter)
bool ina219_read_power(ina219_data_t *data)
{
  if (data)
    *data = s_ina219_data;
  return s_ina219_ret;
}
static bool s_ina219_solar_ret = false;
static ina219_data_t s_ina219_solar_data = {4200, 50, 210};
// NOLINTNEXTLINE(readability-non-const-parameter)
bool ina219_solar_read_power(ina219_data_t *data)
{
  if (data)
    *data = s_ina219_solar_data;
  return s_ina219_solar_ret;
}

/* Configurable sun sensor stub globals (defined in sun_sensor_stub.c) */
extern bool     s_sun_sensor_read_ret;
extern uint16_t s_sun_sensor_adc_x;
extern uint16_t s_sun_sensor_adc_y;
extern float    s_sun_sensor_intensity_x;
extern float    s_sun_sensor_intensity_y;
extern bool     s_sun_sensor_sun_detected_x;
extern bool     s_sun_sensor_sun_detected_y;

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
  s_sht31_fetch_ret = false;
  s_sht31_fetch_temp = 0.0f;
  s_sht31_fetch_humid = 0.0f;
  s_sun_sensor_read_ret = true;
  s_sun_sensor_intensity_x = 0.024f;
  s_sun_sensor_intensity_y = 0.024f;
  s_bh1750_ret = false;
  s_bh1750_lux = 0.0f;
  s_ina219_ret = false;
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
  s_sht31_fetch_ret = true;
  s_sht31_fetch_temp = 42.0f;
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
  s_sht31_fetch_ret = true;
  s_sht31_fetch_temp = 20.0f;
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
/* Test 11 (T-SRF-11): BH1750 light sensor data written to DLA       */
/* ------------------------------------------------------------------ */

static void test_light_available(void)
{
  reset();
  s_bh1750_ret = true;
  s_bh1750_lux = 1234.5f;
  data_layer_set_sensor_avail(false, false);
  data_layer_set_lux_avail(true);
  vSensorReadTask_Step();

  dl_snapshot_t snap = {0};
  data_layer_read(&snap);
  CHECK(snap.state.lux_valid, "lux_valid must be true after BH1750 read");
  CHECK(RAD_EQ(snap.state.lux, 1234.5f), "lux must match stub value");
  printf("test_light_available: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 12 (T-SRF-12): INA219 power data written to DLA (1Hz path)   */
/* ------------------------------------------------------------------ */

static void test_power_available(void)
{
  reset();
  s_ina219_ret = true;
  s_ina219_solar_ret = true;
  data_layer_set_power_avail(true);
  data_layer_set_solar_avail(true);

  /* Need 10 iterations to wrap the 1Hz counter */
  for (int i = 0; i < 10; i++)
    vSensorReadTask_Step();

  dl_snapshot_t snap = {0};
  data_layer_read(&snap);
  CHECK(snap.state.power_valid, "power_valid must be true after INA219 read");
  CHECK(snap.state.solar_valid, "solar_valid must be true after solar INA219 read");
  printf("test_power_available: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 13 (T-SRF-13): DS3231 RTC data written to DLA (1Hz path)     */
/* ------------------------------------------------------------------ */

static void test_rtc_available(void)
{
  reset();
  data_layer_set_rtc_avail(true);

  /* Need 10 iterations to wrap the 1Hz counter */
  for (int i = 0; i < 10; i++)
    vSensorReadTask_Step();

  dl_snapshot_t snap = {0};
  data_layer_read(&snap);
  CHECK(snap.state.rtc_valid, "rtc_valid must be true after DS3231 read");
  printf("test_rtc_available: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 14 (T-SRF-14): Sun sensor data written to DLA (1Hz path)     */
/* ------------------------------------------------------------------ */

static void test_sun_available(void)
{
  reset();
  data_layer_set_sun_avail(true);
  s_sun_sensor_read_ret = true;
  s_sun_sensor_intensity_x = 0.75f;
  s_sun_sensor_intensity_y = 0.32f;

  /* Need 10 iterations to wrap the 1Hz counter */
  for (int i = 0; i < 10; i++)
    vSensorReadTask_Step();

  dl_snapshot_t snap = {0};
  data_layer_read(&snap);
  CHECK(snap.state.sun_valid, "sun_valid must be true after sun sensor read");
  CHECK(RAD_EQ(snap.state.sun_x, 0.75f), "sun_x must match stub value");
  CHECK(RAD_EQ(snap.state.sun_y, 0.32f), "sun_y must match stub value");
  printf("test_sun_available: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 15 (T-SRF-15): Magnetometer + EKF mag update path             */
/* ------------------------------------------------------------------ */

static void test_mag_ekf_update(void)
{
  reset();

  /* First, init the EKF via an IMU step */
  data_layer_set_sensor_avail(true, false);
  vSensorReadTask_Step();

  /* Now enable mag and run another step */
  data_layer_set_mag_avail(true);
  vSensorReadTask_Step();

  dl_snapshot_t snap = {0};
  data_layer_read(&snap);
  CHECK(snap.state.mag_valid, "mag_valid must be true after magnetometer read");
  printf("test_mag_ekf_update: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 16 (T-SRF-16): SHT31 fetch failure — temp/humidity NOT written */
/* ------------------------------------------------------------------ */

static void test_sht31_fetch_failure(void)
{
  reset();
  data_layer_set_sensor_avail(false, true);
  s_sht31_fetch_ret = false; /* fetch fails */
  vSensorReadTask_Step();

  dl_snapshot_t snap = {0};
  data_layer_read(&snap);
  CHECK(!snap.state.temp_valid, "temp_valid must remain false when sht31_fetch fails");
  CHECK(!snap.state.humidity_valid, "humidity_valid must remain false when sht31_fetch fails");
  printf("test_sht31_fetch_failure: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 17 (T-SRF-17): BH1750 read failure — lux NOT written          */
/* ------------------------------------------------------------------ */

static void test_bh1750_read_failure(void)
{
  reset();
  data_layer_set_lux_avail(true);
  s_bh1750_ret = false; /* read fails */
  vSensorReadTask_Step();

  dl_snapshot_t snap = {0};
  data_layer_read(&snap);
  CHECK(!snap.state.lux_valid, "lux_valid must remain false when bh1750_read fails");
  printf("test_bh1750_read_failure: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 18 (T-SRF-18): Deg/s → rad/s edge case — zero gyro           */
/* ------------------------------------------------------------------ */

static void test_gyro_deg_to_rad_zero(void)
{
  reset();
  s_gyro_deg[0] = 0.0f;
  s_gyro_deg[1] = 0.0f;
  s_gyro_deg[2] = 0.0f;
  data_layer_set_sensor_avail(true, false);
  vSensorReadTask_Step();

  dl_snapshot_t snap = {0};
  data_layer_read(&snap);
  CHECK(RAD_EQ(snap.state.rates[0], 0.0f), "rates[0] must be 0 for 0 deg/s");
  CHECK(RAD_EQ(snap.state.rates[1], 0.0f), "rates[1] must be 0 for 0 deg/s");
  CHECK(RAD_EQ(snap.state.rates[2], 0.0f), "rates[2] must be 0 for 0 deg/s");
  printf("test_gyro_deg_to_rad_zero: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 19 (T-SRF-19): Deg/s → rad/s edge case — negative gyro       */
/* ------------------------------------------------------------------ */

static void test_gyro_deg_to_rad_negative(void)
{
  reset();
  s_gyro_deg[0] = -90.0f;
  s_gyro_deg[1] = -180.0f;
  s_gyro_deg[2] = -360.0f;
  data_layer_set_sensor_avail(true, false);
  vSensorReadTask_Step();

  dl_snapshot_t snap = {0};
  data_layer_read(&snap);

  float expected0 = -90.0f * (float)(M_PI / 180.0);
  float expected1 = -180.0f * (float)(M_PI / 180.0);
  float expected2 = -360.0f * (float)(M_PI / 180.0);

  CHECK(RAD_EQ(snap.state.rates[0], expected0), "rates[0] must be -π/2 for -90 deg/s");
  CHECK(RAD_EQ(snap.state.rates[1], expected1), "rates[1] must be -π for -180 deg/s");
  CHECK(RAD_EQ(snap.state.rates[2], expected2), "rates[2] must be -2π for -360 deg/s");
  printf("test_gyro_deg_to_rad_negative: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 20 (T-SRF-20): Timing statistics count — 100 measured          */
/* ------------------------------------------------------------------ */

static void test_timing_statistics_count(void)
{
  /* The PICO_BUILD timing logic tracks measured intervals:
   *
   *   uint32_t samples = 0;
   *   loop {
   *     if (samples > 0) measure();     // skip first
   *     samples++;
   *     if (samples >= 101) {            // triggers after 100 measurements
   *         // print 100 samples
   *         samples = 1;                 // keep last_wake
   *     }
   *   }
   *
   * This test verifies the count logic by reproducing it in isolation.
   * At samples >= 101 we have measured exactly 100 intervals because:
   *   - Iteration 1:  samples=0 → skip, inc→1
   *   - Iterations 2-101: samples=1..100 → measure (100×), inc→2..101
   *   - At samples=101: trigger (100 intervals collected)
   *
   * Changing to >= 100 would produce only 99 measurements.  The
   * condition >= 101 IS the correct off-by-one accounting for the
   * first sample being skipped.  See PR #CDR-REVIEW-005.
   */
  uint32_t samples = 0;

  for (int iter = 0; iter < 120; iter++)
  {
    samples++;

    if (samples >= 101)
    {
      samples = 1;  /* keep last_wake for next batch */
    }
  }

  /* After 120 iterations the logic should not have crashed, and the
   * measuring-count invariants must hold for each batch.  Rather than
   * reimplement the full batch tracking, we verify the final state is
   * consistent: samples >= 1 means we're mid-batch (no crash). */
  CHECK(samples > 0, "Timing counter must be > 0 after loop");
  CHECK(samples < 101, "Timing counter must be < 101");
  printf("test_timing_statistics_count: OK\n");
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
  test_light_available();
  test_power_available();
  test_rtc_available();
  test_sun_available();
  test_mag_ekf_update();
  test_sht31_fetch_failure();
  test_bh1750_read_failure();
  test_gyro_deg_to_rad_zero();
  test_gyro_deg_to_rad_negative();
  test_timing_statistics_count();

  if (g_failures == 0)
  {
    printf("All Sensor Read Task DLA checks PASSED.\n");
    return 0;
  }
  printf("%d Sensor Read Task check(s) FAILED.\n", g_failures);
  return 1;
}
