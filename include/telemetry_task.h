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
  float sun_x;               // Sun sensor X intensity (0.0-1.0) or -1
  float sun_y;               // Sun sensor Y intensity (0.0-1.0) or -1
  float mag_field[3];        // [82] Magnetometer [µT]
  float radiation_dose;      // [94] Radiation dose
  uint16_t image_count;      // [98] Images on payload SD
  uint8_t payload_rail_enabled; // [100] 1=rail on
  uint8_t flags;             // [101] Bit 0: imu_valid, Bit 1: temp_valid,
                             // Bit 2: humidity_valid, Bit 3: lux_valid,
                             // Bit 4: rtc_valid, Bit 5: sun_valid,
                             // Bit 6: power_valid, Bits 7:5: energy_state
} csp_telemetry_packet_t;

_Static_assert(sizeof(csp_telemetry_packet_t) == 98,
               "csp_telemetry_packet_t must be 98 bytes (FR-17)");

void vTelemetryTask(void *pvParameters);
void vTelemetryTask_Step(void);

#endif  // TELEMETRY_TASK_H
