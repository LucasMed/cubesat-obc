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
#include <string.h>

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
/* T-DL-EXT-05..14: Coverage expansion — remaining setter/getter fns  */
/* ------------------------------------------------------------------ */

void test_write_radiation(void)
{
  setup();
  uint32_t seq = data_layer_get_seq();

  data_layer_write_radiation(0.5f);
  dl_snapshot_t snap;
  data_layer_read(&snap);
  CHECK(snap.state.radiation_dose == 0.5f, "radiation_dose stored");
  CHECK(data_layer_get_seq() == seq + 1, "seq increments");
  PASS("T-DL-EXT-05: data_layer_write_radiation");
}

void test_write_payload_status(void)
{
  setup();
  uint32_t seq = data_layer_get_seq();

  data_layer_write_payload_status(true, 42);
  dl_snapshot_t snap;
  data_layer_read(&snap);
  CHECK(snap.state.payload_rail_enabled == true, "rail_enabled");
  CHECK(snap.state.image_count == 42, "image_count");
  CHECK(data_layer_get_seq() == seq + 1, "seq increments");
  PASS("T-DL-EXT-06: data_layer_write_payload_status");
}

void test_flight_mode_roundtrip(void)
{
  setup();

  data_layer_set_flight_mode(FM_SAFE);
  CHECK(data_layer_get_flight_mode() == FM_SAFE, "FM_SAFE roundtrip");

  data_layer_set_flight_mode(FM_NOMINAL);
  dl_snapshot_t snap;
  data_layer_read(&snap);
  CHECK(snap.mode == FM_NOMINAL, "mode in snapshot");

  PASS("T-DL-EXT-07: data_layer_set/get_flight_mode");
}

void test_energy_state_roundtrip(void)
{
  setup();

  data_layer_set_energy_state(ENERGY_CRITICAL);
  dl_snapshot_t snap;
  data_layer_read(&snap);
  CHECK(snap.energy == ENERGY_CRITICAL, "ENERGY_CRITICAL stored");

  PASS("T-DL-EXT-08: data_layer_set_energy_state");
}

void test_seq_null_guard(void)
{
  /* data_layer_get_seq has no NULL guard, but test that it
   * returns a monotonic value after init. */
  data_layer_init();
  uint32_t s0 = data_layer_get_seq();
  data_layer_write_radiation(0.1f);
  CHECK(data_layer_get_seq() == s0 + 1, "seq monotonic after write");
  PASS("T-DL-EXT-09: data_layer_get_seq monotonic");
}

void test_mode_entry_tick_roundtrip(void)
{
  setup();

  data_layer_set_mode_entry_tick(12345);
  CHECK(data_layer_get_mode_entry_tick() == 12345, "mode_entry_tick roundtrip");

  data_layer_set_mode_entry_tick(0);
  CHECK(data_layer_get_mode_entry_tick() == 0, "reset to 0");
  PASS("T-DL-EXT-10: data_layer_set/get_mode_entry_tick");
}

void test_post_last_roundtrip(void)
{
  setup();

  post_record_t rec;
  memset(&rec, 0, sizeof(rec));
  rec.magic = 0xDEAD;
  rec.boot_count = 42;
  data_layer_set_post_last(&rec);
  post_record_t out;
  memset(&out, 0, sizeof(out));
  data_layer_get_post_last(&out);
  CHECK(out.magic == 0xDEAD, "post_last.magic");
  CHECK(out.boot_count == 42, "post_last.boot_count");
  PASS("T-DL-EXT-11: data_layer_set/get_post_last");
}

void test_post_last_null_guard(void)
{
  setup();
  uint32_t seq = data_layer_get_seq();

  data_layer_set_post_last(NULL);
  CHECK(data_layer_get_seq() == seq, "seq unchanged after set_post_last(NULL)");

  /* NULL in -> seq unchanged, no crash */
  uint32_t seq2 = data_layer_get_seq();
  CHECK(seq2 == seq, "seq unchanged after set_post_last(NULL)");

  post_record_t out;
  memset(&out, 0xAA, sizeof(out));
  data_layer_get_post_last(&out);
  /* Should not crash; output unchanged */
  PASS("T-DL-EXT-12: data_layer_*post_last NULL guards");
}

void test_gps_fix_roundtrip(void)
{
  setup();

  GpsFix_t fix = {.lat = -34.5f, .lon = -58.4f, .alt_m = 25.0f,
                   .satellites = 8, .hdop = 1.2f, .valid = true};
  data_layer_set_gps_fix(&fix);

  GpsFix_t out;
  memset(&out, 0, sizeof(out));
  data_layer_get_gps_fix(&out);
  CHECK(out.valid == true, "gps fix valid");
  CHECK(fabsf(out.lat - (-34.5f)) < 1e-5f, "gps lat");
  CHECK(out.satellites == 8, "gps sats");
  PASS("T-DL-EXT-13: data_layer_set/get_gps_fix");
}

void test_set_rtc_avail(void)
{
  setup();

  data_layer_set_rtc_avail(true);
  dl_snapshot_t snap;
  data_layer_read(&snap);
  CHECK(snap.state.rtc_available == true, "rtc_avail set");

  data_layer_set_rtc_avail(false);
  data_layer_read(&snap);
  CHECK(snap.state.rtc_available == false, "rtc_avail cleared");
  PASS("T-DL-EXT-20: data_layer_set_rtc_avail");
}

