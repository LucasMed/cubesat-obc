// test_ds3231.c -- Unit tests for DS3231 RTC driver stub and integration

#include "data_layer.h"
#include "ds3231.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ========== Sensor stubs for sensor_read_task.c linking ========== */
#include "drivers/imu/mpu6050.h"
#include "drivers/mag/hmc5883l.h"
#include "drivers/temperature.h"

/* Return failure so only RTC path is exercised */
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

/* Test: DS3231 init returns true on host stub */
void test_ds3231_init_returns_ok(void)
{
  assert(ds3231_init() && "ds3231_init() must return true on host stub");
  printf("  PASS T-DS3231-01 ds3231_init() returns true\n");
}

/* Test: DS3231 is_present returns true */
void test_ds3231_is_present(void)
{
  assert(ds3231_is_present() && "ds3231_is_present() must return true on host stub");
  printf("  PASS T-DS3231-02 ds3231_is_present() returns true\n");
}

/* Test: DS3231 read_time returns valid data */
void test_ds3231_read_time(void)
{
  uint16_t year   = 0;
  uint8_t month   = 0;
  uint8_t day     = 0;
  uint8_t hour    = 0;
  uint8_t minute  = 0;
  uint8_t second  = 0;

  assert(ds3231_read_time(&year, &month, &day, &hour, &minute, &second) &&
         "ds3231_read_time() must return true on host stub");
  assert(year >= 2000 && year <= 2099 && "year must be in valid range");
  assert(month >= 1 && month <= 12 && "month must be 1-12");
  assert(day >= 1 && day <= 31 && "day must be 1-31");
  assert(hour <= 23 && "hour must be 0-23");
  assert(minute <= 59 && "minute must be 0-59");
  assert(second <= 59 && "second must be 0-59");

  printf("  PASS T-DS3231-03 ds3231_read_time(): %04u-%02u-%02u %02u:%02u:%02u\n",
         year, month, day, hour, minute, second);
}

/* Test: DS3231 read_time rejects NULL pointers */
void test_ds3231_read_time_null_ptrs(void)
{
  uint16_t year   = 2024;
  uint8_t month   = 1;
  uint8_t day     = 1;
  uint8_t hour    = 12;
  uint8_t minute  = 0;
  uint8_t second  = 0;

  /* Suppress "unused variable" warnings — values are only used as non-NULL addresses */
  (void)year;
  (void)month;
  (void)day;
  (void)hour;
  (void)minute;
  (void)second;

  /* Each call with one NULL pointer should return false */
  assert(!ds3231_read_time(NULL, &month, &day, &hour, &minute, &second) &&
         "NULL year pointer must return false");
  assert(!ds3231_read_time(&year, NULL, &day, &hour, &minute, &second) &&
         "NULL month pointer must return false");
  assert(!ds3231_read_time(&year, &month, NULL, &hour, &minute, &second) &&
         "NULL day pointer must return false");
  assert(!ds3231_read_time(&year, &month, &day, NULL, &minute, &second) &&
         "NULL hour pointer must return false");
  assert(!ds3231_read_time(&year, &month, &day, &hour, NULL, &second) &&
         "NULL minute pointer must return false");
  assert(!ds3231_read_time(&year, &month, &day, &hour, &minute, NULL) &&
         "NULL second pointer must return false");

  printf("  PASS T-DS3231-04 ds3231_read_time() rejects NULL pointers\n");
}

/* Test: DS3231 set_time accepts valid values */
void test_ds3231_set_time_valid(void)
{
  /* Valid: 2025-06-15 14:30:45 */
  assert(ds3231_set_time(2025, 6, 15, 14, 30, 45) &&
         "ds3231_set_time() must return true for valid values");

  /* Valid boundary: 2000-01-01 00:00:00 */
  assert(ds3231_set_time(2000, 1, 1, 0, 0, 0) &&
         "ds3231_set_time() must accept 2000-01-01 boundary");

  /* Valid boundary: 2099-12-31 23:59:59 */
  assert(ds3231_set_time(2099, 12, 31, 23, 59, 59) &&
         "ds3231_set_time() must accept 2099-12-31 boundary");

  printf("  PASS T-DS3231-05 ds3231_set_time() accepts valid ranges\n");
}

