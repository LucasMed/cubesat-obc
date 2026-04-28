// test_sht31.c -- Unit tests for SHT31 temperature/humidity driver stub

#include "data_layer.h"
#include "sht31.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ========== Sensor stubs for sensor_read_task.c linking ========== */
#include "drivers/imu/mpu6050.h"
#include "drivers/mag/hmc5883l.h"
#include "drivers/temperature.h"

/* Return failure so only SHT31 path is exercised */
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

/* INA219 stub for sensor_read_task linking */
#include "ina219.h"
// NOLINTNEXTLINE(readability-non-const-parameter)
bool ina219_read_power(ina219_data_t *data)
{
  (void)data;
  return false;
}

/* BH1750 stub */
#include "bh1750.h"
bool bh1750_read(float *lux)
{
  (void)lux;
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

/* Test: SHT31 init returns true on host stub */
void test_sht31_init_returns_ok(void)
{
  assert(sht31_init(SHT31_ADDR_DEFAULT) && "sht31_init() must return true on host stub");
  printf("  PASS T-SHT31-01 sht31_init() returns true\n");
}

/* Test: SHT31 is_present returns true */
void test_sht31_is_present(void)
{
  assert(sht31_is_present(SHT31_ADDR_DEFAULT) && "sht31_is_present() must return true on host stub");
  printf("  PASS T-SHT31-02 sht31_is_present() returns true\n");
}

/* Test: SHT31 read returns false on host stub (no mock data) */
void test_sht31_read_returns_false(void)
{
  float temp = -999.0f;
  float hum  = -999.0f;
  assert(!sht31_read(&temp, &hum) && "sht31_read() returns false on stub by default");
  (void)temp;
  (void)hum;
  printf("  PASS T-SHT31-03 sht31_read() returns false on stub\n");
}

/* Test: SHT31 constants defined correctly */
void test_sht31_constants(void)
{
  assert(SHT31_ADDR_DEFAULT == 0x44 && "Default address must be 0x44");
  assert(SHT31_ADDR_ALTERNATE == 0x45 && "Alternate address must be 0x45");
  printf("  PASS T-SHT31-04 SHT31 constants correct\n");
}

/* Test: SHT31 set_heater is callable */
void test_sht31_set_heater(void)
{
  /* Enable heater */
  assert(sht31_set_heater(true) && "sht31_set_heater(true) must return true");

  /* Disable heater */
  assert(sht31_set_heater(false) && "sht31_set_heater(false) must return true");

  printf("  PASS T-SHT31-05 sht31_set_heater() callable\n");
}

/* Test: Data layer write and read temperature */
void test_data_layer_write_temp(void)
{
  data_layer_init();

  /* Write temperature value */
  data_layer_write_temp(25.0f);

  /* Read snapshot and verify */
  dl_snapshot_t snap;
  data_layer_read(&snap);

  assert(snap.state.temp == 25.0f && "temperature should be 25.0");
  assert(snap.state.temp_valid && "temp_valid should be true after write_temp");
  printf("  PASS T-SHT31-06 data_layer_write_temp(): temp=%.1f C, valid=%d\n",
         snap.state.temp, snap.state.temp_valid);
}

/* Test: Data layer write and read humidity */
void test_data_layer_write_humidity(void)
{
  data_layer_init();

  /* Write humidity value */
  data_layer_write_humidity(60.5f);

  dl_snapshot_t snap;
  data_layer_read(&snap);

  assert(snap.state.humidity == 60.5f && "humidity should be 60.5");
  assert(snap.state.humidity_valid && "humidity_valid should be true when humidity >= 0");
  printf("  PASS T-SHT31-07 data_layer_write_humidity(): humidity=%.1f %%, valid=%d\n",
         snap.state.humidity, snap.state.humidity_valid);
}

/* Test: Data layer humidity with invalid value (-1) */
void test_data_layer_humidity_invalid(void)
{
  data_layer_init();

  /* Write invalid humidity */
  data_layer_write_humidity(-1.0f);

  dl_snapshot_t snap;
  data_layer_read(&snap);

  /* When humidity < 0, humidity_valid should be false */
  assert(snap.state.humidity == -1.0f && "humidity should be -1.0");
  assert(!snap.state.humidity_valid && "humidity_valid should be false when humidity < 0");
  printf("  PASS T-SHT31-08 data_layer: humidity=-1.0 marked as invalid\n");
}

/* Test: Data layer temperature sequence increments */
void test_data_layer_temp_seq_increment(void)
{
  data_layer_init();

  dl_snapshot_t snap1, snap2;

  /* Write first value */
  data_layer_write_temp(20.0f);
  data_layer_read(&snap1);

  /* Write second value */
  data_layer_write_temp(21.0f);
  data_layer_read(&snap2);

  /* Sequence should increment */
  assert(snap2.seq > snap1.seq && "seq should increment after second write");
  printf("  PASS T-SHT31-09 data_layer: seq increments after write (seq1=%lu, seq2=%lu)\n",
         (unsigned long)snap1.seq, (unsigned long)snap2.seq);
}

/* Test: Temperature range values */
void test_temperature_ranges(void)
{
  data_layer_init();

  struct
  {
    float temp;
    const char *desc;
  } cases[] = {
    {25.0f,  "room temperature"},
    {-40.0f, "SHT31 min (-40C)"},
    {125.0f, "SHT31 max (125C)"},
    {0.0f,   "freezing point"},
    {37.0f,  "body temperature"},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    data_layer_write_temp(cases[i].temp);
    dl_snapshot_t snap;
    data_layer_read(&snap);
    assert(snap.state.temp == cases[i].temp && "temperature value mismatch");
    printf("  PASS T-SHT31-10.%zu: %.1f C (%s)\n", i, cases[i].temp, cases[i].desc);
  }
}

/* Test: Humidity range values */
void test_humidity_ranges(void)
{
  data_layer_init();

  struct
  {
    float hum;
    const char *desc;
  } cases[] = {
    {0.0f,   "dry"},
    {50.0f,  "typical indoor"},
    {100.0f, "saturated"},
    {77.1f,  "measured on hardware"},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    data_layer_write_humidity(cases[i].hum);
    dl_snapshot_t snap;
    data_layer_read(&snap);
    assert(snap.state.humidity == cases[i].hum && "humidity value mismatch");
    printf("  PASS T-SHT31-11.%zu: %.1f %% (%s)\n", i, cases[i].hum, cases[i].desc);
  }
}

/* ========== Main test runner ========== */

int main(void)
{
  printf("\n=== SHT31 Temperature/Humidity Driver Unit Tests ===\n");

  printf("\n--- Basic Functionality ---\n");
  test_sht31_init_returns_ok();
  test_sht31_is_present();
  test_sht31_read_returns_false();
  test_sht31_constants();
  test_sht31_set_heater();

  printf("\n--- Data Layer Integration ---\n");
  test_data_layer_write_temp();
  test_data_layer_write_humidity();
  test_data_layer_humidity_invalid();
  test_data_layer_temp_seq_increment();

  printf("\n--- Range Validation ---\n");
  test_temperature_ranges();
  test_humidity_ranges();

  printf("\n=== All SHT31 tests PASSED ===\n");
  return 0;
}
