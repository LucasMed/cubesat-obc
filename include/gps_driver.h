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
  uint32_t utc_time;      ///< UTC time (epoch seconds)
  bool valid;             ///< True if fix is valid
  uint32_t timestamp_ms;  ///< Timestamp for stale fix detection (ms)
} GpsFix_t;

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
 * @brief Returns pointer to last valid GPS fix
 * @return Pointer to static GpsFix_t struct or NULL if no fix
 */
GpsFix_t *gps_get_last_fix(void);

/**
 * @brief Returns true if current fix is valid and not stale
 * @return true if fix is valid
 */
bool gps_is_fix_valid(void);

/**
 * @brief Returns number of satellites in current view (if known)
 * @return uint8_t satellite count
 */
uint8_t gps_get_satellites_in_view(void);

#endif  // GPS_DRIVER_H
