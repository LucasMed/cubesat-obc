/**
 * @file mpu6050.c
 * @brief MPU6050 IMU Driver Implementation.
 */

#include "drivers/imu/mpu6050.h"

#include "drivers/i2c_interface.h"

#include <stdio.h>

// MPU6050 Registers
#define MPU6050_ADDR 0x68
#define MPU6050_PWR_MGMT_1 0x6B
#define MPU6050_ACCEL_XOUT_H 0x3B
#define MPU6050_GYRO_XOUT_H 0x43
#define MPU6050_WHO_AM_I 0x75

// Configuration values
#define MPU6050_WAKEUP 0x00

int mpu6050_init(void)
{
  uint8_t data[2];

  // Check Who Am I
  uint8_t id;
  if (i2c_bus_write_read(MPU6050_ADDR, (uint8_t[]){MPU6050_WHO_AM_I}, 1, &id, 1) < 0)
  {
    printf("mpu6050: Sensor not detected on I2C bus\n");
    return -1;
  }

  if (id != 0x68)
  {
    printf("mpu6050: Unknown device ID: 0x%02X\n", id);
    return -1;
  }

  // Wake up device (write 0 to PWR_MGMT_1)
  data[0] = MPU6050_PWR_MGMT_1;
  data[1] = MPU6050_WAKEUP;
  if (i2c_bus_write(MPU6050_ADDR, data, 2) < 0)
  {
    printf("mpu6050: Failed to wake up\n");
    return -1;
  }

  printf("mpu6050: initialized successfully\n");
  return 0;
}

int mpu6050_read_raw(float accel[3], float gyro[3])
{
  uint8_t buffer[14];
  uint8_t reg = MPU6050_ACCEL_XOUT_H;

  if (i2c_bus_write_read(MPU6050_ADDR, &reg, 1, buffer, 14) < 0)
  {
    return -1;
  }

  // Combine high/low bytes
  int16_t ax = (int16_t)((buffer[0] << 8) | buffer[1]);
  int16_t ay = (int16_t)((buffer[2] << 8) | buffer[3]);
  int16_t az = (int16_t)((buffer[4] << 8) | buffer[5]);
  // buffer[6,7] is temperature (skip for now)
  int16_t gx = (int16_t)((buffer[8] << 8) | buffer[9]);
  int16_t gy = (int16_t)((buffer[10] << 8) | buffer[11]);
  int16_t gz = (int16_t)((buffer[12] << 8) | buffer[13]);

  // Convert to physical units (scales based on ±2g and ±250 deg/s by default)
  // For now using ±2g (16384 LSB/g) and ±250 deg/s (131 LSB/deg/s)
  accel[0] = (float)ax / 16384.0f;
  accel[1] = (float)ay / 16384.0f;
  accel[2] = (float)az / 16384.0f;

  gyro[0] = (float)gx / 131.0f;
  gyro[1] = (float)gy / 131.0f;
  gyro[2] = (float)gz / 131.0f;

  return 0;
}

int mpu6050_read(float *roll, float *pitch, float *yaw)
{
  float accel[3];
  float gyro[3];
  if (mpu6050_read_raw(accel, gyro) < 0)
  {
    return -1;
  }

  // Simple pass-through for now (or basic complementary filter in the future)
  if (roll)
  {
    *roll = gyro[0];
  }
  if (pitch)
  {
    *pitch = gyro[1];
  }
  if (yaw)
  {
    *yaw = gyro[2];
  }

  return 0;
}
