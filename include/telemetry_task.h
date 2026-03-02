// Telemetry Task - packages and transmits telemetry data
#ifndef TELEMETRY_TASK_H
#define TELEMETRY_TASK_H

#include <stdint.h>

#define TELEMETRY_PORT 10

// Packed struct ensures consistent size across different architectures
typedef struct __attribute__((packed))
{
  uint32_t timestamp_ms;  // Time since boot
  float attitude[3];      // Roll, Pitch, Yaw [deg]
  float rates[3];         // Gyro rates [deg/s]
  float temp;             // Internal temperature [C]
  uint8_t flags;          // Bit 0: imu_valid, Bit 1: temp_valid
} csp_telemetry_packet_t;

void vTelemetryTask(void *pvParameters);
void vTelemetryTask_Step(void);

#endif  // TELEMETRY_TASK_H
