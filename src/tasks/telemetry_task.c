#include "telemetry_task.h"

#include "FreeRTOS.h"
#include "config.h"
#include "task.h"

#include <stdio.h>

#ifdef PICO_BUILD
  #include "hardware/uart.h"
  #include "pico/time.h"
  #include "pico_pins.h"
#endif

#include "data_layer.h"
#include "telemetry_storage.h"

#include <csp/csp.h>

#define GN_ADDRESS 1

/* Flags byte layout:
 *   bit 0 : imu_valid
 *   bit 1 : temp_valid
 *   bit 2 : humidity_valid
 *   bits[5:3] : energy_state (ENERGY_NOMINAL=0 .. ENERGY_EMERGENCY=3)
 */
#define TLM_FLAG_IMU_VALID (1u << 0)
#define TLM_FLAG_TEMP_VALID (1u << 1)
#define TLM_FLAG_HUMIDITY_VALID (1u << 2)
#define TLM_FLAG_ENERGY_SHIFT 3u

// Core logic for telemetry (independent of FreeRTOS task loop)
void vTelemetryTask_Step(void)
{
  dl_snapshot_t snap;
  data_layer_read(&snap);

  // 1. Allocate CSP packet
  csp_packet_t *packet = csp_buffer_get(sizeof(csp_telemetry_packet_t));
  if (packet == NULL)
  {
    printf("[telemetry] Warning: No free CSP buffers\n");
    return;
  }

  // 2. Populate packet
  csp_telemetry_packet_t *tlm = (csp_telemetry_packet_t *)packet->data;
#ifdef PICO_BUILD
  tlm->timestamp_ms = to_ms_since_boot(get_absolute_time());
#else
  tlm->timestamp_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
#endif

  /* Flags: validity bits + energy state */
  tlm->flags = 0;
  if (snap.state.imu_valid)
  {
    tlm->flags |= TLM_FLAG_IMU_VALID;
  }
  if (snap.state.temp_valid)
  {
    tlm->flags |= TLM_FLAG_TEMP_VALID;
  }
  if (snap.state.humidity_valid)
  {
    tlm->flags |= TLM_FLAG_HUMIDITY_VALID;
  }
  tlm->flags |= (uint8_t)((snap.energy & 0x07u) << TLM_FLAG_ENERGY_SHIFT);

  /* Full ADCS telemetry only when not in FM_SAFE.
   * In FM_SAFE send minimal HK: temperature retained, attitude/rates zeroed. */
  if (snap.mode != FM_SAFE)
  {
    tlm->attitude[0] = snap.state.attitude[0];
    tlm->attitude[1] = snap.state.attitude[1];
    tlm->attitude[2] = snap.state.attitude[2];
    tlm->rates[0] = snap.state.rates[0];
    tlm->rates[1] = snap.state.rates[1];
    tlm->rates[2] = snap.state.rates[2];
    tlm->temp = snap.state.temp;
    tlm->humidity = snap.state.humidity;
  }
  else
  {
    tlm->attitude[0] = 0.0f;
    tlm->attitude[1] = 0.0f;
    tlm->attitude[2] = 0.0f;
    tlm->rates[0] = 0.0f;
    tlm->rates[1] = 0.0f;
    tlm->rates[2] = 0.0f;
    tlm->temp = snap.state.temp;         /* preserve HK temperature */
    tlm->humidity = snap.state.humidity; /* preserve HK humidity */
  }

  /* GPS fields */
  tlm->gps_lat = snap.gps_fix.lat;
  tlm->gps_lon = snap.gps_fix.lon;
  tlm->gps_alt_m = snap.gps_fix.alt_m;
  tlm->gps_utc_s = snap.gps_fix.utc_time;
  tlm->gps_valid = snap.gps_fix.valid ? 1 : 0;

  packet->length = sizeof(csp_telemetry_packet_t);

  // 3. Send over CSP port connection-less
  csp_sendto(CSP_PRIO_NORM, GN_ADDRESS, TELEMETRY_PORT, TELEMETRY_PORT, CSP_O_NONE, packet);

#ifdef PICO_BUILD
  // Send plain text over UART1 (HC-12) for easy debugging
  char buf[160];
  int len = snprintf(
      buf, sizeof(buf),
      "[TLM] mode=%d att=%.1f,%.1f,%.1f temp=%.1f humidity=%.1f flags=0x%02X gps_lat=%.6f gps_lon=%.6f gps_alt=%.1f gps_valid=%d",
      snap.mode, tlm->attitude[0], tlm->attitude[1], tlm->attitude[2], tlm->temp, tlm->humidity,
      tlm->flags, tlm->gps_lat, tlm->gps_lon, tlm->gps_alt_m, tlm->gps_valid);
  uart_puts(uart1, buf);
  uart_puts(uart1, "\r\n");
#endif

  printf("[telemetry] Tx mode=%d att=[%.1f,%.1f,%.1f] flags=0x%02X\n", snap.mode, tlm->attitude[0],
         tlm->attitude[1], tlm->attitude[2], tlm->flags);

  // Store telemetry to W25Q64 flash for later recovery
  telemetry_record_t record;
  record.timestamp = tlm->timestamp_ms / 1000;  // Convert ms to seconds
  record.sequence = 0;                          // Will be auto-incremented by storage
  record.roll = tlm->attitude[0];
  record.pitch = tlm->attitude[1];
  record.yaw = tlm->attitude[2];
  record.gyro_x = tlm->rates[0];
  record.gyro_y = tlm->rates[1];
  record.gyro_z = tlm->rates[2];
  record.acc_x = 0.0f;  // Not in current telemetry packet
  record.acc_y = 0.0f;
  record.acc_z = 0.0f;
  record.mag_x = 0.0f;  // Not in current telemetry packet
  record.mag_y = 0.0f;
  record.mag_z = 0.0f;
  record.temperature = tlm->temp;
  record.humidity = tlm->humidity;
  record.flags = tlm->flags;

  if (!telemetry_storage_store(&record))
  {
    printf("[telemetry] Warning: Failed to store to flash\n");
  }
}

// Telemetry task: sends telemetry at 1 Hz
void vTelemetryTask(void *pvParameters)
{
  (void)pvParameters;
  /* cppcheck-suppress unreadVariable -- updated each cycle by vTaskDelayUntil */
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(1000);  // 1 Hz

  printf("[telemetry_task] Started\n");
  fflush(stdout);

  // Initialize telemetry storage
  if (!telemetry_storage_init())
  {
    printf("[telemetry_task] Warning: Telemetry storage init failed\n");
  }
  else
  {
    telemetry_storage_stats_t stats;
    telemetry_storage_get_stats(&stats);
    printf("[telemetry_task] Storage: %lu records available\n",
           (unsigned long)stats.records_written);
  }

  while (1)
  {
    vTelemetryTask_Step();
    /* cppcheck-suppress unreadVariable -- macro writes back updated wake time */
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}
