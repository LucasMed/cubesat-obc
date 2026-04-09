/* test_hmc5883l.c — Unit tests for the HMC5883L driver stub + DLA integration (PR-18)
 *
 * T-MAG-01: hmc5883l_init()         — returns 0; no crash
 * T-MAG-02: hmc5883l_read()         — returns {25.0, 0.0, 42.0} µT on host
 * T-MAG-03: data_layer_write_mag()  — sets mag_valid=true; stores field values
 * T-MAG-04: sensor_read_task step   — mag_available=true → mag data propagated to DLA
 *
 * Compiles the real host stub (hmc5883l.c) and data_layer.c so that
 * T-MAG-01/02 exercise the actual stub implementation rather than a mock.
 * mpu6050_read_raw and temperature_read are replaced by minimal stubs so that
 * sensor_read_task.c compiles cleanly in T-MAG-04.
 */

#include "data_layer.h"
#include "drivers/mag/hmc5883l.h"
#include "sensor_read_task.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>

/* ---- FreeRTOS stubs required by sensor_read_task.c headers ------------ */
/* vSensorReadTask_Step() does not call FreeRTOS scheduling functions, but
 * including sensor_read_task.h pulls in FreeRTOS.h / task.h.  The host
 * stub header provides these as no-ops; no additional definitions needed. */
#include "FreeRTOS.h"
#include "task.h"

/* ---- IMU / temp driver stubs (satisfy sensor_read_task.c link) --------- */
#include "drivers/imu/mpu6050.h"
#include "drivers/temperature.h"

/* Return failure by default so only the mag path is exercised in T-MAG-04 */
static int s_imu_ret = -1;

int mpu6050_read_raw(float accel[3], float gyro[3])
{
  (void)accel;
  (void)gyro;
  return s_imu_ret;
}

float temperature_read(void)
{
  return 25.0f;
}

/* ---- BH1750 stub (satisfy sensor_read_task.c link) ---------------------- */
bool bh1750_read(float *lux)
{
  (void)lux;
  return false;
}

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

#define FPEQ(a, b) (fabsf((a) - (b)) < 1e-5f)

/* ========================================================================
 * T-MAG-01  hmc5883l_init() returns 0 and does not crash
 * ======================================================================== */
static void test_mag_init_returns_ok(void)
{
  int ret = hmc5883l_init();
  CHECK(ret == 0, "hmc5883l_init() must return 0 on host stub");
  printf("  PASS T-MAG-01 hmc5883l_init() returns 0\n");
}

/* ========================================================================
 * T-MAG-02  hmc5883l_read() returns the fixed host-stub field vector
 * ======================================================================== */
static void test_mag_read_fixed_values(void)
{
  float field[3] = {0.0f, 0.0f, 0.0f};
  int ret = hmc5883l_read(field);
  CHECK(ret == 0, "hmc5883l_read() must return 0 on host stub");
  CHECK(FPEQ(field[0], 25.0f), "field[0] must be 25.0 \xc2\xb5T");
  CHECK(FPEQ(field[1], 0.0f), "field[1] must be 0.0 \xc2\xb5T");
  CHECK(FPEQ(field[2], 42.0f), "field[2] must be 42.0 \xc2\xb5T");
  printf("  PASS T-MAG-02 hmc5883l_read() returns {25, 0, 42} \xc2\xb5T\n");
}

/* ========================================================================
 * T-MAG-03  data_layer_write_mag() stores field values and sets mag_valid
 * ======================================================================== */
static void test_mag_dla_write(void)
{
  data_layer_init();

  float field[3] = {10.0f, -5.0f, 30.0f};
  data_layer_write_mag(field);

  dl_snapshot_t snap;
  data_layer_read(&snap);

  CHECK(snap.state.mag_valid, "mag_valid must be true after data_layer_write_mag()");
  CHECK(FPEQ(snap.state.mag_field[0], 10.0f), "mag_field[0] must equal written value");
  CHECK(FPEQ(snap.state.mag_field[1], -5.0f), "mag_field[1] must equal written value");
  CHECK(FPEQ(snap.state.mag_field[2], 30.0f), "mag_field[2] must equal written value");
  printf("  PASS T-MAG-03 data_layer_write_mag() stores field & sets mag_valid\n");
}

/* ========================================================================
 * T-MAG-04  vSensorReadTask_Step() with mag_available=true writes mag data
 * ======================================================================== */
