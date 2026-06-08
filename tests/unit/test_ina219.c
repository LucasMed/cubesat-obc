// test_ina219.c -- Unit tests for INA219 driver stub and integration

#include "data_layer.h"
#include "ina219.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ========== Sensor stubs for sensor_read_task.c linking ========== */
#include "drivers/imu/mpu6050.h"
#include "drivers/mag/hmc5883l.h"
#include "drivers/temperature.h"

static int s_imu_ret = 0;

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

/* Test: INA219 init returns true on host stub */
void test_ina219_init_returns_ok(void)
{
  bool result = ina219_init();
  assert(result);
  printf("  PASS T-INA-01 ina219_init() returns true\n");
}

/* Test: INA219 is_present returns true */
void test_ina219_is_present(void)
{
  bool result = ina219_is_present();
  assert(result);
  printf("  PASS T-INA-02 ina219_is_present() returns true\n");
}

/* Test: Power calculation - V * I = P */
void test_ina219_power_calculation(void)
{
  ina219_data_t data;

  /* Simulate: V = 5000 mV, I = 100 mA (100000 µA) */
  data.bus_voltage_mv = 5000;
  data.current_ua = 100000;
  data.power_uw = (int32_t)data.bus_voltage_mv * data.current_ua;

  /* Expected: P = 5000 * 100000 = 500,000,000 µW = 500 mW */
  assert(data.power_uw == 500000000);
  printf("  PASS T-INA-03 power calculation V*I=P: %d µW\n", data.power_uw);
}

/* Test: Power calculation with actual sensor values */
void test_ina219_power_with_sensor_values(void)
{
  ina219_data_t data;

  /* Values from real sensor: V = 5728 mV, I = 5 mA (5000 µA) */
  data.bus_voltage_mv = 5728;
  data.current_ua = 5000;
  data.power_uw = (int32_t)data.bus_voltage_mv * data.current_ua;

  /* Expected: P = 5728 * 5000 = 28,640,000 µW = 28.64 mW */
  assert(data.power_uw == 28640000);

  /* Convert to mW for display */
  int power_mw = data.power_uw / 1000;
  assert(power_mw == 28640);
  printf("  PASS T-INA-04 sensor values: V=%d mV, I=%d µA, P=%d µW (%d mW)\n",
         data.bus_voltage_mv, data.current_ua, data.power_uw, power_mw);
}

/* Test: ina219_read_power returns valid data */
void test_ina219_read_power(void)
{
  ina219_data_t data;
  bool result = ina219_read_power(&data);

  assert(result);
  assert(data.bus_voltage_mv > 0);
  assert(data.current_ua >= 0);
  assert(data.power_uw >= 0);

  printf("  PASS T-INA-05 ina219_read_power(): V=%d mV, I=%d µA, P=%d µW\n",
         data.bus_voltage_mv, data.current_ua, data.power_uw);
}

/* Test: Data layer write and read power */
void test_ina219_data_layer(void)
{
  /* Write power data to data layer */
  data_layer_write_power(5728, 5000, 28640000);

  /* Read back from data layer */
  dl_snapshot_t snap;
  data_layer_read(&snap);

  assert(snap.state.bus_voltage_mv == 5728);
  assert(snap.state.current_ua == 5000);
  assert(snap.state.power_uw == 28640000);
  assert(snap.state.power_valid == true);

  printf("  PASS T-INA-06 data layer: V=%d mV, I=%d µA, P=%d µW\n",
         snap.state.bus_voltage_mv, snap.state.current_ua, snap.state.power_uw);

  /* Test power not valid case */
  data_layer_write_power(-1, 0, 0);
  data_layer_read(&snap);
  assert(snap.state.power_valid == false);

  printf("  PASS T-INA-07 power_valid=false when voltage=-1\n");
}

/* Test: Set power availability */
void test_ina219_availability(void)
{
  data_layer_set_power_avail(true);

  dl_snapshot_t snap;
  data_layer_read(&snap);
  assert(snap.state.power_available == true);

  printf("  PASS T-INA-08 power_available=true\n");

  data_layer_set_power_avail(false);
  data_layer_read(&snap);
  assert(snap.state.power_available == false);

  printf("  PASS T-INA-09 power_available=false\n");
}

