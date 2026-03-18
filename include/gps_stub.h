// gps_stub.h -- GPS stub API for host/unit testing
// All comments in English
// This stub simulates a GPS driver for use in tests when hardware is unavailable.

#ifndef GPS_STUB_H
#define GPS_STUB_H

#include "gps_driver.h"

typedef enum
{
  GPS_MOCK_OK,
  GPS_MOCK_FAULT_TIMEOUT,        // UART silent/no data
  GPS_MOCK_FAULT_CHECKSUM,       // Invalid checksum in NMEA sentence
  GPS_MOCK_FAULT_PARTIAL_FRAME,  // Truncated/no \r\n frame
  GPS_MOCK_FAULT_NO_FIX,         // Valid stream, but no GPS fix
  GPS_MOCK_FAULT_STALE_DATA      // Fix present, but timestamp is old
} GpsMockMode_t;

// Inject custom fix data for simulation
void gps_mock_set_data(const GpsFix_t *fix);

// Set error mode for GPS stub simulation
void gps_mock_set_mode(GpsMockMode_t mode);

// Return number of times a stub function was called (for test assertions)
uint32_t gps_mock_get_call_count(const char *func_name);

// Reset all internal counters for stub
void gps_mock_reset_counters(void);

#endif  // GPS_STUB_H
