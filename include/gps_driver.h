#ifndef GPS_DRIVER_H
#define GPS_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Structure representing one GPS fix.
 */
typedef struct
{
  float lat;              ///< Latitude (decimal degrees)
  float lon;              ///< Longitude (decimal degrees)
  float alt_m;            ///< Altitude (meters)
  float hdop;             ///< Horizontal Dilution of Precision (>= 1.0, >5 indicates poor fix)
  uint32_t utc_time;      ///< UTC time (epoch seconds)
  uint8_t satellites;     ///< Number of satellites used in fix
  bool valid;             ///< True if fix is valid
  uint32_t timestamp_ms;  ///< Timestamp for stale fix detection (ms)
} GpsFix_t;

/**
 * @brief GPS parser statistics (for telemetry and debug)
 */
typedef struct
{
  uint32_t sentences_received;  ///< Total NMEA sentences received
  uint32_t checksum_errors;     ///< Checksum verification failures
  uint32_t parse_errors;        ///< GGA parse failures
  uint32_t fixes_valid;         ///< Number of valid fixes
  uint32_t fixes_invalid;       ///< Number of invalid fixes
  uint32_t buffer_overflows;    ///< Ring buffer overflow events
} GpsStats_t;

/**
 * @brief Initialize the GPS hardware driver (UART, parser, etc.)
 * @return true if successful
 */
bool gps_init(void);

/**
 * @brief Deinitialize GPS driver and release resources
 */
void gps_deinit(void);

/**
 * @brief Reads and returns a new GPS fix (may block)
 * @return Pointer to static GpsFix_t struct, or NULL on error
 */
GpsFix_t *gps_read_fix(void);

/**
 * @brief Copies last valid GPS fix to out parameter
 * @param out Pointer to GpsFix_t where fix will be copied
 * @return true if valid fix was copied, false if no valid fix available
 */
bool gps_get_last_fix(GpsFix_t *out);

/**
 * @brief Returns true if current fix is valid and not stale
 * @return true if fix is valid
 */
bool gps_is_fix_valid(void);

/**
 * @brief Returns number of satellites in current fix
 * @return uint8_t satellite count
 */
uint8_t gps_get_satellites_in_view(void);

/**
 * @brief Get GPS driver statistics
 * @return Pointer to internal GpsStats_t (read-only)
 */
const GpsStats_t *gps_get_stats(void);

/**
 * @brief Reset GPS driver statistics
 */
void gps_reset_stats(void);

#endif  // GPS_DRIVER_H
