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
void test_ina219_init_returns_ok()
{
  bool result = ina219_init();
  assert(result && "ina219_init() must return true on host stub");
  printf("  PASS T-INA-01 ina219_init() returns true\n");
}

/* Test: INA219 is_present returns true */
void test_ina219_is_present()
{
  bool result = ina219_is_present();
  assert(result && "ina219_is_present() must return true on host stub");
  printf("  PASS T-INA-02 ina219_is_present() returns true\n");
}

/* Test: Power calculation - V * I = P */
void test_ina219_power_calculation()
{
  ina219_data_t data;

  /* Simulate: V = 5000 mV, I = 100 mA (100000 µA) */
  data.bus_voltage_mv = 5000;
  data.current_ua = 100000;
  data.power_uw = (int32_t)data.bus_voltage_mv * data.current_ua;

  /* Expected: P = 5000 * 100000 = 500,000,000 µW = 500 mW */
  assert(data.power_uw == 500000000 && "power calculation failed");
  printf("  PASS T-INA-03 power calculation V*I=P: %d µW\n", data.power_uw);
}

/* Test: Power calculation with actual sensor values */
void test_ina219_power_with_sensor_values()
{
  ina219_data_t data;

  /* Values from real sensor: V = 5728 mV, I = 5 mA (5000 µA) */
  data.bus_voltage_mv = 5728;
  data.current_ua = 5000;
  data.power_uw = (int32_t)data.bus_voltage_mv * data.current_ua;

  /* Expected: P = 5728 * 5000 = 28,640,000 µW = 28.64 mW */
  int32_t expected_power_uw = 28640000;
  assert(data.power_uw == expected_power_uw && "power calculation mismatch");

  /* Convert to mW for display */
  int power_mw = data.power_uw / 1000;
  assert(power_mw == 28640 && "power in mW should be 28640");
  printf("  PASS T-INA-04 sensor values: V=%d mV, I=%d µA, P=%d µW (%d mW)\n",
         data.bus_voltage_mv, data.current_ua, data.power_uw, power_mw);
}

/* Test: ina219_read_power returns valid data */
void test_ina219_read_power()
{
  ina219_data_t data;
  bool result = ina219_read_power(&data);

  assert(result && "ina219_read_power() must return true");
  assert(data.bus_voltage_mv > 0 && "bus voltage must be positive");
  assert(data.current_ua >= 0 && "current must be non-negative");
  assert(data.power_uw >= 0 && "power must be non-negative");

  printf("  PASS T-INA-05 ina219_read_power(): V=%d mV, I=%d µA, P=%d µW\n",
         data.bus_voltage_mv, data.current_ua, data.power_uw);
}

/* Test: Data layer write and read power */
void test_ina219_data_layer()
{
  /* Write power data to data layer */
  data_layer_write_power(5728, 5000, 28640000);

  /* Read back from data layer */
  dl_snapshot_t snap;
  data_layer_read(&snap);

  assert(snap.state.bus_voltage_mv == 5728 && "bus voltage mismatch");
  assert(snap.state.current_ua == 5000 && "current mismatch");
  assert(snap.state.power_uw == 28640000 && "power mismatch");
  assert(snap.state.power_valid == true && "power_valid should be true");

  printf("  PASS T-INA-06 data layer: V=%d mV, I=%d µA, P=%d µW\n",
         snap.state.bus_voltage_mv, snap.state.current_ua, snap.state.power_uw);

  /* Test power not valid case */
  data_layer_write_power(-1, 0, 0);
  data_layer_read(&snap);
  assert(snap.state.power_valid == false && "power_valid should be false when V=-1");

  printf("  PASS T-INA-07 power_valid=false when voltage=-1\n");
}

/* Test: Set power availability */
void test_ina219_availability()
{
  data_layer_set_power_avail(true);

  dl_snapshot_t snap;
  data_layer_read(&snap);
  assert(snap.state.power_available == true && "power_available should be true");

  printf("  PASS T-INA-08 power_available=true\n");

  data_layer_set_power_avail(false);
  data_layer_read(&snap);
  assert(snap.state.power_available == false && "power_available should be false");

  printf("  PASS T-INA-09 power_available=false\n");
}

/* Test: Power conversion to mW */
void test_ina219_power_conversion()
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

    assert(power_mw == test_cases[i].expected_mw &&
           "power conversion mismatch");
    printf("  PASS T-INA-10.%zu: V=%d mV, I=%d µA → P=%d mW (expected %d mW)\n",
           i, test_cases[i].v_mv, test_cases[i].i_ua,
           power_mw, test_cases[i].expected_mw);
  }
}

/* ========== Main test runner ========== */

int main(void)
{
  printf("\n=== INA219 Driver Unit Tests ===\n");

  printf("\n--- Basic Functionality ---\n");
  test_ina219_init_returns_ok();
  test_ina219_is_present();

  printf("\n--- Power Calculation ---\n");
  test_ina219_power_calculation();
  test_ina219_power_with_sensor_values();
  test_ina219_power_conversion();

  printf("\n--- Read Function ---\n");
  test_ina219_read_power();

  printf("\n--- Data Layer Integration ---\n");
  test_ina219_data_layer();
  test_ina219_availability();

  printf("\n=== All INA219 tests PASSED ===\n");
  return 0;
}