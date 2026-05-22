#include "telemetry_task.h"

#include "FreeRTOS.h"
#include "config.h"
#include "task.h"

#include <stdio.h>
#include <string.h>

#ifdef PICO_BUILD
  #include "../drivers/uart/pico_usart.h"
  #include "hardware/uart.h"
  #include "pico/time.h"
  #include "pico_pins.h"
#endif

#include "data_layer.h"
#include "eps.h"
#include "telemetry_storage.h"
#include "wcet_profiler.h"

#include <csp/csp.h>

#define GN_ADDRESS 1

/* Flags byte layout:
 *   bit 0 : imu_valid
 *   bit 1 : temp_valid
 *   bit 2 : humidity_valid
 *   bit 3 : lux_valid
 *   bit 4 : rtc_valid
 *   bits[7:5] : energy_state (ENERGY_NOMINAL=0 .. ENERGY_EMERGENCY=3)
 *
 * NOTE: sun_valid and power_valid are NOT in the per-packet flags byte.
 *       They are set once at init in the data layer and always true when
 *       the hardware is operational.  Keeping them out of the flags byte
 *       avoids bit overlap with the energy_state field (bits 7:5).
 */
#define TLM_FLAG_IMU_VALID (1u << 0)
#define TLM_FLAG_TEMP_VALID (1u << 1)
#define TLM_FLAG_HUMIDITY_VALID (1u << 2)
#define TLM_FLAG_LUX_VALID (1u << 3)
#define TLM_FLAG_RTC_VALID (1u << 4)
#define TLM_FLAG_ENERGY_SHIFT 5u

/* CRC8 calculation (Maxim/Dallas style) */
static uint8_t crc8_calc(const uint8_t *data, uint16_t len)
{
  uint8_t crc = 0;
  for (uint16_t i = 0; i < len; i++)
  {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++)
    {
      if (crc & 0x80)
      {
        crc = (crc << 1) ^ 0x31;  // Polynomial for CRC8- Maxim
      }
      else
      {
        crc <<= 1;
      }
    }
  }
  return crc;
}

/* Convert byte to hex char */
static char byte_to_hex(uint8_t b)
{
  return (b < 10) ? ('0' + b) : ('A' + b - 10);
}

/* Telemetry output format storage - defined in header */
static volatile telemetry_format_t g_tlm_format = TLM_FORMAT_TEXT;

/* Implementation of format control functions */
/**
 * @brief Set telemetry output format.
 *
 * Called from command handler.
 *
 * @param format  TLM_FORMAT_TEXT or TLM_FORMAT_JSON
 */
void telemetry_set_format(telemetry_format_t format)
{
  g_tlm_format = format;
  printf("[telemetry] Format switched to %s\n", (format == TLM_FORMAT_JSON) ? "JSON" : "TEXT");
}

/**
 * @brief Get current telemetry format.
 *
 * @return Current format.
 */
telemetry_format_t telemetry_get_format(void)
{
  return g_tlm_format;
}

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
  if (snap.state.lux_valid)
  {
    tlm->flags |= TLM_FLAG_LUX_VALID;
  }
  if (snap.state.rtc_valid)
  {
    tlm->flags |= TLM_FLAG_RTC_VALID;
  }
  /* Note: sun_valid and power_valid are always true at runtime
   * (checked once at init), so we don't waste flag bits on them.
   * The ground station always displays sun and power data. */
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
  tlm->gps_satellites = snap.gps_fix.satellites;

  /* Light sensor */
  tlm->lux = snap.state.lux;

  /* RTC timestamp */
  tlm->rtc_timestamp = snap.state.rtc_timestamp;

  /* Power monitoring (INA219) */
  tlm->bus_voltage_mv = snap.state.bus_voltage_mv;
  tlm->current_ma = (int16_t)(snap.state.current_ua / 1000);
  tlm->power_mw = (int16_t)(snap.state.power_uw / 1000);

  /* Battery voltage via ADC (raw battery, not regulated bus) */
  {
    eps_snapshot_t eps;
    if (eps_snapshot_get(&eps) == 0)
    {
      tlm->battery_mv = (int16_t)(eps.vbatt * 1000.0f);
    }
    else
    {
      tlm->battery_mv = -1; /* EPS not initialised */
    }
  }

  /* Sun sensor */
  tlm->sun_x = snap.state.sun_x;
  tlm->sun_y = snap.state.sun_y;

  packet->length = sizeof(csp_telemetry_packet_t);

  // 3. Send over CSP port connection-less
  csp_sendto(CSP_PRIO_NORM, GN_ADDRESS, TELEMETRY_PORT, TELEMETRY_PORT, CSP_O_NONE, packet);