void test_deploy_in_progress_roundtrip(void)
{
  setup();

  uint32_t seq = data_layer_get_seq();
  data_layer_set_deploy_in_progress(true);
  CHECK(data_layer_get_deploy_in_progress() == true, "deploy_in_progress true");
  CHECK(data_layer_get_seq() == seq + 1, "seq increments");

  data_layer_set_deploy_in_progress(false);
  CHECK(data_layer_get_deploy_in_progress() == false, "deploy_in_progress false");
  PASS("T-DL-EXT-21: data_layer_set/get_deploy_in_progress");
}

void test_set_flight_mode_from_isr(void)
{
  setup();

  data_layer_set_flight_mode_from_isr(FM_DETUMBLE);
  dl_snapshot_t snap;
  data_layer_read(&snap);
  CHECK(snap.mode == FM_DETUMBLE, "FM_DETUMBLE from ISR-safe setter");

  PASS("T-DL-EXT-19: data_layer_set_flight_mode_from_isr");
}

void test_gps_fix_null_guard(void)
{
  setup();
  uint32_t seq = data_layer_get_seq();

  data_layer_set_gps_fix(NULL);
  CHECK(data_layer_get_seq() == seq, "seq unchanged after set_gps_fix(NULL)");

  GpsFix_t gfix;
  /* get_gps_fix with non-NULL should not crash even when nothing was set */
  memset(&gfix, 0, sizeof(gfix));
  data_layer_get_gps_fix(&gfix);
  PASS("T-DL-EXT-14: data_layer_*gps_fix NULL guards");
}

void test_set_sensor_avail(void)
{
  setup();

  data_layer_set_sensor_avail(true, false);
  dl_snapshot_t snap;
  data_layer_read(&snap);
  CHECK(snap.state.imu_available == true, "imu_avail set");
  CHECK(snap.state.temp_available == false, "temp_avail not set");

  data_layer_set_sensor_avail(false, true);
  data_layer_read(&snap);
  CHECK(snap.state.imu_available == false, "imu_avail cleared");
  CHECK(snap.state.temp_available == true, "temp_avail set");
  PASS("T-DL-EXT-15: data_layer_set_sensor_avail");
}

void test_set_mag_avail(void)
{
  setup();

  data_layer_set_mag_avail(true);
  dl_snapshot_t snap;
  data_layer_read(&snap);
  CHECK(snap.state.mag_available == true, "mag_avail set");

  data_layer_set_mag_avail(false);
  data_layer_read(&snap);
  CHECK(snap.state.mag_available == false, "mag_avail cleared");
  PASS("T-DL-EXT-16: data_layer_set_mag_avail");
}

void test_write_rtc(void)
{
  setup();
  uint32_t seq = data_layer_get_seq();

  data_layer_write_rtc(1700000000);
  dl_snapshot_t snap;
  data_layer_read(&snap);
  CHECK(snap.state.rtc_timestamp == 1700000000u, "rtc timestamp");
  CHECK(snap.state.rtc_valid == true, "rtc_valid set");
  CHECK(data_layer_get_seq() == seq + 1, "seq increments");

  data_layer_write_rtc(0);
  data_layer_read(&snap);
  CHECK(snap.state.rtc_valid == false, "rtc_valid false when timestamp == 0");
  PASS("T-DL-EXT-17: data_layer_write_rtc");
}

void test_write_lux(void)
{
  setup();
  uint32_t seq = data_layer_get_seq();

  data_layer_write_lux(450.0f);
  dl_snapshot_t snap;
  data_layer_read(&snap);
  CHECK(snap.state.lux == 450.0f, "lux stored");
  CHECK(snap.state.lux_valid == true, "lux_valid set");
  CHECK(snap.state.lux_available == true, "lux_available set");
  CHECK(data_layer_get_seq() == seq + 1, "seq increments");

  data_layer_write_lux(-1.0f);
  data_layer_read(&snap);
  CHECK(snap.state.lux_valid == false, "lux_valid false when lux < 0");
  PASS("T-DL-EXT-18: data_layer_write_lux");
}

void test_read_null(void)
{
  setup();
  /* data_layer_read(NULL) — must return silently (L104 in data_layer.c) */
  data_layer_read(NULL);
  /* Must reach here without crash */
  PASS("T-DL-EXT-22: data_layer_read(NULL) safe");
}

void test_get_post_last_null(void)
{
  setup();
  /* data_layer_get_post_last(NULL) — must return silently (L426 in data_layer.c) */
  data_layer_get_post_last(NULL);
  PASS("T-DL-EXT-23: data_layer_get_post_last(NULL) safe");
}

void test_get_gps_fix_null(void)
{
  setup();
  /* data_layer_get_gps_fix(NULL) — must return silently (L481 in data_layer.c) */
  data_layer_get_gps_fix(NULL);
  PASS("T-DL-EXT-24: data_layer_get_gps_fix(NULL) safe");
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

  printf("\n--- Coverage Expansion ---\n");
  test_write_radiation();
  test_write_payload_status();
  test_flight_mode_roundtrip();
  test_energy_state_roundtrip();
  test_seq_null_guard();
  test_mode_entry_tick_roundtrip();
  test_post_last_roundtrip();
  test_post_last_null_guard();
  test_gps_fix_roundtrip();
  test_gps_fix_null_guard();
  test_set_sensor_avail();
  test_set_mag_avail();
  test_write_rtc();
  test_write_lux();
  test_set_rtc_avail();
  test_deploy_in_progress_roundtrip();
  test_set_flight_mode_from_isr();

  /* NULL-getter coverage */
  test_read_null();
  test_get_post_last_null();
  test_get_gps_fix_null();

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
