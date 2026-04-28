/**
 * @file imu_calib.h
 * @brief MPU-6050 accelerometer + gyroscope calibration API.
 *
 * Calibration procedure (ground, before launch):
 *   1. Call imu_calib_start() — begins collecting samples
 *   2. Keep CubeSat stationary for gyro calibration (10+ seconds)
 *   3. Rotate to 6 orientations for accel calibration
 *   4. Call imu_calib_finish() — computes offsets and writes to hardware
 *   5. Call imu_calib_save_to_flash() — persists to W25Q64
 *
 * In orbit: gyro static bias already applied; EKF refines residual drift.
 * Accelerometer should not be used for tilt estimation in microgravity
 * unless under thrust or rotation.
 */

#ifndef IMU_CALIB_H
#define IMU_CALIB_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Calibration state structure.
 */
typedef struct
{
  float accel_offset[3];      /**< Accel offsets in g (for software path) */
  float accel_scale[3];       /**< Accel scale factors (unitless) */
  int16_t gyro_offset_raw[3]; /**< Raw register values for 0x13-0x18 */
  float gyro_bias_rads[3];    /**< Gyro static bias in rad/s (for EKF seed) */
  bool calibrated;            /**< True if valid calibration exists */
} imu_calib_t;

/**
 * @brief Start calibration collection (resets accumulators).
 */
void imu_calib_start(void);

/**
 * @brief Feed one raw IMU sample during calibration.
 *
 * @param accel_g   Raw accel in g (from mpu6050_read_raw)
 * @param gyro_dps  Raw gyro in deg/s (from mpu6050_read_raw)
 */
void imu_calib_collect(const float accel_g[3], const float gyro_dps[3]);

/**
 * @brief Finish calibration: compute offsets, write hardware registers, seed EKF.
 */
void imu_calib_finish(void);

/**
 * @brief Apply calibration to a raw accel reading.
 *
 * @param accel_raw   Raw in g
 * @param accel_cal   Calibrated in g
 */
void imu_calib_apply_accel(const float accel_raw[3], float accel_cal[3]);

/**
 * @brief Check if valid calibration exists.
 */
bool imu_calib_is_valid(void);

/**
 * @brief Get current calibration data (for flash storage).
 */
void imu_calib_get(imu_calib_t *out);

/**
 * @brief Load calibration data (from flash), write hardware registers, seed EKF.
 */
void imu_calib_load(const imu_calib_t *in);

/**
 * @brief Save calibration to W25Q64 flash.
 */
void imu_calib_save_to_flash(void);

/**
 * @brief Load calibration from W25Q64 flash on boot.
 */
void imu_calib_load_from_flash(void);

#endif /* IMU_CALIB_H */