/* Test: Device-instance based API */
void test_ina219_init_device(void)
{
  ina219_t dev;

  /* NULL pointer guard */
  bool null_ok = ina219_init_device(NULL, INA219_ADDR);
  assert(!null_ok);
  printf("  PASS T-INA-11 NULL pointer guard on init_device\n");

  /* Valid init */
  bool init_ok = ina219_init_device(&dev, INA219_ADDR);
  assert(init_ok);
  assert(dev.initialized);
  assert(dev.addr == INA219_ADDR);
  printf("  PASS T-INA-12 ina219_init_device() at 0x%02X\n", (unsigned)dev.addr);
}

/* Test: Device-instance read power */
void test_ina219_device_read_power(void)
{
  ina219_t dev;
  ina219_data_t data;

  bool init_ok = ina219_init_device(&dev, INA219_ADDR);
  assert(init_ok);

  bool read_ok = ina219_device_read_power(&dev, &data);
  assert(read_ok);
  assert(data.bus_voltage_mv > 0);
  assert(data.current_ua >= 0);

  /* Check that instance state was updated */
  assert(dev.last_voltage_mv == data.bus_voltage_mv);
  assert(dev.last_reading_ms > 0);

  printf("  PASS T-INA-13 device_read: V=%d mV, I=%d µA, P=%d µW\n",
         data.bus_voltage_mv, data.current_ua, data.power_uw);
}

/* Test: Solar INA219 init */
void test_ina219_solar_init_ok(void)
{
  bool result = ina219_solar_init();
  assert(result);
  printf("  PASS T-INA-14 ina219_solar_init() returns true\n");
}

/* Test: Solar INA219 is_present */
void test_ina219_solar_is_present(void)
{
  bool result = ina219_solar_is_present();
  assert(result);
  printf("  PASS T-INA-15 ina219_solar_is_present() returns true\n");
}

/* Test: Solar INA219 read returns valid data */
void test_ina219_solar_read_power(void)
{
  ina219_data_t data;
  bool result = ina219_solar_read_power(&data);

  assert(result);
  assert(data.bus_voltage_mv > 0);
  assert(data.current_ua >= 0);
  assert(data.power_uw >= 0);

  /* Solar mock data is 6.5V, different from bus mock 5.0V */
  assert(data.bus_voltage_mv >= 6000);
  printf("  PASS T-INA-16 solar_read: V=%d mV, I=%d µA, P=%d µW\n",
         data.bus_voltage_mv, data.current_ua, data.power_uw);
}

/* Test: Solar panel data layer write and read */
void test_ina219_solar_data_layer(void)
{
  /* Write solar power data to data layer */
  data_layer_write_solar_power(6500, 100000, 650000);

  /* Read back from data layer */
  dl_snapshot_t snap;
  data_layer_read(&snap);

  assert(snap.state.solar_voltage_mv == 6500);
  assert(snap.state.solar_current_ua == 100000);
  assert(snap.state.solar_power_uw == 650000);
  assert(snap.state.solar_valid == true);

  printf("  PASS T-INA-17 solar data layer: V=%d mV, I=%d µA, P=%d µW\n",
         snap.state.solar_voltage_mv, snap.state.solar_current_ua, snap.state.solar_power_uw);

  /* Test solar not valid case */
  data_layer_write_solar_power(-1, 0, 0);
  data_layer_read(&snap);
  assert(snap.state.solar_valid == false);

  printf("  PASS T-INA-18 solar_valid=false when voltage=-1\n");
}

/* Test: Solar availability */
void test_ina219_solar_availability(void)
{
  data_layer_set_solar_avail(true);

  dl_snapshot_t snap;
  data_layer_read(&snap);
  assert(snap.state.solar_available == true);

  printf("  PASS T-INA-19 solar_available=true\n");

  data_layer_set_solar_avail(false);
  data_layer_read(&snap);
  assert(snap.state.solar_available == false);

  printf("  PASS T-INA-20 solar_available=false\n");
}

