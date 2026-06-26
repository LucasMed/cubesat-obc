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

#include "../protocols/telemetry_packet.h"
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

/**
 * Build a CSP telemetry packet from the current DLA snapshot.
 *
 * Allocates a CSP buffer and populates it with sensor, GPS, power, and
 * payload housekeeping data from the snapshot.  In FM_SAFE the attitude
 * and rate fields are zeroed (HK-only mode).
 *
 * @param snap  Current data-layer snapshot
 * @return      Populated CSP packet, or NULL if buffer allocation failed
 */
static csp_packet_t *telemetry_build_packet(const dl_snapshot_t *snap)
{
  csp_packet_t *packet = csp_buffer_get(sizeof(csp_telemetry_packet_t));
  if (packet == NULL)
  {
    printf("[telemetry] Warning: No free CSP buffers\n");
    return NULL;
  }

  csp_telemetry_packet_t *tlm = (csp_telemetry_packet_t *)packet->data;
#ifdef PICO_BUILD
  tlm->timestamp_ms = to_ms_since_boot(get_absolute_time());
#else
  tlm->timestamp_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
#endif

  /* Flags: validity bits + energy state */
  tlm->flags = 0;
  if (snap->state.imu_valid)
  {
    tlm->flags |= TLM_FLAG_IMU_VALID;
  }
  if (snap->state.temp_valid)
  {
    tlm->flags |= TLM_FLAG_TEMP_VALID;
  }
  if (snap->state.humidity_valid)
  {
    tlm->flags |= TLM_FLAG_HUMIDITY_VALID;
  }
  if (snap->state.lux_valid)
  {
    tlm->flags |= TLM_FLAG_LUX_VALID;
  }
  if (snap->state.rtc_valid)
  {
    tlm->flags |= TLM_FLAG_RTC_VALID;
  }
  /* Note: sun_valid and power_valid are always true at runtime
   * (checked once at init), so we don't waste flag bits on them.
   * The ground station always displays sun and power data. */
  tlm->flags |= (uint8_t)((snap->energy & 0x07u) << TLM_FLAG_ENERGY_SHIFT);

  /* Full ADCS telemetry only when not in FM_SAFE.
   * In FM_SAFE send minimal HK: temperature retained, attitude/rates zeroed. */
  if (snap->mode != FM_SAFE)
  {
    tlm->attitude[0] = snap->state.attitude[0];
    tlm->attitude[1] = snap->state.attitude[1];
    tlm->attitude[2] = snap->state.attitude[2];
    tlm->rates[0] = snap->state.rates[0];
    tlm->rates[1] = snap->state.rates[1];
    tlm->rates[2] = snap->state.rates[2];
    tlm->temp = snap->state.temp;
    tlm->humidity = snap->state.humidity;
  }
  else
  {
    tlm->attitude[0] = 0.0f;
    tlm->attitude[1] = 0.0f;
    tlm->attitude[2] = 0.0f;
    tlm->rates[0] = 0.0f;
    tlm->rates[1] = 0.0f;
    tlm->rates[2] = 0.0f;
    tlm->temp = snap->state.temp;         /* preserve HK temperature */
    tlm->humidity = snap->state.humidity; /* preserve HK humidity */
  }

  /* GPS fields */
  tlm->gps_lat = snap->gps_fix.lat;
  tlm->gps_lon = snap->gps_fix.lon;
  tlm->gps_alt_m = snap->gps_fix.alt_m;
  tlm->gps_utc_s = snap->gps_fix.utc_time;
  tlm->gps_valid = snap->gps_fix.valid ? 1 : 0;
  tlm->gps_satellites = snap->gps_fix.satellites;

  /* Light sensor */
  tlm->lux = snap->state.lux;

  /* RTC timestamp */
  tlm->rtc_timestamp = snap->state.rtc_timestamp;

  /* Power monitoring (INA219) */
  tlm->bus_voltage_mv = snap->state.bus_voltage_mv;
  tlm->current_ma = (int16_t)(snap->state.current_ua / 1000);
  tlm->power_mw = (int16_t)(snap->state.power_uw / 1000);

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
  tlm->sun_x = snap->state.sun_x;
  tlm->sun_y = snap->state.sun_y;

  /* Payload housekeeping (FR-17) */
  tlm->mag_field[0] = snap->state.mag_field[0];
  tlm->mag_field[1] = snap->state.mag_field[1];
  tlm->mag_field[2] = snap->state.mag_field[2];
  tlm->radiation_dose = snap->state.radiation_dose;
  tlm->image_count = snap->state.image_count;
  tlm->payload_rail_enabled = snap->state.payload_rail_enabled ? 1 : 0;

  packet->length = sizeof(csp_telemetry_packet_t);
  return packet;
}

