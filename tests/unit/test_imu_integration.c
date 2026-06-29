/**
 * @file test_imu_integration.c
 * @brief IMU integration tests - NULL parameter handling.
 *
 * Tests:
 *   T-IMU-EXT-01 - mpu6050_read_raw with NULL accel parameter
 *   T-IMU-EXT-02 - mpu6050_read_raw with NULL gyro parameter
 *   T-DL-EXT-01 - data_layer_write_imu with NULL att_rad
 *   T-DL-EXT-02 - data_layer_write_imu with NULL rates_rad
 *
 * Driver stubs replace actual I2C hardware access.
 */

#include "data_layer.h"
#include "drivers/imu/mpu6050.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

/* ------------------------------------------------------------------ */
/* Driver stubs                                                        */
/* ------------------------------------------------------------------ */

static int g_i2c_calls = 0;
static bool g_i2c_success = true;

int i2c_bus_write(uint8_t addr, uint8_t *data, uint8_t len)
{
  (void)addr; (void)data; (void)len;
  g_i2c_calls++;
  return g_i2c_success ? (int)len : -1;
}

int i2c_bus_write_read(uint8_t addr, uint8_t *wdata, uint8_t wlen, uint8_t *rdata, uint8_t rlen)
{
  (void)addr; (void)wdata; (void)wlen;
  g_i2c_calls++;
  if (!g_i2c_success)
  {
    return -1;
  }
  if (rlen == 1)
  {
    rdata[0] = 0x68;
  }
  else if (rlen == 14)
  {
    int16_t ax = 16384;
    int16_t ay = 0;
    int16_t az = 0;
    int16_t gx = 131;
    int16_t gy = 65;
    int16_t gz = -131;
    rdata[0] = (uint8_t)(ax >> 8);
    rdata[1] = (uint8_t)(ax & 0xFF);
    rdata[2] = (uint8_t)(ay >> 8);
    rdata[3] = (uint8_t)(ay & 0xFF);
    rdata[4] = (uint8_t)(az >> 8);
    rdata[5] = (uint8_t)(az & 0xFF);
    rdata[6] = 0;
    rdata[7] = 0;
    rdata[8] = (uint8_t)(gx >> 8);
    rdata[9] = (uint8_t)(gx & 0xFF);
    rdata[10] = (uint8_t)(gy >> 8);
    rdata[11] = (uint8_t)(gy & 0xFF);
    rdata[12] = (uint8_t)(gz >> 8);
    rdata[13] = (uint8_t)(gz & 0xFF);
  }
  return (int)rlen;
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
      printf("  FAIL [%s:%d] %s\n", __func__, __LINE__, msg);                                      \
      g_failures++;                                                                                \
    }                                                                                              \
  } while (0)

#define PASS(msg) printf("  PASS %s\n", msg)

/* ------------------------------------------------------------------ */
/* Setup                                                               */
/* ------------------------------------------------------------------ */

void setup(void)
{
  g_i2c_calls = 0;
  g_i2c_success = true;
  data_layer_init();
}

/* ------------------------------------------------------------------ */
/* T-IMU-EXT-01: mpu6050_read_raw with NULL accel only                */
/* ------------------------------------------------------------------ */

void test_imu_null_accel(void)
{
  setup();

  float gyro[3];
  int ret = mpu6050_read_raw(NULL, gyro);

  CHECK(ret == 0, "mpu6050_read_raw should return 0 when accel is NULL");
  CHECK(g_i2c_calls > 0, "I2C read should be attempted even with NULL accel");
  CHECK(gyro[0] != 0.0f || gyro[1] != 0.0f || gyro[2] != 0.0f,
        "gyro data should be populated");

  PASS("T-IMU-EXT-01: mpu6050_read_raw accepts NULL accel");
}

/* ------------------------------------------------------------------ */
/* T-IMU-EXT-02: mpu6050_read_raw with NULL gyro only                */
/* ------------------------------------------------------------------ */

