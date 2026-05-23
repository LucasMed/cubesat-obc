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
  float lux;               // Illuminance [lux]
  uint32_t rtc_timestamp;  // Unix epoch seconds (DS3231)
  int16_t bus_voltage_mv;  // Bus voltage [mV] (INA219)
  int16_t battery_mv;      // Battery voltage [mV] (ADC0/GPIO26)
  int16_t current_ma;      // Current [mA] (INA219)
  int16_t power_mw;        // Power [mW] (INA219)
  float sun_x;             // Sun sensor X intensity (0.0-1.0) or -1
  float sun_y;             // Sun sensor Y intensity (0.0-1.0) or -1
  uint8_t flags;  // Bit 0: imu_valid, Bit 1: temp_valid, Bit 2: humidity_valid, Bit 3: lux_valid,
                  // Bit 4: rtc_valid, Bit 5: sun_valid, Bit 6: power_valid, Bits 7:5: energy_state
} csp_telemetry_packet_t;

void vTelemetryTask(void *pvParameters);
void vTelemetryTask_Step(void);

#endif  // TELEMETRY_TASK_H
