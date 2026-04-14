/**
 * @file mpu6050.c
 * @brief MPU6050 IMU Driver Implementation.
 */

#include "drivers/imu/mpu6050.h"

#include "drivers/i2c_interface.h"

/* cppcheck-suppress misra-c2012-21.6 -- MISRA deviation: printf used for
 * IMU driver fault diagnostics; acceptable during bring-up; to be replaced
 * with log_event() in Phase 7. See MISRA_DEVIATIONS.md §21.6-D4. */
#include <stdio.h>

#ifdef PICO_BUILD
  #include "pico/stdlib.h"
#else
  /* Host/emulation build: provide a stub delay (no-op) since host is single-threaded */
  #define sleep_ms(ms) ((void)(ms))
#endif

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

  /* Soft reset the MPU6050 to ensure clean state on startup.
   * Write 1 to PWR_MGMT_1[7] (DEVICE_RESET) to trigger POR-like reset.
   * This brings the device from an unknown state into a predictable one.
   * Spec: Reset takes max 100ms; use 50ms conservative delay. */
  data[0] = MPU6050_PWR_MGMT_1;
  data[1] = 0x80u; /* Set DEVICE_RESET bit (PWR_MGMT_1[7] = 1) */
  if (i2c_bus_write(MPU6050_ADDR, data, 2u) < 0)
  {
    (void)printf("mpu6050: Failed to reset device\n");
    return -1;
  }
  /* Wait for reset to complete */
  sleep_ms(50);

  // Check Who Am I
  uint8_t id;
  if (i2c_bus_write_read(MPU6050_ADDR, (uint8_t[]){MPU6050_WHO_AM_I}, 1, &id, 1) < 0)
  {
    (void)printf("mpu6050: Sensor not detected on I2C bus\n");
    return -1;
  }

  if (id != (uint8_t)0x68u && id != (uint8_t)0x70u)
  {
    (void)printf("mpu6050: Unknown device ID: 0x%02X (expected 0x68 or 0x70)\n", id);
    return -1;
  }

  (void)printf("mpu6050: detected (ID=0x%02X)\n", id);

  // Wake up device (write 0 to PWR_MGMT_1)
  data[0] = MPU6050_PWR_MGMT_1;
  data[1] = MPU6050_WAKEUP;
  if (i2c_bus_write(MPU6050_ADDR, data, 2u) < 0)
  {
    (void)printf("mpu6050: Failed to wake up\n");
    return -1;
  }

  (void)printf("mpu6050: initialized successfully\n");
  return 0;
}

int mpu6050_read_raw(float accel[3], float gyro[3])
{
  if (accel == NULL && gyro == NULL)
  {
    return 0;
  }

  uint8_t buffer[14];
  uint8_t reg = MPU6050_ACCEL_XOUT_H;

  if (i2c_bus_write_read(MPU6050_ADDR, &reg, 1, buffer, 14) < 0)
  {
    return -1;
  }

  // Combine high/low bytes
  int16_t ax = (int16_t)(((uint16_t)buffer[0] << 8u) | (uint16_t)buffer[1]);
  int16_t ay = (int16_t)(((uint16_t)buffer[2] << 8u) | (uint16_t)buffer[3]);
  int16_t az = (int16_t)(((uint16_t)buffer[4] << 8u) | (uint16_t)buffer[5]);
  // buffer[6,7] is temperature (skip for now)
  int16_t gx = (int16_t)(((uint16_t)buffer[8] << 8u) | (uint16_t)buffer[9]);
  int16_t gy = (int16_t)(((uint16_t)buffer[10] << 8u) | (uint16_t)buffer[11]);
  int16_t gz = (int16_t)(((uint16_t)buffer[12] << 8u) | (uint16_t)buffer[13]);

  // Convert to physical units (scales based on ±2g and ±250 deg/s by default)
  // For now using ±2g (16384 LSB/g) and ±250 deg/s (131 LSB/deg/s)
  if (accel != NULL)
  {
    accel[0] = (float)ax / 16384.0f;
    accel[1] = (float)ay / 16384.0f;
    accel[2] = (float)az / 16384.0f;
  }

  if (gyro != NULL)
  {
    gyro[0] = (float)gx / 131.0f;
    gyro[1] = (float)gy / 131.0f;
    gyro[2] = (float)gz / 131.0f;
  }

  return 0;
}

/* cppcheck-suppress unusedFunction -- called from sensor_read_task on Pico */
int mpu6050_read(float *roll, float *pitch, float *yaw)
{
  float accel[3];
  float gyro[3];
  if (mpu6050_read_raw(accel, gyro) < 0)
  {
    return -1;
  }

  // Simple pass-through for now (or basic complementary filter in the future)
  if (roll != NULL)
  {
    *roll = gyro[0];
  }
  if (pitch != NULL)
  {
    *pitch = gyro[1];
  }
  if (yaw != NULL)
  {
    *yaw = gyro[2];
  }

  return 0;
}
