// gps_stub.h -- GPS stub API for host/unit testing
// All comments in English
// This stub simulates a GPS driver for use in tests when hardware is unavailable.

#ifndef GPS_STUB_H
#define GPS_STUB_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  GPS_MOCK_OK,
  GPS_MOCK_FAULT_TIMEOUT,        // UART silent/no data
  GPS_MOCK_FAULT_CHECKSUM,       // Invalid checksum in NMEA sentence
  GPS_MOCK_FAULT_PARTIAL_FRAME,  // Truncated/no \r\n frame
  GPS_MOCK_FAULT_NO_FIX,         // Valid stream, but no GPS fix
  GPS_MOCK_FAULT_STALE_DATA      // Fix present, but timestamp is old
} GpsMockMode_t;

typedef struct
{
  float lat;
  float lon;
  float alt_m;
  uint32_t utc_time;
  bool valid;
  uint32_t timestamp_ms;
  uint8_t satellites_in_view;
} GpsFix_t;

// Initialize the GPS stub driver (simulate UART initialization)
bool gps_init(void);

// De-initialize/reset the GPS stub driver
void gps_deinit(void);

// Return a simulated GPS fix. Will reflect injected or configured state.
GpsFix_t *gps_read_fix(void);

// Return last stored simulated GPS fix.
GpsFix_t *gps_get_last_fix(void);

// Return true if fix is valid (not stale/expired)
bool gps_is_fix_valid(void);

// Return simulated satellite count
uint8_t gps_get_satellites_in_view(void);

// Inject custom fix data for simulation
void gps_mock_set_data(const GpsFix_t *fix);

// Set error mode for GPS stub simulation
void gps_mock_set_mode(GpsMockMode_t mode);

// Return number of times a stub function was called (for test assertions)
uint32_t gps_mock_get_call_count(const char *func_name);

// Reset all internal counters for stub
void gps_mock_reset_counters(void);

#endif  // GPS_STUB_H
