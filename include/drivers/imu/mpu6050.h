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
 * Wakes up the device and sets default ranges (±8g, ±500°/s).
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

#endif // MPU6050_H