/* Test: Power conversion to mW */
void test_ina219_power_conversion(void)
{
  /* Test multiple values */
  struct
  {
    int16_t v_mv;
    int32_t i_ua;
    int32_t expected_mw;
  } test_cases[] = {
    {5000, 100000, 500000},   /* 5V * 100mA = 500 mW */
    {5728, 5000, 28640},      /* 5.7V * 5mA = 28.64mW ≈ 28640mW (stored as µW then divided) */
    {3700, 50000, 185000},    /* 3.7V * 50mA = 185mW */
    {4200, 0, 0},             /* 4.2V * 0mA = 0mW */
  };

  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
  {
    int32_t power_uw = (int32_t)test_cases[i].v_mv * test_cases[i].i_ua;
    int32_t power_mw = power_uw / 1000;

    assert(power_mw == test_cases[i].expected_mw);
    printf("  PASS T-INA-10.%zu: V=%d mV, I=%d µA → P=%d mW (expected %d mW)\n",
           i, test_cases[i].v_mv, test_cases[i].i_ua,
           power_mw, test_cases[i].expected_mw);
  }
}

/* Test: Device-instance read power — NULL guards */
void test_ina219_device_read_power_null_guards(void)
{
  /* NULL dev guard */
  ina219_data_t data;
  bool null_dev = ina219_device_read_power(NULL, &data);
  assert(!null_dev);

  /* NULL data guard */
  ina219_t dev;
  ina219_init_device(&dev, INA219_ADDR);
  bool null_data = ina219_device_read_power(&dev, NULL);
  assert(!null_data);

  printf("  PASS T-INA-21 device_read_power NULL guards OK\n");
}

/* Test: Device-instance is_present */
void test_ina219_device_is_present(void)
{
  bool result = ina219_device_is_present(NULL);
  assert(result);
  printf("  PASS T-INA-22 device_is_present() returns true\n");
}

/* Test: Device-instance reset */
void test_ina219_device_reset(void)
{
  /* NULL guard */
  bool null_ok = ina219_device_reset(NULL);
  assert(!null_ok);
  printf("  PASS T-INA-23 device_reset NULL guard OK\n");

  /* Normal path: init, read, reset, verify state cleared */
  ina219_t dev;
  ina219_init_device(&dev, INA219_ADDR);
  ina219_data_t d;
  ina219_device_read_power(&dev, &d);
  assert(dev.last_voltage_mv != 0);
  assert(dev.last_reading_ms != 0);

  bool reset_ok = ina219_device_reset(&dev);
  assert(reset_ok);
  assert(dev.last_voltage_mv == 0);
  assert(dev.last_reading_ms == 0);
  printf("  PASS T-INA-24 device_reset clears state\n");
}

/* Test: Device-instance get_last_reading_ms */
void test_ina219_device_get_last_reading_ms(void)
{
  /* NULL guard */
  uint32_t null_ms = ina219_device_get_last_reading_ms(NULL);
  assert(null_ms == 0);
  printf("  PASS T-INA-25 device_get_last_reading_ms NULL guard OK\n");

  /* Normal path: init, read, then get */
  ina219_t dev;
  ina219_init_device(&dev, INA219_ADDR);
  ina219_data_t d;
  ina219_device_read_power(&dev, &d);
  assert(ina219_device_get_last_reading_ms(&dev) > 0);
  printf("  PASS T-INA-26 device_get_last_reading_ms returns value\n");
}

/* Test: Device-instance get_voltage_mv */
void test_ina219_device_get_voltage_mv(void)
{
  /* NULL guard */
  int16_t null_mv = ina219_device_get_voltage_mv(NULL);
  assert(null_mv == 0);
  printf("  PASS T-INA-27 device_get_voltage_mv NULL guard OK\n");

  /* Normal path: init, read, then get */
  ina219_t dev;
  ina219_init_device(&dev, INA219_ADDR);
  ina219_data_t d;
  ina219_device_read_power(&dev, &d);
  assert(ina219_device_get_voltage_mv(&dev) > 0);
  printf("  PASS T-INA-28 device_get_voltage_mv returns value\n");
}

/* Test: Legacy singleton — auto-init path (read without prior init) */
void test_ina219_read_power_auto_init(void)
{
  /* This test MUST be first in the sequence to ensure s_bus_initialised=false.
   * Since assert() aborts, we guard by resetting via the test sequence.
   * Run this before any init-based tests. */
  ina219_data_t data;
  bool result = ina219_read_power(&data);

  assert(result);
  assert(data.bus_voltage_mv > 0);
  printf("  PASS T-INA-29 ina219_read_power auto-init OK\n");
}

