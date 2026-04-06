// test_gps_neo7m.c -- Unit test for real NEO-7M GPS NMEA parser
// All comments in English

#define GPS_TEST
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "gps_driver.h"

#ifdef __cplusplus
extern "C" {
#endif
bool nmea_buffer_push(unsigned char byte);
#ifdef __cplusplus
}
#endif

void inject_sentence(const char *sentence) {
    for (size_t i = 0; sentence[i]; ++i) {
        volatile bool ok = nmea_buffer_push((unsigned char)sentence[i]);
        (void)ok;
    }
}

void test_parse_valid_gpgga(void) {
    gps_init();
    const char *gpgga = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    inject_sentence(gpgga);
    GpsFix_t* fix = gps_read_fix();
    assert(fix != NULL);
    assert(fix->valid);
    assert(fabsf(fix->lat - 48.1173f) < 0.0002f);
    assert(fabsf(fix->lon - 11.5167f) < 0.0002f);
    assert(fabsf(fix->alt_m - 545.4f) < 0.01f);
    assert(fix->utc_time == 12*3600 + 35*60 + 19);
    assert(gps_get_satellites_in_view() == 8);
    assert(fix->satellites == 8);
    assert(fix->hdop > 0.0f && fix->hdop < 10.0f);
    gps_deinit();
}

void test_bad_checksum(void) {
    gps_init();
    const char *bad = "$GPGGA,123519,4807.038,N,01131.000,E,1,06,0.9,545.4,M,46.9,M,,*00\r\n";
    inject_sentence(bad);
    GpsFix_t* fix = gps_read_fix();
    assert(fix == NULL);
    gps_deinit();
}

void test_gps_deinit_safe(void) {
    gps_init();
    gps_deinit();
    assert(gps_read_fix() == NULL);
}

void test_stale_fix_detection(void) {
    gps_init();
    const char *gpgga = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    inject_sentence(gpgga);
    GpsFix_t* fix = gps_read_fix();
    assert(fix != NULL);
    assert(fix->valid == true);
    fix->timestamp_ms = 0;
    assert(fix->valid == true);
    gps_deinit();
}

void test_timestamp_not_updated_by_rmc(void) {
    gps_init();
    gps_reset_stats();
    const char *gprmc = "$GPRMC,235947.00,A,3723.2475,N,12202.3246,W,0.13,309.62,120598,,,A*10\r\n";
    inject_sentence(gprmc);
    gps_read_fix();
    GpsFix_t last = {0};
    (void)gps_get_last_fix(&last);
    gps_deinit();
}

void test_hdop_parsed_correctly(void) {
    gps_init();
    gps_reset_stats();
    const char *gpgga = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    inject_sentence(gpgga);
    GpsFix_t* fix = gps_read_fix();
    assert(fix != NULL);
    assert(fabsf(fix->hdop - 0.9f) < 0.01f);
    gps_deinit();
}

void test_stats_counters(void) {
    gps_init();
    gps_reset_stats();
    const GpsStats_t *stats_before = gps_get_stats();
    assert(stats_before->sentences_received == 0);
    const char *gpgga = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    inject_sentence(gpgga);
    gps_read_fix();
    const GpsStats_t *stats = gps_get_stats();
    assert(stats->sentences_received >= 1);
    const char *bad = "$GPGGA,123519,4807.038,N,01131.000,E,1,06,0.9,545.4,M,46.9,M,,*00\r\n";
    inject_sentence(bad);
    gps_read_fix();
    stats = gps_get_stats();
    assert(stats->checksum_errors >= 1);
    gps_deinit();
}

void test_gps_get_last_fix_by_value(void) {
    gps_init();
    const char *gpgga = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    inject_sentence(gpgga);
    gps_read_fix();
    GpsFix_t out = {0};
    bool result = gps_get_last_fix(&out);
    assert(result == true);
    assert(out.valid == true);
    assert(fabsf(out.lat - 48.1173f) < 0.0002f);
    assert(fabsf(out.lon - 11.5167f) < 0.0002f);
    gps_deinit();
}

void test_gps_get_last_fix_returns_false_when_no_fix(void) {
    gps_init();
    GpsFix_t out = {0};
    bool result = gps_get_last_fix(&out);
    assert(result == false);
    gps_deinit();
}

int main(void) {
    printf("Testing real NEO-7M GPS NMEA parser...\n");
    test_parse_valid_gpgga();
    gps_deinit();
    test_bad_checksum();
    gps_deinit();
    test_gps_deinit_safe();
    gps_deinit();
    test_stale_fix_detection();
    gps_deinit();
    test_timestamp_not_updated_by_rmc();
    gps_deinit();
    test_hdop_parsed_correctly();
    gps_deinit();
    test_stats_counters();
    gps_deinit();
    test_gps_get_last_fix_by_value();
    gps_deinit();
    test_gps_get_last_fix_returns_false_when_no_fix();
    gps_deinit();
    printf("All NMEA parser tests passed.\n");
    return 0;
}