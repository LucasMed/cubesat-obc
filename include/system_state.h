/**
 * @file system_state.h
 * @brief Thread-safe global system state for OBC.
 */

#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Structure holding the global status of the satellite.
 */
typedef struct {
    float attitude[3];  // Roll, Pitch, Yaw [deg]
    float rates[3];     // Gyro rates [deg/s]
    float temp;         // Internal temperature [C]
    float battery_v;    // Battery voltage [V] (future)
    bool imu_available; // Hardware detected during boot
    bool imu_valid;     // Recent data is fresh and valid
    bool temp_available;
    bool temp_valid;
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

#endif // SYSTEM_STATE_H