/* Test: Legacy singleton — reset */
void test_ina219_reset(void)
{
  /* Read first to set state, then reset */
  ina219_data_t data;
  ina219_read_power(&data);

  bool reset_ok = ina219_reset();
  assert(reset_ok);

  /* After reset, last_reading_ms should be 0 */
  assert(ina219_get_last_reading_ms() == 0);
  assert(ina219_get_voltage_mv() == 0);
  printf("  PASS T-INA-30 ina219_reset OK\n");
}

/* Test: Legacy singleton — get_last_reading_ms and get_voltage_mv */
void test_ina219_singleton_getters(void)
{
  /* Read first to populate state */
  ina219_data_t data;
  ina219_read_power(&data);

  uint32_t ms = ina219_get_last_reading_ms();
  assert(ms > 0);

  int16_t mv = ina219_get_voltage_mv();
  assert(mv > 0);

  printf("  PASS T-INA-31 singleton getters: last_ms=%lu, voltage_mv=%d\n",
         (unsigned long)ms, mv);
}

/* Test: Solar — auto-init path */
void test_ina219_solar_read_power_auto_init(void)
{
  ina219_data_t data;
  bool result = ina219_solar_read_power(&data);

  assert(result);
  assert(data.bus_voltage_mv >= 6000);
  printf("  PASS T-INA-32 ina219_solar_read_power auto-init OK\n");
}

/* Test: Solar — reset */
void test_ina219_solar_reset(void)
{
  /* Read first to set state */
  ina219_data_t data;
  ina219_solar_read_power(&data);

  bool reset_ok = ina219_solar_reset();
  assert(reset_ok);

  assert(ina219_solar_get_last_reading_ms() == 0);
  assert(ina219_solar_get_voltage_mv() == 0);
  printf("  PASS T-INA-33 ina219_solar_reset OK\n");
}

/* Test: Solar — getters */
void test_ina219_solar_getters(void)
{
  ina219_data_t data;
  ina219_solar_read_power(&data);

  assert(ina219_solar_get_last_reading_ms() > 0);
  assert(ina219_solar_get_voltage_mv() > 0);
  printf("  PASS T-INA-34 solar getters return values\n");
}

/* ========== Main test runner ========== */

int main(void)
{
  printf("\n=== INA219 Driver Unit Tests ===\n");

  /* ── Auto-init path tests ──
   * These MUST run before init() is called for the first time,
   * because the auto-init guard checks a static flag.
   * The first read_power call triggers init() internally.        */
  printf("\n--- Auto-Init Path (no prior init) ---\n");
  test_ina219_read_power_auto_init();
  test_ina219_solar_read_power_auto_init();

  printf("\n--- Basic Functionality ---\n");
  test_ina219_init_returns_ok();
  test_ina219_is_present();

  printf("\n--- Power Calculation ---\n");
  test_ina219_power_calculation();
  test_ina219_power_with_sensor_values();
  test_ina219_power_conversion();

  printf("\n--- Read Function ---\n");
  test_ina219_read_power();

  printf("\n--- Device Instance API ---\n");
  test_ina219_init_device();
  test_ina219_device_read_power();
  test_ina219_device_read_power_null_guards();
  test_ina219_device_is_present();
  test_ina219_device_reset();
  test_ina219_device_get_last_reading_ms();
  test_ina219_device_get_voltage_mv();

  printf("\n--- Legacy Singleton API ---\n");
  test_ina219_reset();
  test_ina219_singleton_getters();

  printf("\n--- Solar Panel INA219 ---\n");
  test_ina219_solar_init_ok();
  test_ina219_solar_is_present();
  test_ina219_solar_read_power();

  printf("\n--- Solar API (reset, getters) ---\n");
  test_ina219_solar_reset();
  test_ina219_solar_getters();

  printf("\n--- Solar Data Layer Integration ---\n");
  test_ina219_solar_data_layer();
  test_ina219_solar_availability();

  printf("\n--- Data Layer Integration ---\n");
  test_ina219_data_layer();
  test_ina219_availability();

  printf("\n=== All INA219 tests PASSED ===\n");
  return 0;
}