/**
 * Build and transmit a binary telemetry frame over UART1 (HC-12).
 *
 * PICO_BUILD only.  Converts the CSP telemetry fields to fixed-point
 * representation and sends with a 2-byte sync prefix.
 *
 * Format: [SYNC 0xAA 0x55] [64-byte telemetry_packet_t]
 * Total: 66 bytes @ 9600 baud ≈ 69ms.
 */
#ifdef PICO_BUILD
static void telemetry_build_binary_frame(const dl_snapshot_t *snap,
                                         const csp_telemetry_packet_t *tlm)
{
  telemetry_packet_t bin;
  memset(&bin, 0, sizeof(bin));

  /* Convert float/int fields to fixed-point */
  bin.ts = (uint32_t)tlm->timestamp_ms;
  bin.rtc = (uint32_t)tlm->rtc_timestamp;
  bin.mode = (uint8_t)snap->mode;
  bin.roll = (int16_t)(tlm->attitude[0] * 10.0f);
  bin.pitch = (int16_t)(tlm->attitude[1] * 10.0f);
  bin.yaw = (int16_t)(tlm->attitude[2] * 10.0f);
  bin.temp = (int16_t)(tlm->temp * 10.0f);
  bin.humidity = (int16_t)(tlm->humidity * 10.0f);
  bin.lux = (uint16_t)(tlm->lux);
  bin.gps_lat = (int32_t)(tlm->gps_lat * 10000000.0);
  bin.gps_lon = (int32_t)(tlm->gps_lon * 10000000.0);
  bin.gps_alt = (int16_t)(tlm->gps_alt_m);
  bin.gps_valid = (uint8_t)tlm->gps_valid;
  bin.gps_sats = (uint8_t)tlm->gps_satellites;
  bin.bus_mv = (int16_t)(tlm->bus_voltage_mv);
  bin.bus_ma = (int16_t)((tlm->current_ma < 0) ? -tlm->current_ma : tlm->current_ma);
  bin.bus_mw = (int16_t)((tlm->power_mw < 0) ? -tlm->power_mw : tlm->power_mw);
  bin.battery_mv = (int16_t)(tlm->battery_mv);
  bin.solar_mv = (int16_t)(snap->state.solar_voltage_mv);
  bin.solar_ma = (int16_t)((snap->state.solar_current_ua < 0)
                               ? (int16_t)(-(snap->state.solar_current_ua / 1000))
                               : (int16_t)(snap->state.solar_current_ua / 1000));
  bin.solar_mw = (int16_t)((snap->state.solar_power_uw < 0)
                               ? (int16_t)(-(snap->state.solar_power_uw / 1000))
                               : (int16_t)(snap->state.solar_power_uw / 1000));
  bin.sun_x = (int16_t)(tlm->sun_x * 100.0f);
  bin.sun_y = (int16_t)(tlm->sun_y * 100.0f);

  /* Payload housekeeping (FR-17) */
  bin.mag_x = (int16_t)(tlm->mag_field[0] * 100.0f);
  bin.mag_y = (int16_t)(tlm->mag_field[1] * 100.0f);
  bin.mag_z = (int16_t)(tlm->mag_field[2] * 100.0f);
  bin.radiation = (uint16_t)(tlm->radiation_dose);
  bin.image_count = (uint16_t)(tlm->image_count);
  bin.payload_rail_enabled = (uint8_t)(tlm->payload_rail_enabled);

  bin.flags = (uint8_t)tlm->flags;
  bin.crc = tlm_crc8((const uint8_t *)&bin, sizeof(bin) - 1);

  printf("[CRC] OBC crc=0x%02X ts=%lu rtc=%lu mode=%d flags=0x%02X bus_mv=%d batt_mv=%d temp=%d\n",
         bin.crc, (unsigned long)bin.ts, (unsigned long)bin.rtc, bin.mode, bin.flags, bin.bus_mv,
         bin.battery_mv, bin.temp);

  /* Send sync word + packet in one atomic write
   *
   * IMPORTANT: sync[2] and bin[64] MUST be concatenated into a single buffer
   * so the UART1 lock is acquired once.  Two separate uart1_write_buf calls
   * would release the lock between them, allowing other tasks (CSP/KISS,
   * command_task) to interleave data and corrupt the ground-station frame. */
  uint8_t frame[2 + TLM_PACKET_SIZE];
  frame[0] = TLM_SYNC_BYTE_1;
  frame[1] = TLM_SYNC_BYTE_2;
  memcpy(frame + 2, &bin, sizeof(bin));
  uart1_write_buf(frame, sizeof(frame));
}
#endif  // PICO_BUILD

