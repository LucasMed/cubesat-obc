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
  float attitude[3];        /**< Roll, Pitch, Yaw [rad]              */
  float rates[3];           /**< Gyro angular rates [rad/s]          */
  float temp;               /**< Internal temperature [°C]           */
  float battery_v;          /**< Battery voltage [V]                 */
  float gyro_bias[3];       /**< EKF-estimated gyro bias [rad/s]     */
  float att_uncertainty[3]; /**< EKF diagonal covariance P[0..2] [rad²] */
  bool imu_available;       /**< IMU detected during boot            */
  bool imu_valid;           /**< IMU data is fresh and valid         */
  bool temp_available;      /**< Temp sensor detected during boot    */
  bool temp_valid;          /**< Temperature data is fresh           */
  bool imu_ekf_valid;       /**< EKF estimate is converged and fresh */
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
