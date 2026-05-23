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
#include "gps_driver.h"
#include "post.h"
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
    system_state_t state;     /**< Sensor data: attitude (rad), rates (rad/s),
                               *   temperature (°C), validity flags             */
    flight_mode_t mode;       /**< Current flight mode (from FMM)               */
    energy_state_t energy;    /**< Current energy state (from EPS monitor)      */
    GpsFix_t gps_fix;         /**< Last GPS fix (lat, lon, alt, utc, valid)    */
    bool deploy_in_progress;  /**< True during auto-deploy sequence           */
    post_record_t post_last;  /**< Last POST result from boot self-test       */
    uint32_t mode_entry_tick; /**< xTaskGetTickCount() at last mode entry    */
    uint32_t seq;             /**< Write sequence counter.  Incremented on every
                               *   successful write call.  Readers can detect
                               *   stale copies by comparing seq values.         */
  } dl_snapshot_t;
  /**
   * @brief Update the GPS fix in the shared snapshot.
   * @param fix Pointer to GpsFix_t struct (must not be NULL)
   */
  void data_layer_set_gps_fix(const GpsFix_t *fix);

  /**
   * @brief Get the last GPS fix from the shared snapshot.
   * @param out Pointer to GpsFix_t struct to fill (must not be NULL)
   */
  void data_layer_get_gps_fix(GpsFix_t *out);

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
   * @brief Update humidity reading.
   *
   * @param humidity  Relative humidity in percent, or -1 if not available.
   *
   * Sets @c humidity_valid = true if humidity >= 0.
   */
  void data_layer_write_humidity(float humidity);

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
   * @brief Update illuminance reading.
   *
   * @param lux Illuminance in Lux, or -1 if not available.
   *
   * Sets @c lux_valid = true if lux >= 0.
   */
  void data_layer_write_lux(float lux);

  /**
   * @brief Set light sensor hardware availability flag.
   *
   * Called once during boot after BH1750 detection.
   *
   * @param lux true if the BH1750 was detected on the I2C bus.
   */
  void data_layer_set_lux_avail(bool lux);

  /**
   * @brief Update RTC timestamp in the shared snapshot.
   *
   * @param timestamp Unix epoch seconds, or 0 if not available.
   *
   * Sets @c rtc_valid = true if timestamp > 0.
   */
  void data_layer_write_rtc(uint32_t timestamp);

  /**
   * @brief Set RTC hardware availability flag.
   *
   * Called once during boot after DS3231 detection.
   *
   * @param rtc true if the DS3231 was detected on the I2C bus.
   */
  void data_layer_set_rtc_avail(bool rtc);

  /**
   * @brief Update power monitoring data in the shared snapshot.
   *
   * @param bus_voltage_mv Bus voltage in millivolts, or -1 if N/A.
   * @param current_ua Current in microamps, or 0 if N/A.
   * @param power_uw Power in microwatts, or 0 if N/A.
   *
   * Sets @c power_valid = true if bus_voltage_mv >= 0.
   */
  void data_layer_write_power(int16_t bus_voltage_mv, int32_t current_ua, int32_t power_uw);

  /**
   * @brief Set power monitor hardware availability flag.
   *
   * Called once during boot after INA219 detection.
   *
   * @param power true if the INA219 was detected on the I2C bus.
   */
  void data_layer_set_power_avail(bool power);

  /**
   * @brief Update solar panel power generation data.
   *
   * @param solar_voltage_mv Solar panel voltage in millivolts, or -1 if N/A.
   * @param solar_current_ua Solar panel current in microamps, or 0 if N/A.
   * @param solar_power_uw   Solar panel power in microwatts, or 0 if N/A.
   *
   * Sets @c solar_valid = true if solar_voltage_mv >= 0.
   */
  void data_layer_write_solar_power(int16_t solar_voltage_mv, int32_t solar_current_ua,
                                    int32_t solar_power_uw);

  /**
   * @brief Set solar panel INA219 hardware availability flag.
   *
   * Called once during boot after solar INA219 detection.
   *
   * @param solar true if the solar INA219 was detected on the I2C bus.
   */
  void data_layer_set_solar_avail(bool solar);

  /**
   * @brief Update radiation dose in the shared snapshot.
   *
   * @param dose  Radiation dose (placeholder units).
   */
  void data_layer_write_radiation(float dose);

  /**
   * @brief Update sun sensor readings.
   *
   * @param sun_x  Sun intensity X (0.0-1.0), or -1 if N/A.
   * @param sun_y  Sun intensity Y (0.0-1.0), or -1 if N/A.
   *
   * Sets @c sun_valid = true if sun_x >= 0.
   */
  void data_layer_write_sun(float sun_x, float sun_y);

  /**
   * @brief Set sun sensor hardware availability flag.
   *
   * Called once during boot after sun sensor detection.
   *
   * @param sun  true if sun sensor was detected.
   */
  void data_layer_set_sun_avail(bool sun);

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
   * @brief Update the current flight mode from ISR context.
   *
   * ISR-safe variant using critical section instead of mutex.
   * Safe to call from HardFault, PendSV, SysTick, or any hardware ISR.
   *
   * @param mode  New @ref flight_mode_t value.
   */
  void data_layer_set_flight_mode_from_isr(flight_mode_t mode);

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

  /* ------------------------------------------------------------------ */
  /* Deploy / POST accessors                                             */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Set the deploy-in-progress flag.
   * @param in_progress true when deploy sequence is active.
   */
  void data_layer_set_deploy_in_progress(bool in_progress);

  /**
   * @brief Get the deploy-in-progress flag.
   * @return true if the deploy sequence is active.
   */
  bool data_layer_get_deploy_in_progress(void);

  /**
   * @brief Store the most recent POST record.
   * @param record  POST record to save (must not be NULL).
   */
  void data_layer_set_post_last(const post_record_t *record);

  /**
   * @brief Retrieve the most recent POST record.
   * @param out  Populated with the stored record (must not be NULL).
   */
  void data_layer_get_post_last(post_record_t *out);

  /**
   * @brief Set the mode-entry tick count.
   *
   * Called by the FMM on every successful mode transition to record
   * when the current mode was entered.  Used by the deploy monitor
   * for timeout enforcement.
   *
   * @param tick  Value of xTaskGetTickCount() at mode entry.
   */
  void data_layer_set_mode_entry_tick(uint32_t tick);

  /**
   * @brief Get the mode-entry tick count.
   * @return The tick count recorded at the last mode transition.
   */
  uint32_t data_layer_get_mode_entry_tick(void);

#ifdef __cplusplus
}
#endif

#endif /* DATA_LAYER_H */
