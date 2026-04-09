// Telemetry Task - packages and transmits telemetry data
#ifndef TELEMETRY_TASK_H
#define TELEMETRY_TASK_H

#include <stdint.h>

#define TELEMETRY_PORT 10

// Packed struct ensures consistent size across different architectures
typedef struct __attribute__((packed))
{
  uint32_t timestamp_ms;   // Time since boot
  float attitude[3];       // Roll, Pitch, Yaw [deg]
  float rates[3];          // Gyro rates [deg/s]
  float temp;              // Temperature [C] (SHT31 or ADC)
  float humidity;          // Relative humidity [%] (SHT31) or -1 if not available
  float gps_lat;           // GPS latitude (deg)
  float gps_lon;           // GPS longitude (deg)
  float gps_alt_m;         // GPS altitude (m)
  uint32_t gps_utc_s;      // GPS UTC time (s)
  uint8_t gps_valid;       // 1 if GPS fix valid
  uint8_t gps_satellites;  // Number of satellites in fix
  uint8_t flags;           // Bit 0: imu_valid, Bit 1: temp_valid, Bit 2: humidity_valid
} csp_telemetry_packet_t;

void vTelemetryTask(void *pvParameters);
void vTelemetryTask_Step(void);

#endif  // TELEMETRY_TASK_H