void test_imu_null_gyro(void)
{
  setup();

  float accel[3];
  int ret = mpu6050_read_raw(accel, NULL);

  CHECK(ret == 0, "mpu6050_read_raw should return 0 when gyro is NULL");
  CHECK(g_i2c_calls > 0, "I2C read should be attempted even with NULL gyro");
  CHECK(accel[0] != 0.0f || accel[1] != 0.0f || accel[2] != 0.0f,
        "accel data should be populated");

  PASS("T-IMU-EXT-02: mpu6050_read_raw accepts NULL gyro");
}

/* ------------------------------------------------------------------ */
/* T-IMU-EXT-03: mpu6050_read_raw with both NULL (no-op)              */
/* ------------------------------------------------------------------ */

void test_imu_null_both(void)
{
  setup();

  int ret = mpu6050_read_raw(NULL, NULL);

  CHECK(ret == -1, "mpu6050_read_raw should return -1 when both params are NULL");
  CHECK(g_i2c_calls == 0, "I2C should not be called when both params are NULL");

  PASS("T-IMU-EXT-03: mpu6050_read_raw returns error when both pointers are NULL");
}

/* ------------------------------------------------------------------ */
/* T-DL-EXT-01: data_layer_write_imu with NULL att_rad                */
/* ------------------------------------------------------------------ */

void test_data_layer_null_att(void)
{
  setup();

  float rates[3] = {1.0f, 2.0f, 3.0f};
  uint32_t seq_before = data_layer_get_seq();

  data_layer_write_imu(NULL, rates);

  uint32_t seq_after = data_layer_get_seq();
  CHECK(seq_after == seq_before, "seq should not increment on NULL att_rad");

  dl_snapshot_t snap;
  data_layer_read(&snap);
  CHECK(!snap.state.imu_valid, "imu_valid should remain false on NULL att_rad");

  PASS("T-DL-EXT-01: data_layer_write_imu rejects NULL att_rad");
}

/* ------------------------------------------------------------------ */
/* T-DL-EXT-02: data_layer_write_imu with NULL rates_rad              */
/* ------------------------------------------------------------------ */

void test_data_layer_null_rates(void)
{
  setup();

  float att[3] = {0.1f, 0.2f, 0.3f};
  uint32_t seq_before = data_layer_get_seq();

  data_layer_write_imu(att, NULL);

  uint32_t seq_after = data_layer_get_seq();
  CHECK(seq_after == seq_before, "seq should not increment on NULL rates_rad");

  dl_snapshot_t snap;
  data_layer_read(&snap);
  CHECK(!snap.state.imu_valid, "imu_valid should remain false on NULL rates_rad");

  PASS("T-DL-EXT-02: data_layer_write_imu rejects NULL rates_rad");
}

/* ------------------------------------------------------------------ */
/* T-DL-EXT-03: data_layer_write_imu with both NULL                  */
/* ------------------------------------------------------------------ */

void test_data_layer_null_both(void)
{
  setup();

  uint32_t seq_before = data_layer_get_seq();

  data_layer_write_imu(NULL, NULL);

  uint32_t seq_after = data_layer_get_seq();
  CHECK(seq_after == seq_before, "seq should not increment on both NULL");

  PASS("T-DL-EXT-03: data_layer_write_imu handles both NULL");
}

/* ------------------------------------------------------------------ */
/* Main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
  printf("=== IMU Integration Tests ===\n\n");

  test_imu_null_accel();
  test_imu_null_gyro();
  test_imu_null_both();

  printf("\n=== Data Layer IMU Write Tests ===\n\n");

  test_data_layer_null_att();
  test_data_layer_null_rates();
  test_data_layer_null_both();

  printf("\n=== Summary ===\n");
  if (g_failures == 0)
  {
    printf("All tests PASSED\n");
    return 0;
  }
  else
  {
    printf("FAILURES: %d\n", g_failures);
    return 1;
  }
}
