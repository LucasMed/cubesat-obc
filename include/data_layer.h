/**
 * @file data_layer.h
 * @brief Data Layer Abstraction (DLA) — thread-safe shared state bus.
 *
 * The DLA is the single authoritative source for the satellite's
 * run-time state.  All subsystem tasks MUST read and write shared data
 * exclusively through this API.  Direct access to module-internal
 * globals is forbidden.
 *
 * @par Units convention (SPEC-2-DLA §2.4)
 *   - Attitude angles : radians
 *   - Angular rates   : rad/s
 *   - Temperature     : °C
 *   - Voltage         : V
 *
 * @par Thread safety
 *   On PICO_BUILD a FreeRTOS mutex serialises every read/write.
 *   On host builds (unit tests) the mutex is a no-op; tests are
 *   expected to be single-threaded.
 *
 * Spec ref: SPEC-2-DLA v1.6, SPEC-2 v2.0 §4.5
 */

#ifndef DATA_LAYER_H
#define DATA_LAYER_H

#include "eps.h"
#include "flight_mode.h"
#include "system_state.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /* ------------------------------------------------------------------ */
  /* Aggregated snapshot type                                            */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Complete satellite state snapshot.
   *
   * Extends @ref system_state_t with flight-level state fields that are
   * managed by the FMM and EPS monitor.  A single mutex protects the
   * entire structure.
   */
  typedef struct
  {
    system_state_t state;  /**< Sensor data: attitude (rad), rates (rad/s),
                            *   temperature (°C), validity flags             */
    flight_mode_t mode;    /**< Current flight mode (from FMM)               */
    energy_state_t energy; /**< Current energy state (from EPS monitor)      */
    uint32_t seq;          /**< Write sequence counter.  Incremented on every
                            *   successful write call.  Readers can detect
                            *   stale copies by comparing seq values.         */
  } dl_snapshot_t;

  /* ------------------------------------------------------------------ */
  /* Initialisation                                                      */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Initialise the Data Layer.
   *
   * Zeroes the internal snapshot, creates the FreeRTOS mutex (PICO_BUILD
   * only), and calls system_state_init().  Must be called once during
   * system_init() before any task or subsystem accesses the DLA.
   */
  void data_layer_init(void);

  /* ------------------------------------------------------------------ */
  /* Read                                                                */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Atomically copy the current snapshot.
   *
   * @param out  Caller-allocated @ref dl_snapshot_t to fill.
   *             Must not be NULL.
   */
  void data_layer_read(dl_snapshot_t *out);

  /* ------------------------------------------------------------------ */
  /* Write — sensor data                                                 */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Update IMU data (attitude and angular rates).
   *
   * @param att_rad   Roll/Pitch/Yaw angles in **radians** [3].
   * @param rates_rad Angular rates in **rad/s** [3].
   *
   * Sets @c imu_valid = true and increments @c seq.
   */
  void data_layer_write_imu(const float att_rad[3], const float rates_rad[3]);

  /**
   * @brief Update EKF outputs in the shared snapshot.
   *
   * Called by the sensor read task immediately after each EKF predict+update
   * cycle.  Stores EKF attitude estimate, gyro bias, and diagonal covariance
   * back to @ref system_state_t and sets @c imu_ekf_valid = true.
   *
   * @param q          EKF attitude estimate [w, x, y, z] unit quaternion [4].
   * @param bias_rad   EKF gyro-bias estimate [rad/s] [3].
   * @param cov_diag   EKF diagonal covariance P [7] (q0..q3, bx..bz).
   */
  void data_layer_write_ekf(const float q[4], const float bias_rad[3], const float cov_diag[7]);

  /**
   * @brief Update temperature reading.
   *
   * @param temp_c  Temperature in degrees Celsius.
   *
   * Sets @c temp_valid = true and increments @c seq.
   */
  void data_layer_write_temp(float temp_c);

  /**
   * @brief Set sensor hardware availability flags.
   *
   * Called once during boot after sensor detection.
   *
   * @param imu   true if IMU was detected on the I2C bus.
   * @param temp  true if temperature sensor was detected.
   */
  void data_layer_set_sensor_avail(bool imu, bool temp);

  /**
   * @brief Update magnetometer data in the shared snapshot.
   *
   * @param field_uT  Calibrated magnetic field vector [Bx, By, Bz] in µT [3].
   *
   * Sets @c mag_valid = true and increments @c seq.
   */
  void data_layer_write_mag(const float field_uT[3]);

  /**
   * @brief Set magnetometer hardware availability flag.
   *
   * Called once during boot after magnetometer detection.
   *
   * @param mag  true if the RM3100 was detected on the SPI bus.
   */
  void data_layer_set_mag_avail(bool mag);

  /**
   * @brief Update radiation dose in the shared snapshot.
   *
   * @param dose  Radiation dose (placeholder units).
   */
  void data_layer_write_radiation(float dose);

  /**
   * @brief Update payload status in the shared snapshot.
   *
   * @param rail_enabled  Status of the payload power rail.
   * @param img_count     Number of images stored.
   */
  void data_layer_write_payload_status(bool rail_enabled, uint16_t img_count);

  /* ------------------------------------------------------------------ */
  /* Write — flight-level state                                          */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Update the current flight mode in the shared snapshot.
   *
   * Called exclusively by the Flight Mode Manager after a successful
   * mode transition.
   *
   * @param mode  New @ref flight_mode_t value.
   */
  void data_layer_set_flight_mode(flight_mode_t mode);

  /**
   * @brief Update the current energy state in the shared snapshot.
   *
   * Called exclusively by the EPS monitor task.
   *
   * @param energy  New @ref energy_state_t value.
   */
  void data_layer_set_energy_state(energy_state_t energy);

  /* ------------------------------------------------------------------ */
  /* Convenience accessors (lock-free fast path for single fields)       */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Return the current flight mode without copying the full snapshot.
   * @return Current @ref flight_mode_t.
   */
  flight_mode_t data_layer_get_flight_mode(void);

  /**
   * @brief Return the current energy state without copying the full snapshot.
   * @return Current @ref energy_state_t.
   */
  energy_state_t data_layer_get_energy_state(void);

  /**
   * @brief Return the current write sequence counter.
   *
   * Useful for detecting whether the snapshot has been updated since the
   * last read without taking the full mutex.
   */
  uint32_t data_layer_get_seq(void);

#ifdef __cplusplus
}
#endif

#endif /* DATA_LAYER_H */
