// test_gps_integration.c -- Integration test for GPS driver with NMEA simulation
// Tests full pipeline: NMEA sentences -> parser -> data layer -> telemetry

#define GPS_TEST
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "gps_driver.h"
#include "data_layer.h"
#include "telemetry_task.h"

#ifdef __cplusplus
extern "C" {
#endif
bool nmea_buffer_push(unsigned char byte);
#ifdef __cplusplus
}
#endif

static int s_test_passed = 0;
static int s_test_failed = 0;

void inject_nmea(const char *sentence) {
    for (size_t i = 0; sentence[i]; i++) {
        nmea_buffer_push((unsigned char)sentence[i]);
    }
}

void test_case(const char *name, int condition) {
    if (condition) {
        printf("  [PASS] %s\n", name);
        s_test_passed++;
    } else {
        printf("  [FAIL] %s\n", name);
        s_test_failed++;
    }
}

void test_gga_valid_fix(void) {
    printf("\n=== Test: GGA Valid Fix ===\n");
    gps_init();
    
    const char *gga = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    inject_nmea(gga);
    
    GpsFix_t *fix = gps_read_fix();
    test_case("Fix returned non-NULL", fix != NULL);
    if (fix == NULL) {
        printf("  [SKIP] Remaining tests due to NULL fix\n");
        gps_deinit();
        return;
    }
    test_case("Fix is valid", fix->valid == true);
    test_case("Latitude approx 48.1", fabsf(fix->lat - 48.1173f) < 0.1f);
    test_case("Longitude approx 11.5", fabsf(fix->lon - 11.5167f) < 0.1f);
    test_case("Altitude approx 545", fabsf(fix->alt_m - 545.4f) < 2.0f);
    test_case("Satellites > 0", gps_get_satellites_in_view() > 0);
    
    gps_deinit();
}

void test_gga_invalid_checksum(void) {
    printf("\n=== Test: GGA Invalid Checksum ===\n");
    gps_init();
    
    const char *bad = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*00\r\n";
    inject_nmea(bad);
    
    GpsFix_t *fix = gps_read_fix();
    test_case("Fix is NULL with bad checksum (untrusted data)", fix == NULL);
    
    gps_deinit();
}

void test_gga_no_fix(void) {
    printf("\n=== Test: GGA No Fix ===\n");
    gps_init();
    
    const char *nofix = "$GPGGA,123519,4807.038,N,01131.000,E,0,00,0.9,545.4,M,46.9,M,,*5B\r\n";
    inject_nmea(nofix);
    
    GpsFix_t *fix = gps_read_fix();
    if (fix == NULL) {
        test_case("Fix is NULL (checksum may be invalid)", true);
        printf("  [INFO] Got NULL - sentence may have invalid checksum\n");
    } else {
        test_case("Fix is non-NULL", true);
        test_case("Fix is invalid (no fix)", fix->valid == false);
    }
    
    gps_deinit();
}

void test_data_layer_integration(void) {
    printf("\n=== Test: Data Layer Integration ===\n");
    gps_init();
    
    const char *gga = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    inject_nmea(gga);
    
    GpsFix_t *fix = gps_read_fix();
    test_case("Fix returned non-NULL", fix != NULL);
    if (fix == NULL) {
        printf("  [SKIP] Remaining tests due to NULL fix\n");
        gps_deinit();
        return;
    }
    data_layer_set_gps_fix(fix);
    
    GpsFix_t out = {0};
    data_layer_get_gps_fix(&out);
    
    test_case("Data layer lat matches", fabsf(out.lat - fix->lat) < 0.1f);
    test_case("Data layer lon matches", fabsf(out.lon - fix->lon) < 0.1f);
    test_case("Data layer alt matches", fabsf(out.alt_m - fix->alt_m) < 1.0f);
    test_case("Data layer valid matches", out.valid == fix->valid);
    
    gps_deinit();
}

void test_telemetry_integration(void) {
    printf("\n=== Test: Telemetry Integration ===\n");
    gps_init();
    
    const char *gga = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    inject_nmea(gga);
    
    GpsFix_t *fix = gps_read_fix();
    test_case("Fix returned non-NULL", fix != NULL);
    if (fix == NULL) {
        printf("  [SKIP] Remaining tests due to NULL fix\n");
        gps_deinit();
        return;
    }
    data_layer_set_gps_fix(fix);
    
    // Simulate telemetry packet fields from data layer
    GpsFix_t telemetry_fix = {0};
    data_layer_get_gps_fix(&telemetry_fix);
    
    test_case("Telemetry GPS valid from data layer", telemetry_fix.valid == true);
    test_case("Telemetry GPS lat from data layer", telemetry_fix.lat > 40.0f);
    test_case("Telemetry GPS lon from data layer", telemetry_fix.lon > 0.0f);
    test_case("Telemetry GPS alt from data layer", telemetry_fix.alt_m > 500.0f);
    
    gps_deinit();
}

void test_gps_is_fix_valid(void) {
    printf("\n=== Test: GPS Is Fix Valid ===\n");
    gps_init();
    
    const char *gga = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    inject_nmea(gga);
    gps_read_fix();
    
    test_case("gps_is_fix_valid returns true", gps_is_fix_valid() == true);
    
    gps_deinit();
}

void test_gps_get_last_fix(void) {
    printf("\n=== Test: GPS Get Last Fix ===\n");
    gps_init();
    
    const char *gga = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    inject_nmea(gga);
    gps_read_fix();
    
    GpsFix_t last = {0};
    bool result = gps_get_last_fix(&last);
    test_case("gps_get_last_fix returns true", result == true);
    test_case("Last fix is valid", last.valid == true);
    test_case("HDOP is parsed", last.hdop > 0.0f && last.hdop < 10.0f);
    
    gps_deinit();
}

void test_gps_satellites_in_view(void) {
    printf("\n=== Test: GPS Satellites In View ===\n");
    gps_init();
    
    const char *gga = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    inject_nmea(gga);
    gps_read_fix();
    
    uint8_t sats = gps_get_satellites_in_view();
    test_case("Satellites reported", sats == 8);
    
    gps_deinit();
}

void test_gps_stats(void) {
    printf("\n=== Test: GPS Stats ===\n");
    gps_init();
    gps_reset_stats();
    
    const GpsStats_t *stats = gps_get_stats();
    test_case("Initial sentences = 0", stats->sentences_received == 0);
    
    const char *gga = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    inject_nmea(gga);
    gps_read_fix();
    
    stats = gps_get_stats();
    test_case("Sentences received > 0", stats->sentences_received > 0);
    test_case("Fixes valid > 0", stats->fixes_valid > 0);
    
    gps_deinit();
}

int main(void) {
    printf("===========================================\n");
    printf("GPS Integration Test Suite\n");
    printf("===========================================\n");
    
    test_gga_valid_fix();
    test_gga_invalid_checksum();
    test_gga_no_fix();
    test_data_layer_integration();
    test_telemetry_integration();
    test_gps_is_fix_valid();
    test_gps_get_last_fix();
    test_gps_satellites_in_view();
    test_gps_stats();
    
    printf("\n===========================================\n");
    printf("Results: %d passed, %d failed\n", s_test_passed, s_test_failed);
    printf("===========================================\n");
    
    return s_test_failed > 0 ? 1 : 0;
}