static void test_mag_sensor_task_propagates_to_dla(void)
{
  data_layer_init();
  /* Mark only the magnetometer as available; IMU/temp left unavailable so
   * the test only exercises the mag path. */
  data_layer_set_sensor_avail(false, false);
  data_layer_set_mag_avail(true);

  /* imu_ret remains -1 → IMU path is skipped; mag path should still run */
  vSensorReadTask_Step();

  dl_snapshot_t snap;
  data_layer_read(&snap);

  CHECK(snap.state.mag_valid, "mag_valid must be true after Step() with mag_available");
  /* The real hmc5883l_read stub always returns {25, 0, 42}. */
  CHECK(FPEQ(snap.state.mag_field[0], 25.0f), "mag_field[0] must be stub value 25.0");
  CHECK(FPEQ(snap.state.mag_field[1], 0.0f), "mag_field[1] must be stub value 0.0");
  CHECK(FPEQ(snap.state.mag_field[2], 42.0f), "mag_field[2] must be stub value 42.0");
  printf("  PASS T-MAG-04 Step() propagates mag data to DLA\n");
}

/* ========================================================================
 * test_hmc5883l_init_success — T-MAG-05
 * ======================================================================== */
static void test_hmc5883l_init_success(void)
{
  int ret = hmc5883l_init();
  CHECK(ret == 0, "hmc5883l_init() must return 0 on host stub");
  printf("  PASS T-MAG-05 hmc5883l_init_success\n");
}

/* ========================================================================
 * test_hmc5883l_read_returns_nonzero — T-MAG-06
 * ======================================================================== */
static void test_hmc5883l_read_returns_nonzero(void)
{
  float field[3] = {0.0f, 0.0f, 0.0f};
  int ret = hmc5883l_read(field);
  CHECK(ret == 0, "hmc5883l_read() must return 0");
  CHECK(field[0] != 0.0f || field[1] != 0.0f || field[2] != 0.0f,
        "At least one field component must be non-zero");
  printf("  PASS T-MAG-06 hmc5883l_read_returns_nonzero\n");
}

/* ========================================================================
 * test_hmc5883l_read_field_components — T-MAG-07
 * LEO magnetic field is typically 20-60 µT per component
 * ======================================================================== */
static void test_hmc5883l_read_field_components(void)
{
  float field[3] = {0.0f, 0.0f, 0.0f};
  hmc5883l_read(field);

  float min_leo = 20.0f;
  float max_leo = 60.0f;

  int any_nonzero = (field[0] != 0.0f) || (field[1] != 0.0f) || (field[2] != 0.0f);
  CHECK(any_nonzero, "At least one component must be non-zero");

  int bx_ok = (fabsf(field[0]) >= min_leo && fabsf(field[0]) <= max_leo);
  int by_ok = (fabsf(field[1]) >= min_leo && fabsf(field[1]) <= max_leo);
  int bz_ok = (fabsf(field[2]) >= min_leo && fabsf(field[2]) <= max_leo);

  int all_ok = bx_ok || by_ok || bz_ok;
  CHECK(all_ok, "At least one component must be in LEO range [20, 60] µT");
  printf("  PASS T-MAG-07 hmc5883l_read_field_components\n");
}

/* ========================================================================
 * test_hmc5883l_init_called_multiple_times — T-MAG-08
 * ======================================================================== */
static void test_hmc5883l_init_called_multiple_times(void)
{
  int ret1 = hmc5883l_init();
  int ret2 = hmc5883l_init();
  int ret3 = hmc5883l_init();

  CHECK(ret1 == 0, "hmc5883l_init() first call must return 0");
  CHECK(ret2 == 0, "hmc5883l_init() second call must return 0");
  CHECK(ret3 == 0, "hmc5883l_init() third call must return 0");
  printf("  PASS T-MAG-08 hmc5883l_init_called_multiple_times (idempotent)\n");
}

/* ========================================================================
 * main
 * ======================================================================== */
int main(void)
{
  printf("=== HMC5883L driver + DLA integration tests (PR-18) ===\n");

  test_mag_init_returns_ok();
  test_mag_read_fixed_values();
  test_mag_dla_write();
  test_mag_sensor_task_propagates_to_dla();
  test_hmc5883l_init_success();
  test_hmc5883l_read_returns_nonzero();
  test_hmc5883l_read_field_components();
  test_hmc5883l_init_called_multiple_times();

  if (g_failures == 0)
  {
    printf("ALL HMC5883L TESTS PASSED\n");
    return 0;
  }
  printf("%d HMC5883L TEST(S) FAILED\n", g_failures);
  return 1;
}
