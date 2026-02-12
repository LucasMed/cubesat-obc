#include "config.h"
#include <stdio.h>

// Minimal stub driver for MPU6050 (I2C) - simulated for now
int mpu6050_init(void) {
    // In real driver, initialize I2C and device
    printf("mpu6050: init (stub)\n");
    return 0;
}

int mpu6050_read(float *roll, float *pitch, float *yaw) {
    // Return simulated values (zeros)
    if (roll) *roll = 0.0f;
    if (pitch) *pitch = 0.0f;
    if (yaw) *yaw = 0.0f;
    return 0;
}
