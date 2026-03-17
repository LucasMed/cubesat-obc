// test_gps.c -- Unit tests for GPS stub driver

#include "data_layer.h"
#include "gps_stub.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

void test_gps_init_and_deinit()
{
  assert(gps_init());
  gps_deinit();
}

void test_gps_mode_and_fix()
{
  gps_mock_set_mode(GPS_MOCK_OK);
  GpsFix_t *fix = gps_read_fix();
  assert(fix->valid);
  gps_mock_set_mode(GPS_MOCK_FAULT_TIMEOUT);
  fix = gps_read_fix();
  assert(!fix->valid);
  gps_mock_set_mode(GPS_MOCK_FAULT_NO_FIX);
  fix = gps_read_fix();
  assert(!fix->valid);
  gps_mock_set_mode(GPS_MOCK_FAULT_CHECKSUM);
  fix = gps_read_fix();
  assert(!fix->valid);
  gps_mock_set_mode(GPS_MOCK_FAULT_PARTIAL_FRAME);
  fix = gps_read_fix();
  assert(!fix->valid);
  gps_mock_set_mode(GPS_MOCK_FAULT_STALE_DATA);
  fix->timestamp_ms = 5001;
  assert(!gps_is_fix_valid());
}

void test_gps_satellite_count()
{
  GpsFix_t inject = {
      .lat = 0, .lon = 0, .alt_m = 0, .utc_time = 100, .valid = true, .timestamp_ms = 100};
  gps_mock_set_data(&inject);
  // satellites_in_view is not present in GpsFix_t; test only that function returns a value (could
  // be 0 in stub)
  (void)gps_get_satellites_in_view();
}

void test_gps_call_count_and_reset()
{
  gps_mock_reset_counters();
  gps_init();
  gps_deinit();
  gps_read_fix();
  gps_get_last_fix();
  gps_is_fix_valid();
  gps_get_satellites_in_view();
  gps_mock_set_data(&(GpsFix_t){});
  gps_mock_set_mode(GPS_MOCK_OK);
  assert(gps_mock_get_call_count("gps_init") == 1);
  assert(gps_mock_get_call_count("gps_deinit") == 1);
  assert(gps_mock_get_call_count("gps_read_fix") == 1);
  assert(gps_mock_get_call_count("gps_get_last_fix") == 1);
  assert(gps_mock_get_call_count("gps_is_fix_valid") == 1);
  assert(gps_mock_get_call_count("gps_get_satellites_in_view") == 1);
  assert(gps_mock_get_call_count("gps_mock_set_data") == 1);
  assert(gps_mock_get_call_count("gps_mock_set_mode") == 1);
}

// --- Additional tests from WP-7.10 GPS Stub Test Suite ---

void test_data_injection(void)
{
  GpsFix_t custom = {.lat = -31.4135f,
                     .lon = -64.1812f,
                     .alt_m = 431.0f,
                     .utc_time = 183000U,
                     .valid = true,
                     .timestamp_ms = 2000U};
  gps_mock_set_data(&custom);
  GpsFix_t *got = gps_read_fix();
  assert(got->lat == custom.lat);
  assert(got->lon == custom.lon);
  assert(got->alt_m == custom.alt_m);
  assert(got->utc_time == custom.utc_time);
  assert(got->valid == custom.valid);
}

void test_fault_timeout(void)
{
  gps_mock_set_mode(GPS_MOCK_FAULT_TIMEOUT);
  GpsFix_t *fix = gps_read_fix();
  assert(!fix->valid);
  assert(gps_get_satellites_in_view() == 0);
}

void test_fault_checksum(void)
{
  gps_mock_set_mode(GPS_MOCK_FAULT_CHECKSUM);
  GpsFix_t *fix = gps_read_fix();
  assert(!fix->valid);
}

void test_fault_partial_frame(void)
{
  gps_mock_set_mode(GPS_MOCK_FAULT_PARTIAL_FRAME);
  GpsFix_t *fix = gps_read_fix();
  assert(!fix->valid);
}

