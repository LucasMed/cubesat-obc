/**
 * @file system_state.h
 * @brief Thread-safe global system state for OBC.
 */

#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Structure holding the raw sensor state of the satellite.
 *
 * @note All angular quantities are in **radians / rad/s** (SPEC-2-DLA §2.4).
 *       Callers that need degrees must convert explicitly.
 *       Use the Data Layer API (data_layer.h) for thread-safe access.
 */
typedef struct
{
  float attitude[3];         /**< Roll, Pitch, Yaw [rad]              */
  float q[4];                /**< Quaternion [w, x, y, z]             */
  float rates[3];            /**< Gyro angular rates [rad/s]          */
  float temp;                /**< Internal temperature [°C]           */
  float humidity;            /**< Relative humidity [%], -1 if N/A    */
  float battery_v;           /**< Battery voltage [V]                 */
  float gyro_bias[3];        /**< EKF-estimated gyro bias [rad/s]     */
  float att_uncertainty[4];  /**< EKF diagonal covariance (q error)   */
  float mag_field[3];        /**< Calibrated magnetic field [µT]      */
  bool imu_available;        /**< IMU detected during boot            */
  bool imu_valid;            /**< IMU data is fresh and valid         */
  bool temp_available;       /**< Temp sensor detected during boot    */
  bool temp_valid;           /**< Temperature data is fresh           */
  bool humidity_valid;       /**< Humidity data is fresh and valid    */
  bool imu_ekf_valid;        /**< EKF estimate is converged and fresh */
  bool mag_available;        /**< Magnetometer detected during boot   */
  bool mag_valid;            /**< Magnetometer data is fresh          */
  float lux;                 /**< Illuminance [lux], -1 if N/A       */
  bool lux_valid;            /**< lux data is fresh and valid        */
  bool lux_available;        /**< BH1750 detected during boot        */
  uint32_t rtc_timestamp;    /**< Unix epoch seconds, 0 if N/A      */
  bool rtc_valid;            /**< RTC data is fresh and valid        */
  bool rtc_available;        /**< DS3231 detected during boot        */
  int16_t bus_voltage_mv;    /**< Bus voltage [mV], -1 if N/A       */
  int32_t current_ua;        /**< Current [µA], 0 if N/A            */
  int32_t power_uw;          /**< Power [µW], 0 if N/A              */
  bool power_valid;          /**< Power data is fresh and valid      */
  bool power_available;      /**< INA219 detected during boot        */
  float radiation_dose;      /**< Radiation dose (placeholder unit)   */
  float sun_x;               /**< Sun sensor X intensity (0.0-1.0), -1 if N/A */
  float sun_y;               /**< Sun sensor Y intensity (0.0-1.0), -1 if N/A */
  bool sun_valid;            /**< Sun sensor data is fresh and valid    */
  bool sun_available;        /**< Sun sensor detected during boot       */
  uint16_t image_count;      /**< Number of images stored on SD       */
  bool payload_rail_enabled; /**< Status of 3.3V/5V payload rail     */
} system_state_t;

/**
 * @brief Initialize the system state module and mutexes.
 */
void system_state_init(void);

/**
 * @brief Mark sensor as available or not (thread-safe).
 */
void system_state_set_available(bool imu, bool temp);

/**
 * @brief Set the current attitude and rates (thread-safe).
 */
void system_state_set_imu(const float att[3], const float rates[3]);

/**
 * @brief Set the current temperature (thread-safe).
 */
void system_state_set_temp(float temp);

/**
 * @brief Get a copy of the entire system state (thread-safe).
 */
void system_state_get(system_state_t *out_state);

#endif  // SYSTEM_STATE_H
