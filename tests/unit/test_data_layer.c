/**
 * @file test_data_layer.c
 * @brief Data Layer unit tests - NULL parameter handling.
 *
 * Tests:
 *   T-DL-EXT-01 - data_layer_write_imu with NULL parameters
 *   T-DL-EXT-02 - data_layer_write_mag with NULL field_uT
 *   T-DL-EXT-03 - data_layer_write_ekf with NULL parameters
 *   T-DL-EXT-04 - Normal operation still works
 */

#include "data_layer.h"
#include "quaternion.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

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
  data_layer_init();
}

/* ------------------------------------------------------------------ */
/* T-DL-EXT-01: data_layer_write_imu NULL handling                    */
/* ------------------------------------------------------------------ */

void test_imu_null_params(void)
{
  setup();

  float att[3] = {0.1f, 0.2f, 0.3f};
  float rates[3] = {1.0f, 2.0f, 3.0f};
  uint32_t seq_base = data_layer_get_seq();

  data_layer_write_imu(NULL, rates);
  CHECK(data_layer_get_seq() == seq_base, "seq unchanged when att_rad is NULL");

  data_layer_write_imu(att, NULL);
  CHECK(data_layer_get_seq() == seq_base, "seq unchanged when rates_rad is NULL");

  data_layer_write_imu(NULL, NULL);
  CHECK(data_layer_get_seq() == seq_base, "seq unchanged when both are NULL");

  PASS("T-DL-EXT-01: data_layer_write_imu handles NULL params");
}

/* ------------------------------------------------------------------ */
/* T-DL-EXT-02: data_layer_write_mag NULL handling                    */
/* ------------------------------------------------------------------ */

void test_mag_null_params(void)
{
  setup();

  uint32_t seq_base = data_layer_get_seq();

  data_layer_write_mag(NULL);
  CHECK(data_layer_get_seq() == seq_base, "seq unchanged when field_uT is NULL");

  dl_snapshot_t snap;
  data_layer_read(&snap);
  CHECK(!snap.state.mag_valid, "mag_valid remains false when field_uT is NULL");

  PASS("T-DL-EXT-02: data_layer_write_mag handles NULL field_uT");
}

/* ------------------------------------------------------------------ */
/* T-DL-EXT-03: data_layer_write_ekf NULL handling                   */
/* ------------------------------------------------------------------ */

void test_ekf_null_params(void)
{
  setup();

  float q[4] = {1.0f, 0.0f, 0.0f, 0.0f};
  float bias[3] = {0.01f, -0.01f, 0.005f};
  float cov[7] = {0.1f, 0.1f, 0.1f, 0.1f, 0.01f, 0.01f, 0.01f};
  uint32_t seq_base = data_layer_get_seq();

  data_layer_write_ekf(NULL, bias, cov);
  CHECK(data_layer_get_seq() == seq_base, "seq unchanged when q is NULL");

  data_layer_write_ekf(q, NULL, cov);
  CHECK(data_layer_get_seq() == seq_base, "seq unchanged when bias is NULL");

  data_layer_write_ekf(q, bias, NULL);
  CHECK(data_layer_get_seq() == seq_base, "seq unchanged when cov_diag is NULL");

  data_layer_write_ekf(NULL, NULL, NULL);
  CHECK(data_layer_get_seq() == seq_base, "seq unchanged when all are NULL");

  PASS("T-DL-EXT-03: data_layer_write_ekf handles NULL params");
}

/* ------------------------------------------------------------------ */
/* T-DL-EXT-04: Normal operation verification                         */
/* ------------------------------------------------------------------ */

void test_normal_operation(void)
{
  setup();

  float att[3] = {0.1f, 0.2f, 0.3f};
  float rates[3] = {1.0f, 2.0f, 3.0f};

  uint32_t seq_before = data_layer_get_seq();
  data_layer_write_imu(att, rates);
  uint32_t seq_after = data_layer_get_seq();

  CHECK(seq_after == seq_before + 1, "seq increments on valid imu write");

  dl_snapshot_t snap;
  data_layer_read(&snap);
  CHECK(snap.state.imu_valid, "imu_valid set after valid write");
  CHECK(fabsf(snap.state.attitude[0] - att[0]) < 1e-5f, "attitude[0] matches");
  CHECK(fabsf(snap.state.attitude[1] - att[1]) < 1e-5f, "attitude[1] matches");
  CHECK(fabsf(snap.state.attitude[2] - att[2]) < 1e-5f, "attitude[2] matches");
  CHECK(fabsf(snap.state.rates[0] - rates[0]) < 1e-5f, "rates[0] matches");
  CHECK(fabsf(snap.state.rates[1] - rates[1]) < 1e-5f, "rates[1] matches");
  CHECK(fabsf(snap.state.rates[2] - rates[2]) < 1e-5f, "rates[2] matches");

  float mag[3] = {25.0f, 10.0f, 45.0f};
  seq_before = data_layer_get_seq();
  data_layer_write_mag(mag);
  seq_after = data_layer_get_seq();

  CHECK(seq_after == seq_before + 1, "seq increments on valid mag write");

  data_layer_read(&snap);
  CHECK(snap.state.mag_valid, "mag_valid set after valid write");
  CHECK(fabsf(snap.state.mag_field[0] - mag[0]) < 1e-5f, "mag_field[0] matches");
  CHECK(fabsf(snap.state.mag_field[1] - mag[1]) < 1e-5f, "mag_field[1] matches");
  CHECK(fabsf(snap.state.mag_field[2] - mag[2]) < 1e-5f, "mag_field[2] matches");

  float q[4] = {0.9659f, 0.2588f, 0.0f, 0.0f};
  float bias[3] = {0.01f, -0.01f, 0.005f};
  float cov[7] = {0.1f, 0.1f, 0.1f, 0.1f, 0.01f, 0.01f, 0.01f};
  seq_before = data_layer_get_seq();
  data_layer_write_ekf(q, bias, cov);
  seq_after = data_layer_get_seq();

  CHECK(seq_after == seq_before + 1, "seq increments on valid ekf write");

  PASS("T-DL-EXT-04: Normal operation works correctly");
}

/* ------------------------------------------------------------------ */
/* Main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
  printf("=== Data Layer NULL Parameter Tests ===\n\n");

  test_imu_null_params();
  test_mag_null_params();
  test_ekf_null_params();
  test_normal_operation();

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