/**
 * Store the current telemetry record to W25Q64 flash for later recovery.
 */
static void telemetry_store_record(const dl_snapshot_t *snap,
                                   const csp_telemetry_packet_t *tlm)
{
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
  record.mag_x = snap->state.mag_field[0];
  record.mag_y = snap->state.mag_field[1];
  record.mag_z = snap->state.mag_field[2];
  record.temperature = tlm->temp;
  record.humidity = tlm->humidity;
  record.radiation_dose = snap->state.radiation_dose;
  record.image_count = snap->state.image_count;
  record.payload_rail_enabled = snap->state.payload_rail_enabled ? 1 : 0;
  record.flags = tlm->flags;

  if (!telemetry_storage_store(&record))
  {
    printf("[telemetry] Warning: Failed to store to flash\n");
  }
}

// Core logic for telemetry (independent of FreeRTOS task loop)
void vTelemetryTask_Step(void)
{
  dl_snapshot_t snap;
  data_layer_read(&snap);

  csp_packet_t *packet = telemetry_build_packet(&snap);
  if (packet == NULL)
  {
    return;
  }

  csp_telemetry_packet_t *tlm = (csp_telemetry_packet_t *)packet->data;

  // Send over CSP port connection-less (host build only).
  // On PICO_BUILD, CSP KISS shares UART1 with the binary telemetry frame.
  // Sending both would corrupt the binary frame — the ground station state
  // machine cannot distinguish CSP KISS data from binary frame sync bytes.
#ifndef PICO_BUILD
  csp_sendto(CSP_PRIO_NORM, GN_ADDRESS, TELEMETRY_PORT, TELEMETRY_PORT, CSP_O_NONE, packet);
#endif

#ifdef PICO_BUILD
  telemetry_build_binary_frame(&snap, tlm);
#endif

  // Debug output to UART0
  printf(
      "[telemetry] Tx mode=%d att=[%.1f,%.1f,%.1f] temp=%.1f lux=%.1f batt=%dmV bus=%dmV flags=0x%02X\n",
      snap.mode, tlm->attitude[0], tlm->attitude[1], tlm->attitude[2], tlm->temp, tlm->lux,
      tlm->battery_mv, tlm->bus_voltage_mv, tlm->flags);

  telemetry_store_record(&snap, tlm);

  // On PICO, csp_sendto was not called so the packet buffer was never freed.
  // Free it now — all tlm accesses are done.
#ifdef PICO_BUILD
  csp_buffer_free(packet);
#endif
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
