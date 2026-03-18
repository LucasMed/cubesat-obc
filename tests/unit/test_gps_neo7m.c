// test_gps_neo7m.c -- Unit test for real NEO-7M GPS NMEA parser
// All comments in English

// GPS_TEST is defined via CMake (target_compile_definitions)
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "gps_driver.h"

#ifdef __cplusplus
extern "C" {
#endif
// Exposed by driver with macro GPS_TEST
bool nmea_buffer_push(unsigned char byte);
#ifdef __cplusplus
}
#endif

void inject_sentence(const char *sentence) {
    // Injects a NMEA sentence, including \r\n
    for (size_t i = 0; sentence[i]; ++i) {
        assert(nmea_buffer_push((unsigned char)sentence[i]));
    }
}

void test_parse_valid_gpgga(void) {
    gps_init();
    // Example from NMEA standard: $GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n
    const char *gpgga = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    inject_sentence(gpgga);
    GpsFix_t* fix = gps_read_fix();
    assert(fix != NULL);
    assert(fix->valid);
    // Check values (allow small numerical tolerance due to float conversion)
    assert(fabsf(fix->lat - 48.1173f) < 0.0002f);
    assert(fabsf(fix->lon - 11.5167f) < 0.0002f);
    assert(fabsf(fix->alt_m - 545.4f) < 0.01f);
    assert(fix->utc_time == 12*3600 + 35*60 + 19);
    assert(gps_get_satellites_in_view() == 8);
}

void test_bad_checksum(void) {
    gps_init();
    const char *bad = "$GPGGA,123519,4807.038,N,01131.000,E,1,06,0.9,545.4,M,46.9,M,,*00\r\n";
    inject_sentence(bad);
    GpsFix_t* fix = gps_read_fix();
    // gps_read_fix returns NULL for invalid checksum
    assert(fix == NULL);
}

int main(void) {
    printf("Testing real NEO-7M GPS NMEA parser...\n");
    test_parse_valid_gpgga();
    test_bad_checksum();
    printf("All NMEA parser tests passed.\n");
    return 0;
}
