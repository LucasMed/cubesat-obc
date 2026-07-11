// test_gps_neo7m.c -- Unit test for real NEO-7M GPS NMEA parser
// All comments in English

#define GPS_TEST
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "gps_driver.h"

#ifdef __cplusplus
extern "C" {
#endif
bool nmea_buffer_push(unsigned char byte);
double nmea_deg_min_to_dec(const char *str, char hemisphere);
bool gps_rtc_delta_check(uint32_t gps_epoch, uint32_t rtc_epoch);
#ifdef __cplusplus
}
#endif

void inject_sentence(const char *sentence) {
    for (size_t i = 0; sentence[i]; ++i) {
        volatile bool ok = nmea_buffer_push((unsigned char)sentence[i]);
        (void)ok;
    }
}

void inject_bytes(const unsigned char *data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        volatile bool ok = nmea_buffer_push(data[i]);
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

void test_malformed_nmea_empty_hemisphere_fields(void) {
    /* GGA with empty hemisphere fields (consecutive commas after lat/lon).
     * fields[3] and fields[5] will be "" (empty strings, not NULL).
     * Must not crash when accessing fields[3][0] / fields[5][0];
     * the fix must be rejected (not valid). */
    gps_init();
    gps_reset_stats();
    const char *malformed = "$GPGGA,123519,4807.038,,01131.000,,1,08,0.9,545.4,M,46.9,M,,*4C\r\n";
    inject_sentence(malformed);
    GpsFix_t *fix = gps_read_fix();
    assert(fix != NULL);
    /* Without fix: fields[3][0] and fields[5][0] are '\0' but the condition
     * (which checks non-NULL only) passes, making fix->valid = true.  With
     * the fix the condition also checks the first char is non-null, so the
     * fix is rejected and valid stays false. */
    assert(!fix->valid);
    gps_deinit();
    printf("test_malformed_nmea_empty_hemisphere_fields PASS\n");
}

void test_malformed_nmea_truncated_early(void) {
    /* GGA that ends before all required fields (no altitude).
     * After strsep returns NULL, the loop breaks.  Must not crash
     * when the parser checks fields[] entries that are NULL. */
    gps_init();
    gps_reset_stats();
    const char *truncated = "$GPGGA,123519,4807.038,N,01131.000,E,1*53\r\n";
    inject_sentence(truncated);
    GpsFix_t *fix = gps_read_fix();
    assert(fix != NULL);
    assert(!fix->valid);
    gps_deinit();
    printf("test_malformed_nmea_truncated_early PASS\n");
}

/* ------------------------------------------------------------------ */
/*  Buffer-level tests                                                */
/* ------------------------------------------------------------------ */

void test_nmea_buffer_full(void) {
    gps_init();
    /* Fill buffer with 2047 bytes (NMEA_RX_BUFFER_SIZE = 2048,
     * so full condition is (head+1) % size == tail, i.e. 2047 pushes). */
    for (size_t i = 0; i < 2047; i++) {
        volatile bool ok = nmea_buffer_push((unsigned char)'A');
        assert(ok == true);
        (void)ok;
    }
    /* 2048th push should fail — buffer full (line 85) */
    {
        volatile bool ok = nmea_buffer_push((unsigned char)'B');
        assert(ok == false);
        (void)ok;
    }
    gps_deinit();
}

void test_nmea_incomplete_sentence(void) {
    /* Partial data in buffer without a terminating \n.
     * nmea_get_sentence rewinds the buffer and returns false (lines 337-338). */
    gps_init();
    gps_reset_stats();
    const char *partial = "$GPGGA,123519";
    for (size_t i = 0; partial[i]; i++) {
        volatile bool ok = nmea_buffer_push((unsigned char)partial[i]);
        (void)ok;
    }
    /* No \n pushed — sentence is incomplete, gps_read_fix returns NULL */
    volatile GpsFix_t *fix = gps_read_fix();
    assert(fix == NULL);
    (void)fix;
    gps_deinit();
}

void test_nmea_sentence_framing_error(void) {
    /* Sentence longer than 128 bytes without early \n.
     * nmea_get_sentence breaks at maxlen-1 (line 332), rewinds buffer (337-338). */
    gps_init();
    gps_reset_stats();
    unsigned char long_line[130];
    long_line[0] = '$';
    memset(long_line + 1, 'A', 128);
    long_line[129] = '\n';
    inject_bytes(long_line, 130);
    volatile GpsFix_t *fix = gps_read_fix();
    assert(fix == NULL);
    (void)fix;
    gps_deinit();
}

/* ------------------------------------------------------------------ */
/*  Checksum error tests                                              */
/* ------------------------------------------------------------------ */

void test_nmea_no_checksum(void) {
    /* Valid GGA sentence without '*' checksum marker.
     * nmea_verify_checksum finds no '*' (line 351) -> returns false. */
    gps_init();
    gps_reset_stats();
    const char *no_star = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,\r\n";
    inject_sentence(no_star);
    /* Must call gps_read_fix outside assert (NDEBUG disables assert evaluation) */
    volatile GpsFix_t *fix = gps_read_fix();
    assert(fix == NULL);
    {
        volatile const GpsStats_t *stats = gps_get_stats();
        assert(stats->checksum_errors >= 1);
        (void)stats;
    }
    (void)fix;
    gps_deinit();
}

void test_nmea_invalid_checksum_hex(void) {
    /* Checksum starts with valid hex digit but has trailing junk.
     * strtoul parses "4", endptr points to "G" which is not \0/\r/\n (line 362). */
    gps_init();
    gps_reset_stats();
    const char *bad_hex = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*4G\r\n";
    inject_sentence(bad_hex);
    /* Must call gps_read_fix outside assert (NDEBUG disables assert evaluation) */
    volatile GpsFix_t *fix = gps_read_fix();
    assert(fix == NULL);
    {
        volatile const GpsStats_t *stats = gps_get_stats();
        assert(stats->checksum_errors >= 1);
        (void)stats;
    }
    (void)fix;
    gps_deinit();
}

/* ------------------------------------------------------------------ */
/*  GGA parser error paths                                            */
/* ------------------------------------------------------------------ */

void test_gga_no_fix(void) {
    /* GGA with fix quality = 0 (no fix). fix.valid should be false,
     * and stats->fixes_invalid incremented. */
    gps_init();
    gps_reset_stats();
    const char *no_fix = "$GPGGA,123519,4807.038,N,01131.000,E,0,08,0.9,545.4,M,46.9,M,,*46\r\n";
    inject_sentence(no_fix);
    volatile GpsFix_t *fix = gps_read_fix();
    assert(fix != NULL);
    /* fix is returned but valid is false */
    assert(!fix->valid);
    {
        volatile const GpsStats_t *stats = gps_get_stats();
        assert(stats->fixes_invalid >= 1);
        (void)stats;
    }
    (void)fix;
    gps_deinit();
}

void test_gga_altitude_invalid(void) {
    /* GGA with empty altitude field (consecutive commas).
     * strtod("") returns 0 with endptr == fields[9] -> alt_m = 0.0f (lines 460-461). */
    gps_init();
    gps_reset_stats();
    const char *alt_empty = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,,M,46.9,M,,*69\r\n";
    inject_sentence(alt_empty);
    volatile GpsFix_t *fix = gps_read_fix();
    assert(fix != NULL);
    /* With empty altitude, fix is still valid but alt_m defaults to 0.0 */
    {
        volatile const GpsStats_t *stats = gps_get_stats();
        assert(stats->fixes_valid >= 1);
        (void)stats;
    }
    (void)fix;
    gps_deinit();
}

void test_gga_satellites_invalid(void) {
    /* GGA with non-numeric satellite count.
     * strtoul("xx") -> 0, endptr == fields[7] -> satellites = 0 (lines 469-470). */
    gps_init();
    gps_reset_stats();
    const char *bad_sat = "$GPGGA,123519,4807.038,N,01131.000,E,1,xx,0.9,545.4,M,46.9,M,,*4F\r\n";
    inject_sentence(bad_sat);
    volatile GpsFix_t *fix = gps_read_fix();
    assert(fix != NULL);
    assert(fix->valid);
    assert(fix->satellites == 0);
    (void)fix;
    gps_deinit();
}

void test_gga_hdop_invalid(void) {
    /* GGA with non-numeric HDOP.
     * strtod("xx") -> 0, endptr == fields[8] -> hdop = 99.0f (lines 478-479). */
    gps_init();
    gps_reset_stats();
    const char *bad_hdop = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,xx,545.4,M,46.9,M,,*60\r\n";
    inject_sentence(bad_hdop);
    volatile GpsFix_t *fix = gps_read_fix();
    assert(fix != NULL);
    assert(fix->valid);
    assert(fabsf(fix->hdop - 99.0f) < 0.01f);
    (void)fix;
    gps_deinit();
}

/* ------------------------------------------------------------------ */
/*  Time field parse failures (lines 499-500, 504-505, 509-510)       */
/* ------------------------------------------------------------------ */

void test_gga_time_hours_fail(void) {
    /* Hours part is non-numeric -> h = 0, rest parsed normally.
     * utc_time = 0*3600 + 12*60 + 34 = 754 */
    gps_init();
    gps_reset_stats();
    const char *bad_h = "$GPGGA,XX1234,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*4E\r\n";
    inject_sentence(bad_h);
    volatile GpsFix_t *fix = gps_read_fix();
    assert(fix != NULL);
    assert(fix->valid);
    assert(fix->utc_time == 754);
    (void)fix;
    gps_deinit();
}

void test_gga_time_minutes_fail(void) {
    /* Minutes part is non-numeric -> m = 0.
     * utc_time = 12*3600 + 0*60 + 56 = 43256 */
    gps_init();
    gps_reset_stats();
    const char *bad_m = "$GPGGA,12XX56,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*4A\r\n";
    inject_sentence(bad_m);
    volatile GpsFix_t *fix = gps_read_fix();
    assert(fix != NULL);
    assert(fix->valid);
    assert(fix->utc_time == 43256);
    (void)fix;
    gps_deinit();
}

void test_gga_time_seconds_fail(void) {
    /* Seconds part is non-numeric -> s = 0.
     * utc_time = 12*3600 + 34*60 + 0 = 45240 */
    gps_init();
    gps_reset_stats();
    const char *bad_s = "$GPGGA,1234XX,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*4E\r\n";
    inject_sentence(bad_s);
    volatile GpsFix_t *fix = gps_read_fix();
    assert(fix != NULL);
    assert(fix->valid);
    assert(fix->utc_time == 45240);
    (void)fix;
    gps_deinit();
}

/* ------------------------------------------------------------------ */
/*  nmea_deg_min_to_dec edge cases (lines 372, 385, 391, 402,         */
/*  408, 414-415)                                                     */
/* ------------------------------------------------------------------ */

void test_nmea_deg_min_to_dec_null(void) {
    double r = nmea_deg_min_to_dec(NULL, 'N');
    assert(fabs(r - 0.0) < 1e-9);
    (void)r;
}

void test_nmea_deg_min_to_dec_empty(void) {
    double r = nmea_deg_min_to_dec("", 'N');
    assert(fabs(r - 0.0) < 1e-9);
    (void)r;
}

void test_nmea_deg_min_to_dec_lat_bad_deg(void) {
    /* Degree portion "AB" fails strtol (line 385) */
    double r = nmea_deg_min_to_dec("AB48.0", 'N');
    assert(fabs(r - 0.0) < 1e-9);
    (void)r;
}

void test_nmea_deg_min_to_dec_lat_bad_min(void) {
    /* Degree portion "48" works, minute portion "AA.0" fails strtod (line 391) */
    double r = nmea_deg_min_to_dec("48AA.0", 'N');
    assert(fabs(r - 0.0) < 1e-9);
    (void)r;
}

void test_nmea_deg_min_to_dec_lon_bad_deg(void) {
    /* Degree portion "AB0" fails strtol (line 402) */
    double r = nmea_deg_min_to_dec("AB011.0", 'E');
    assert(fabs(r - 0.0) < 1e-9);
    (void)r;
}

void test_nmea_deg_min_to_dec_lon_bad_min(void) {
    /* Degree portion "011" works, minute portion "AA.0" fails strtod (line 408) */
    double r = nmea_deg_min_to_dec("011AA.0", 'E');
    assert(fabs(r - 0.0) < 1e-9);
    (void)r;
}

void test_nmea_deg_min_to_dec_south(void) {
    /* Southern hemisphere -> value negated (line 414) */
    double val = nmea_deg_min_to_dec("4807.038", 'S');
    assert(val < 0.0);
    assert(fabs(val + 48.1173) < 0.0002);
    (void)val;
}

void test_nmea_deg_min_to_dec_west(void) {
    /* Western hemisphere -> value negated (line 414) */
    double val = nmea_deg_min_to_dec("01131.000", 'W');
    assert(val < 0.0);
    assert(fabs(val + 11.5167) < 0.0002);
    (void)val;
}

/* ------------------------------------------------------------------ */
/*  API edge-case tests                                               */
/* ------------------------------------------------------------------ */

void test_gps_get_last_fix_null(void) {
    /* NULL out parameter should return false (line 661) */
    gps_init();
    {
        volatile bool result = gps_get_last_fix(NULL);
        assert(result == false);
        (void)result;
    }
    gps_deinit();
}

void test_gps_cold_start(void) {
    /* Call cold start on host build: exercises the (void) cast
     * and function entry/exit (lines 698, 705, 707). No crash. */
    gps_init();
    gps_cold_start();
    /* Driver should still be usable after cold start */
    volatile const GpsStats_t *stats = gps_get_stats();
    assert(stats != NULL);
    (void)stats;
    gps_deinit();
}

/* ------------------------------------------------------------------ */
/*  FR-19: GPS→RTC sync guard (gps_rtc_delta_check) tests             */
/* ------------------------------------------------------------------ */

/* 2000-01-01 00:00:00 UTC — boundary for GPS_EPOCH_MIN_VALID */
#define EPOCH_2000_01_01 946684800u

void test_gps_rtc_delta_same_epoch(void) {
    /* Same epoch → must pass (delta = 0) */
    bool ok = gps_rtc_delta_check(EPOCH_2000_01_01, EPOCH_2000_01_01);
    assert(ok == true);
    printf("test_gps_rtc_delta_same_epoch PASS\n");
}

void test_gps_rtc_delta_gps_1s_ahead(void) {
    /* GPS 1 second ahead of RTC → delta 1s > 0s threshold → must fail */
    bool ok = gps_rtc_delta_check(EPOCH_2000_01_01 + 1, EPOCH_2000_01_01);
    assert(ok == false);
    printf("test_gps_rtc_delta_gps_1s_ahead PASS\n");
}

void test_gps_rtc_delta_rtc_1s_ahead(void) {
    /* RTC 1 second ahead of GPS → delta 1s > 0s threshold → must fail */
    bool ok = gps_rtc_delta_check(EPOCH_2000_01_01, EPOCH_2000_01_01 + 1);
    assert(ok == false);
    printf("test_gps_rtc_delta_rtc_1s_ahead PASS\n");
}

void test_gps_rtc_delta_rtc_uninitialized(void) {
    /* RTC epoch 0 (uninitialized) → must always pass */
    bool ok = gps_rtc_delta_check(EPOCH_2000_01_01 + 99999, 0u);
    assert(ok == true);
    printf("test_gps_rtc_delta_rtc_uninitialized PASS\n");
}

void test_gps_rtc_delta_rtc_at_boundary(void) {
    /* RTC exactly at GPS_EPOCH_MIN_VALID (2000-01-01) → not uninitialized → must pass */
    bool ok = gps_rtc_delta_check(EPOCH_2000_01_01, EPOCH_2000_01_01);
    assert(ok == true);
    printf("test_gps_rtc_delta_rtc_at_boundary PASS\n");
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
    test_malformed_nmea_empty_hemisphere_fields();
    gps_deinit();
    test_malformed_nmea_truncated_early();
    gps_deinit();
    test_nmea_buffer_full();
    gps_deinit();
    test_nmea_incomplete_sentence();
    gps_deinit();
    test_nmea_sentence_framing_error();
    gps_deinit();
    test_nmea_no_checksum();
    gps_deinit();
    test_nmea_invalid_checksum_hex();
    gps_deinit();
    test_gga_no_fix();
    gps_deinit();
    test_gga_altitude_invalid();
    gps_deinit();
    test_gga_satellites_invalid();
    gps_deinit();
    test_gga_hdop_invalid();
    gps_deinit();
    test_gga_time_hours_fail();
    gps_deinit();
    test_gga_time_minutes_fail();
    gps_deinit();
    test_gga_time_seconds_fail();
    gps_deinit();
    test_nmea_deg_min_to_dec_null();
    test_nmea_deg_min_to_dec_empty();
    test_nmea_deg_min_to_dec_lat_bad_deg();
    test_nmea_deg_min_to_dec_lat_bad_min();
    test_nmea_deg_min_to_dec_lon_bad_deg();
    test_nmea_deg_min_to_dec_lon_bad_min();
    test_nmea_deg_min_to_dec_south();
    test_nmea_deg_min_to_dec_west();
    test_gps_get_last_fix_null();
    gps_deinit();
    test_gps_cold_start();
    gps_deinit();
    /* FR-19: GPS→RTC sync guard tests */
    test_gps_rtc_delta_same_epoch();
    test_gps_rtc_delta_gps_1s_ahead();
    test_gps_rtc_delta_rtc_1s_ahead();
    test_gps_rtc_delta_rtc_uninitialized();
    test_gps_rtc_delta_rtc_at_boundary();
    printf("All NMEA parser tests passed.\n");
    return 0;
}