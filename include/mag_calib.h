/**
 * @file mag_calib.h
 * @brief Magnetometer hard/soft iron calibration API.
 *
 * Provides in-flight calibration routines to correct for PCB-induced
 * magnetic fields (hard iron) and field distortions (soft iron).
 *
 * Calibration procedure:
 *   1. Call mag_calib_start() before rotating the CubeSat.
 *   2. Rotate the spacecraft in all orientations (figure-8 motion)
 *      for at least 60 seconds.
 *   3. Call mag_calib_finish() to compute offsets.
 *   4. Calibrated values are applied automatically in mag_calib_apply().
 *
 * Storage: Calibration values should be saved to flash (W25Q64)
 * for persistence across reboots (TODO: Phase 7).
 */
#ifndef MAG_CALIB_H
#define MAG_CALIB_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Calibration state structure.
 */
typedef struct
{
  float offset[3]; /**< Hard iron offsets (µT) */
  float scale[3];  /**< Soft iron scale factors (unitless) */
  bool calibrated; /**< True if valid calibration exists */
} mag_calib_t;

/**
 * @brief Start calibration collection.
 *
 * Resets min/max trackers.  Call this before rotating the spacecraft.
 */
void mag_calib_start(void);

/**
 * @brief Feed one raw magnetometer sample during rotation.
 *
 * Call this at your sensor read rate (e.g., 10 Hz) while
 * performing figure-8 rotations.
 *
 * @param mag_raw  Raw magnetic field vector (µT, sensor frame).
 */
void mag_calib_collect(const float mag_raw[3]);

/**
 * @brief Finish calibration and compute offsets.
 *
 * Computes:
 *   offset[i] = (max[i] + min[i]) / 2
 *   scale[i]  = 1.0 (simplified; full soft-iron is advanced)
 */
void mag_calib_finish(void);

/**
 * @brief Apply calibration to a raw reading.
 *
 * @param mag_raw   Raw vector (µT, sensor frame).
 * @param mag_cal  Output: calibrated vector (µT, sensor frame).
 */
void mag_calib_apply(const float mag_raw[3], float mag_cal[3]);

/**
 * @brief Check if valid calibration exists.
 */
bool mag_calib_is_valid(void);

/**
 * @brief Get current calibration data (for saving to flash).
 */
void mag_calib_get(mag_calib_t *out);

/**
 * @brief Load calibration data (from flash).
 */
void mag_calib_load(const mag_calib_t *in);

#endif /* MAG_CALIB_H */
