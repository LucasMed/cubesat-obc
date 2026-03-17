// test_gps.c -- Unit tests for GPS stub driver
// All comments in English

#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include "gps_stub.h"

void test_gps_init_and_deinit() {
    assert(gps_init());
    gps_deinit();
}

void test_gps_mode_and_fix() {
    gps_mock_set_mode(GPS_MOCK_OK);
    GpsFix_t* fix = gps_read_fix();
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

void test_gps_satellite_count() {
    GpsFix_t inject = {
        .lat = 0,
        .lon = 0,
        .alt_m = 0,
        .utc_time = 100,
        .valid = true,
        .timestamp_ms = 100,
        .satellites_in_view = 5
    };
    gps_mock_set_data(&inject);
    assert(gps_get_satellites_in_view() == 5);
}

void test_gps_call_count_and_reset() {
    gps_mock_reset_counters();
    gps_init(); gps_deinit(); gps_read_fix(); gps_get_last_fix(); gps_is_fix_valid(); gps_get_satellites_in_view(); gps_mock_set_data(&(GpsFix_t){}); gps_mock_set_mode(GPS_MOCK_OK);
    assert(gps_mock_get_call_count("gps_init") == 1);
    assert(gps_mock_get_call_count("gps_deinit") == 1);
    assert(gps_mock_get_call_count("gps_read_fix") == 1);
    assert(gps_mock_get_call_count("gps_get_last_fix") == 1);
    assert(gps_mock_get_call_count("gps_is_fix_valid") == 1);
    assert(gps_mock_get_call_count("gps_get_satellites_in_view") == 1);
    assert(gps_mock_get_call_count("gps_mock_set_data") == 1);
    assert(gps_mock_get_call_count("gps_mock_set_mode") == 1);
}

int main(void) {
    printf("Testing GPS stub...\n");
    test_gps_init_and_deinit();
    test_gps_mode_and_fix();
    test_gps_satellite_count();
    test_gps_call_count_and_reset();
    printf("All GPS stub tests passed.\n");
    return 0;
}
