/**
 * @file mpu6050.h
 * @brief MPU6050 IMU Driver Interface.
 */

#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>

/**
 * @brief Initialize the MPU6050 sensor.
 *
 * Wakes up the device and sets default ranges (±2g, ±250°/s).
 *
 * @return 0 on success, negative error code otherwise.
 */
int mpu6050_init(void);

/**
 * @brief Read accelerometer and gyroscope data.
 *
 * @param roll Pointer to store roll angle/rate (X-axis)
 * @param pitch Pointer to store pitch angle/rate (Y-axis)
 * @param yaw Pointer to store yaw angle/rate (Z-axis)
 * @return 0 on success, negative error code otherwise.
 */
int mpu6050_read(float *roll, float *pitch, float *yaw);

/**
 * @brief Read raw IMU data.
 *
 * Useful for debugging or advanced processing.
 *
 * @param accel Array to store [ax, ay, az] in G's
 * @param gyro Array to store [gx, gy, gz] in deg/s
 * @return 0 on success.
 */
int mpu6050_read_raw(float accel[3], float gyro[3]);

/**
 * @brief Write gyroscope offset registers (0x13-0x18).
 *
 * @param offset Array of 3 int16_t values for X/Y/Z axes.
 *        Scale: 32.8 LSB/(°/s) at ±250 °/s.
 * @return 0 on success, negative on error.
 */
int mpu6050_write_gyro_offset(const int16_t offset[3]);

/**
 * @brief Write accelerometer offset registers (0x06-0x0B).
 *
 * @param offset Array of 3 int16_t values for X/Y/Z axes.
 *        Scale: 2048 LSB/g at ±2g.
 * @return 0 on success, negative on error.
 */
int mpu6050_write_accel_offset(const int16_t offset[3]);

#endif  // MPU6050_H
