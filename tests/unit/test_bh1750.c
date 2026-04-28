// test_bh1750.c -- Unit tests for BH1750 driver stub and integration

#include "data_layer.h"
#include "bh1750.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ========== Sensor stubs for sensor_read_task.c linking ========== */
#include "drivers/imu/mpu6050.h"
#include "drivers/mag/hmc5883l.h"
#include "drivers/temperature.h"

/* Return failure so only lux path is exercised */
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

int hmc5883l_init(void)
{
  return 0;
}

int hmc5883l_read(float field_uT[3])
{
  (void)field_uT;
  return 0;
}

bool sht31_read(float *temperature, float *humidity)
{
  (void)temperature;
  (void)humidity;
  return false;
}

bool sht31_fetch(float *temperature, float *humidity)
{
  (void)temperature;
  (void)humidity;
  return false;
}

/* INA219 stub for sensor_read_task linking */
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

/* ========== Test functions ========== */

/* Test: BH1750 init returns true on host stub */
void test_bh1750_init_returns_ok()
{
  bool result = bh1750_init(BH1750_ADDR_DEFAULT);
  assert(result && "bh1750_init() must return true on host stub");
}

/* Test: BH1750 is_present returns true */
void test_bh1750_is_present()
{
  bool result = bh1750_is_present(BH1750_ADDR_DEFAULT);
  assert(result && "bh1750_is_present() must return true on host stub");
}

/* Test: BH1750 read returns false on host stub (no mock) */
void test_bh1750_read_returns_false()
{
  float lux = -1.0f;
  bool result = bh1750_read(&lux);
  /* Stub returns false by default (tests can override) */
  assert(!result && "bh1750_read() returns false on stub by default");
  (void)lux;
}

/* Test: Data layer write and read lux */
void test_data_layer_lux()
{
  data_layer_init();
  
  /* Write lux value */
  data_layer_write_lux(150.5f);
  
  /* Read snapshot and verify */
  dl_snapshot_t snap;
  data_layer_read(&snap);
  
  assert(snap.state.lux == 150.5f && "lux should be 150.5");
  assert(snap.state.lux_valid && "lux_valid should be true when lux >= 0");
}

/* Test: Data layer lux with invalid value (-1) */
void test_data_layer_lux_invalid()
{
  data_layer_init();
  
  /* Write invalid lux */
  data_layer_write_lux(-1.0f);
  
  dl_snapshot_t snap;
  data_layer_read(&snap);
  
  assert(snap.state.lux == -1.0f && "lux should be -1.0");
  assert(!snap.state.lux_valid && "lux_valid should be false when lux < 0");
}

/* Test: Data layer set lux availability */
void test_data_layer_lux_avail()
{
  data_layer_init();
  
  dl_snapshot_t snap;
  
  /* Initially should be unavailable */
  data_layer_read(&snap);
  assert(!snap.state.lux_available && "lux should be unavailable initially");
  
  /* Set as available */
  data_layer_set_lux_avail(true);
  data_layer_read(&snap);
  assert(snap.state.lux_available && "lux should be available after set_lux_avail(true)");
  
  /* Set as unavailable */
  data_layer_set_lux_avail(false);
  data_layer_read(&snap);
  assert(!snap.state.lux_available && "lux should be unavailable after set_lux_avail(false)");
}

/* Test: Data layer sequence increments on lux write */
void test_data_layer_lux_seq_increment()
{
  data_layer_init();
  
  dl_snapshot_t snap1, snap2;
  
  /* Write first value */
  data_layer_write_lux(100.0f);
  data_layer_read(&snap1);
  
  /* Write second value */
  data_layer_write_lux(200.0f);
  data_layer_read(&snap2);
  
  /* Sequence should increment */
  assert(snap2.seq > snap1.seq && "seq should increment after second write");
}

/* Test: BH1750 constants defined correctly */
void test_bh1750_constants()
{
  assert(BH1750_ADDR_DEFAULT == 0x23 && "Default address should be 0x23");
  assert(BH1750_ADDR_ALTERNATE == 0x5C && "Alternate address should be 0x5C");
  assert(BH1750_CMD_POWER_ON == 0x01 && "Power on command should be 0x01");
  assert(BH1750_CMD_RESET == 0x07 && "Reset command should be 0x07");
  assert(BH1750_CMD_OT_H_RES2 == 0x20 && "OT_H_RES2 command should be 0x20");
}

int main(void)
{
  printf("=== BH1750 Unit Tests ===\n");
  
  printf("Test: bh1750_init returns ok... ");
  test_bh1750_init_returns_ok();
  printf("PASS\n");
  
  printf("Test: bh1750_is_present... ");
  test_bh1750_is_present();
  printf("PASS\n");
  
  printf("Test: bh1750_read returns false... ");
  test_bh1750_read_returns_false();
  printf("PASS\n");
  
  printf("Test: data_layer write_lux... ");
  test_data_layer_lux();
  printf("PASS\n");
  
  printf("Test: data_layer lux invalid... ");
  test_data_layer_lux_invalid();
  printf("PASS\n");
  
  printf("Test: data_layer set_lux_avail... ");
  test_data_layer_lux_avail();
  printf("PASS\n");
  
  printf("Test: data_layer lux seq increment... ");
  test_data_layer_lux_seq_increment();
  printf("PASS\n");
  
  printf("Test: bh1750 constants... ");
  test_bh1750_constants();
  printf("PASS\n");
  
  printf("\n=== All BH1750 tests passed! ===\n");
  return 0;
}