/* Test: DS3231 set_time rejects invalid values */
void test_ds3231_set_time_invalid(void)
{
  /* Invalid year */
  assert(!ds3231_set_time(1999, 1, 1, 0, 0, 0) && "year < 2000 must return false");
  assert(!ds3231_set_time(2100, 1, 1, 0, 0, 0) && "year > 2099 must return false");

  /* Invalid month */
  assert(!ds3231_set_time(2025, 0, 1, 0, 0, 0) && "month < 1 must return false");
  assert(!ds3231_set_time(2025, 13, 1, 0, 0, 0) && "month > 12 must return false");

  /* Invalid day */
  assert(!ds3231_set_time(2025, 1, 0, 0, 0, 0) && "day < 1 must return false");

  /* Invalid hour */
  assert(!ds3231_set_time(2025, 1, 1, 24, 0, 0) && "hour > 23 must return false");

  /* Invalid minute */
  assert(!ds3231_set_time(2025, 1, 1, 0, 60, 0) && "minute > 59 must return false");

  /* Invalid second */
  assert(!ds3231_set_time(2025, 1, 1, 0, 0, 60) && "second > 59 must return false");

  printf("  PASS T-DS3231-06 ds3231_set_time() rejects invalid ranges\n");
}

/* Test: DS3231 to_epoch conversion */
void test_ds3231_to_epoch(void)
{
  /* Host stub implements real conversion. Verify known correct epoch:
   * 2024-01-01 12:00:00 UTC = days(1970→2024-01-01) * 86400 + 43200
   * Days from 1970 to 2024-01-01: 41 regular years * 365 + 13 leap years * 366 = 19723
   * 19723 * 86400 + 43200 = 1704110400 */
  uint32_t epoch = ds3231_to_epoch(2024, 1, 1, 12, 0, 0);
  assert(epoch == 1704110400UL && "2024-01-01 12:00:00 UTC must be 1704110400");

  printf("  PASS T-DS3231-07 ds3231_to_epoch() conversion correct\n");
}

/* Test: DS3231 epoch conversion for various dates */
void test_ds3231_epoch_dates(void)
{
  /* Known Unix timestamps (UTC) */
  struct
  {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint32_t expected;
  } cases[] = {
    /* Unix epoch: 1970-01-01 00:00:00 */
    {1970, 1, 1, 0, 0, 0, 0},
    /* 2024-01-01 00:00:00 = 1704067200 */
    {2024, 1, 1, 0, 0, 0, 1704067200UL},
    /* 2024-06-15 12:00:00 = 1718452800 */
    {2024, 6, 15, 12, 0, 0, 1718452800UL},
    /* 2025-01-01 00:00:00 = 1735689600 */
    {2025, 1, 1, 0, 0, 0, 1735689600UL},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    uint32_t ts = ds3231_to_epoch(cases[i].year, cases[i].month, cases[i].day,
                                   cases[i].hour, cases[i].minute, cases[i].second);
    assert(ts == cases[i].expected &&
           "epoch conversion mismatch");
    printf("  PASS T-DS3231-08.%zu: %04u-%02u-%02u %02u:%02u:%02u = %lu\n",
           i, cases[i].year, cases[i].month, cases[i].day,
           cases[i].hour, cases[i].minute, cases[i].second,
           (unsigned long)ts);
  }
}

/* Test: DS3231 constants defined correctly */
void test_ds3231_constants(void)
{
  assert(DS3231_ADDR == 0x68 && "I2C address must be 0x68");
  assert(DS3231_REG_SECONDS == 0x00 && "REG_SECONDS must be 0x00");
  assert(DS3231_REG_MINUTES == 0x01 && "REG_MINUTES must be 0x01");
  assert(DS3231_REG_HOURS == 0x02 && "REG_HOURS must be 0x02");
  assert(DS3231_REG_DAY == 0x03 && "REG_DAY must be 0x03");
  assert(DS3231_REG_DATE == 0x04 && "REG_DATE must be 0x04");
  assert(DS3231_REG_MONTH == 0x05 && "REG_MONTH must be 0x05");
  assert(DS3231_REG_YEAR == 0x06 && "REG_YEAR must be 0x06");
  assert(DS3231_REG_CONTROL == 0x0E && "REG_CONTROL must be 0x0E");
  assert(DS3231_REG_STATUS == 0x0F && "REG_STATUS must be 0x0F");

  printf("  PASS T-DS3231-09 DS3231 constants correct\n");
}

/* ========== Main test runner ========== */

int main(void)
{
  printf("\n=== DS3231 RTC Driver Unit Tests ===\n");

  printf("\n--- Basic Functionality ---\n");
  test_ds3231_init_returns_ok();
  test_ds3231_is_present();
  test_ds3231_constants();

  printf("\n--- Read Time ---\n");
  test_ds3231_read_time();
  test_ds3231_read_time_null_ptrs();

  printf("\n--- Set Time ---\n");
  test_ds3231_set_time_valid();
  test_ds3231_set_time_invalid();

  printf("\n--- Epoch Conversion ---\n");
  test_ds3231_to_epoch();
  test_ds3231_epoch_dates();

  printf("\n=== All DS3231 tests PASSED ===\n");
  return 0;
}