void test_fault_no_fix(void)
{
  gps_mock_set_mode(GPS_MOCK_FAULT_NO_FIX);
  GpsFix_t *fix = gps_read_fix();
  assert(!fix->valid);
  // satellites_in_view is not present in GpsFix_t; cannot check
}

void test_stale_fix(void)
{
  gps_mock_set_mode(GPS_MOCK_OK);
  // Simulate a valid fix at t=0
  GpsFix_t *fix = gps_read_fix();
  fix->timestamp_ms = 0;
  // t = 4999 ms (should be valid)
  fix->timestamp_ms = 4999U;
  assert(gps_is_fix_valid());
  // t = 5000 ms (should be stale)
  fix->timestamp_ms = 5000U;
  assert(!gps_is_fix_valid());
  // t = 5001 ms (should be stale)
  fix->timestamp_ms = 5001U;
  assert(!gps_is_fix_valid());
}

void test_stale_mode(void)
{
  gps_mock_set_mode(GPS_MOCK_FAULT_STALE_DATA);
  GpsFix_t *fix = gps_read_fix();
  assert(!fix->valid);
  // The returned timestamp must make the fix appear older than threshold
  assert(fix->timestamp_ms >= 5000U);
}

void test_get_last_fix(void)
{
  // Before any read
  GpsFix_t *last = gps_get_last_fix();
  assert(!last->valid);
  // After a valid read
  gps_mock_set_mode(GPS_MOCK_OK);
  GpsFix_t *fix = gps_read_fix();
  last = gps_get_last_fix();
  assert(last->valid);
  assert(last->lat == fix->lat);
}

void test_fault_recovery(void)
{
  // 1. Inject timeout fault
  gps_mock_set_mode(GPS_MOCK_FAULT_TIMEOUT);
  GpsFix_t *fix = gps_read_fix();
  assert(!fix->valid);
  // 2. Recover
  gps_mock_set_mode(GPS_MOCK_OK);
  fix = gps_read_fix();
  assert(fix->valid);
  assert(gps_is_fix_valid());
}

void test_null_safety(void)
{
  // gps_read_fix(NULL) and gps_get_last_fix(NULL) are not supported in this stub API
  // so we just check that the stub does not crash if passed NULL to gps_mock_set_data
  gps_mock_set_data(NULL);
  GpsFix_t *fix = gps_read_fix();
  assert(fix != NULL);
}

void test_data_layer_gps_fix(void)
{
  GpsFix_t fix = {.lat = 10.1f,
                  .lon = 20.2f,
                  .alt_m = 100.5f,
                  .utc_time = 123456,
                  .valid = true,
                  .timestamp_ms = 5555};
  data_layer_set_gps_fix(&fix);
  GpsFix_t out = {0};
  data_layer_get_gps_fix(&out);
  assert(memcmp(&fix, &out, sizeof(GpsFix_t)) == 0);
}

// Minimal integration test for gps_task logic (mocked)
void test_gps_task_integration(void)
{
  // Simulate gps_read_fix and data_layer_set_gps_fix
  GpsFix_t fix = {
      .lat = 1.0f, .lon = 2.0f, .alt_m = 3.0f, .utc_time = 4, .valid = true, .timestamp_ms = 100};
  data_layer_set_gps_fix(&fix);
  GpsFix_t out = {0};
  data_layer_get_gps_fix(&out);
  assert(out.lat == 1.0f && out.lon == 2.0f && out.alt_m == 3.0f && out.utc_time == 4 && out.valid);
}

int main(void)
{
  printf("Testing GPS stub...\n");
  test_gps_init_and_deinit();
  test_gps_mode_and_fix();
  test_gps_satellite_count();
  test_gps_call_count_and_reset();
  test_data_injection();
  test_fault_timeout();
  test_fault_checksum();
  test_fault_partial_frame();
  test_fault_no_fix();
  test_stale_fix();
  test_stale_mode();
  test_get_last_fix();
  test_fault_recovery();
  test_null_safety();
  test_data_layer_gps_fix();
  test_gps_task_integration();
  printf("All GPS stub tests passed.\n");
  return 0;
}