#ifdef PICO_BUILD
  // Send telemetry over UART1 (HC-12) in configured format
  char buf[512];

  if (g_tlm_format == TLM_FORMAT_JSON)
  {
    // JSON format for simulator (simplified, with CRC8)
    int16_t current_abs = (tlm->current_ma < 0) ? -tlm->current_ma : tlm->current_ma;
    int16_t power_abs = (tlm->power_mw < 0) ? -tlm->power_mw : tlm->power_mw;
    int16_t solar_v = snap.state.solar_voltage_mv;
    int16_t solar_i = (snap.state.solar_current_ua < 0)
                          ? (int16_t)(-(snap.state.solar_current_ua / 1000))
                          : (int16_t)(snap.state.solar_current_ua / 1000);
    int16_t solar_p = (snap.state.solar_power_uw < 0)
                          ? (int16_t)(-(snap.state.solar_power_uw / 1000))
                          : (int16_t)(snap.state.solar_power_uw / 1000);

    int len = snprintf(
        buf, sizeof(buf),
        "[JSON] {ts:%lu,r:%lu,m:%d,a:%.1f,%.1f,%.1f,t:%.1f,h:%.1f,l:%.1f,g:%.6f,%.6f,%.1f,v:%d,s:%d,p:%d,%d,%d,b:%d,sp:%d,%d,%d,sx:%.2f,sy:%.2f,f:%u",
        (unsigned long)tlm->timestamp_ms, (unsigned long)tlm->rtc_timestamp, snap.mode, tlm->attitude[0], tlm->attitude[1],
        tlm->attitude[2], tlm->temp, tlm->humidity, tlm->lux, tlm->gps_lat, tlm->gps_lon,
        tlm->gps_alt_m, tlm->gps_valid, tlm->gps_satellites, tlm->bus_voltage_mv, current_abs,
        power_abs, tlm->battery_mv, solar_v, solar_i, solar_p, tlm->sun_x, tlm->sun_y, tlm->flags);

    uint8_t json_crc = crc8_calc((const uint8_t *)buf + 7, len - 9);  // CRC on data only
    int pos = len;
    buf[pos++] = ',';
    buf[pos++] = 'c';
    buf[pos++] = ':';
    buf[pos++] = byte_to_hex(json_crc >> 4);
    buf[pos++] = byte_to_hex(json_crc & 0x0F);
    buf[pos++] = '}';
    buf[pos++] = '\r';
    buf[pos++] = '\n';
    buf[pos] = '\0';
  }
  else
  {
    // TEXT format (compact, with CRC8)
    int16_t current_abs = (tlm->current_ma < 0) ? -tlm->current_ma : tlm->current_ma;
    int16_t power_abs = (tlm->power_mw < 0) ? -tlm->power_mw : tlm->power_mw;
    int16_t solar_v = snap.state.solar_voltage_mv;
    int16_t solar_i = (snap.state.solar_current_ua < 0)
                          ? (int16_t)(-(snap.state.solar_current_ua / 1000))
                          : (int16_t)(snap.state.solar_current_ua / 1000);
    int16_t solar_p = (snap.state.solar_power_uw < 0)
                          ? (int16_t)(-(snap.state.solar_power_uw / 1000))
                          : (int16_t)(snap.state.solar_power_uw / 1000);

    int len = snprintf(buf, sizeof(buf),
                       "[TLM] m=%d a=%.1f,%.1f,%.1f t=%.1f h=%.1f l=%.1f r=%lu f=0x%02X "
                       "g=%.6f,%.6f,%.1f v=%d s=%d p=%d,%d,%d b=%d sp=%d,%d,%d "
                       "sx=%.2f sy=%.2f c=  \r\n",
                       snap.mode, tlm->attitude[0], tlm->attitude[1], tlm->attitude[2], tlm->temp,
                       tlm->humidity, tlm->lux, (unsigned long)tlm->rtc_timestamp, tlm->flags,
                       tlm->gps_lat, tlm->gps_lon, tlm->gps_alt_m, tlm->gps_valid,
                       tlm->gps_satellites, tlm->bus_voltage_mv, current_abs, power_abs,
                       tlm->battery_mv, solar_v, solar_i, solar_p, tlm->sun_x, tlm->sun_y);
    // Calculate CRC and insert (skip "[TLM] " = 6 chars, CRC replaces two spaces after c=)
    uint8_t text_crc = crc8_calc((const uint8_t *)buf + 6, len - 7);  // -7 for " c=  \r\n"
    buf[len - 4] = byte_to_hex(text_crc >> 4);                        // Replace 1st space
    buf[len - 3] = byte_to_hex(text_crc & 0x0F);                      // Replace 2nd space
    (void)len;
  }

  uart1_puts_safe(buf);

  // Debug: show first 100 chars of generated buffer
  buf[100] = '\0';
  if (g_tlm_format == TLM_FORMAT_JSON)
    printf("[telemetry] UART buf (first 100): %s\n", buf + 7);  // Skip "[JSON] " prefix
  else
    printf("[telemetry] UART buf (first 100): %s\n", buf + 6);  // Skip "[TLM] " prefix
#endif

  // Debug output to UART0
  printf(
      "[telemetry] Tx mode=%d att=[%.1f,%.1f,%.1f] temp=%.1f lux=%.1f batt=%dmV bus=%dmV flags=0x%02X\n",
      snap.mode, tlm->attitude[0], tlm->attitude[1], tlm->attitude[2], tlm->temp, tlm->lux,
      tlm->battery_mv, tlm->bus_voltage_mv, tlm->flags);

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
    wcet_task_begin(WCET_TASK_TELEMETRY);
    vTelemetryTask_Step();
    wcet_task_end(WCET_TASK_TELEMETRY);
    /* cppcheck-suppress unreadVariable -- macro writes back updated wake time */
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}
