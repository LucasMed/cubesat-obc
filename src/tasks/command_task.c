#include "command_task.h"

#include "FreeRTOS.h"
#include "bh1750.h"
#include "data_layer.h"
#include "drivers/imu/imu_calib.h"
#include "drivers/imu/mpu6050.h"
#include "drivers/mag/hmc5883l.h"
#include "drivers/temperature.h"
#include "ds3231.h"
#include "ekf.h"
#include "fault_manager.h"
#include "flight_mode.h"
#include "gps_driver.h"
#include "ina219.h"
#include "mag_calib.h"
#include "payload_task.h"
#include "sht31.h"
#include "sun_sensor.h"
#include "task.h"
#include "telemetry_task.h"
#include "wcet_profiler.h"

#include <csp/csp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * When compiling the unit‑test variant we want command_task.c to call
 * into mock functions defined by the test harness instead of the real
 * CSP library.  The wrapper file used by the tests defines CSP_MOCK
 * before including this C file, causing the macros below to kick in.
 */
#ifdef CSP_MOCK
  #define csp_send mock_csp_send
  #define csp_buffer_free mock_csp_buffer_free
  #define csp_conn_src mock_csp_conn_src
#endif

#ifdef PICO_BUILD
  #include "../drivers/uart/pico_usart.h"
  #include "bh1750.h"
  #include "drivers/i2c_interface.h"
  #include "hardware/uart.h"
  #include "hardware/watchdog.h"
  #include "pico/stdlib.h"
  #include "pico_pins.h"
#endif

#ifdef PICO_BUILD
static void process_text_command(const char *cmd)
{
  if (strncmp(cmd, "REBOOT", 6) == 0)
  {
    uart1_puts_safe("[CMD] REBOOT OK\r\n");
    printf("[command_task] Text command: REBOOT\r\n");
    vTaskDelay(pdMS_TO_TICKS(100));
    watchdog_reboot(0, 0, 10);
  }
  else if (strncmp(cmd, "STATUS", 6) == 0)
  {
    flight_mode_t mode = fmm_get_mode();
    energy_state_t energy = data_layer_get_energy_state();
    dl_snapshot_t snapshot = {0};
    data_layer_read(&snapshot);

    uart1_acquire_lock();

    char buf[96];
    const char *mode_names[] = {"BOOT", "SAFE", "DETUMBLE", "NOMINAL", "DIAG", "PAYLOAD"};
    const char *energy_names[] = {"NOMINAL", "LOW", "CRITICAL", "EMERGENCY"};
    snprintf(buf, sizeof(buf), "[CMD] SYSTEM: mode=%s energy=%s imu=%s temp=%s mag=%s\r\n",
             mode_names[mode], energy_names[energy], snapshot.state.imu_valid ? "OK" : "FAIL",
             snapshot.state.temp_valid ? "OK" : "FAIL", snapshot.state.mag_valid ? "OK" : "FAIL");
    uart1_write_unsafe(buf);

    GpsFix_t fix = {0};
    if (gps_get_last_fix(&fix))
    {
      snprintf(buf, sizeof(buf), "[CMD] GPS: v=%d lat=%.5f lon=%.5f alt=%.1f s=%d hdop=%.1f\r\n",
               fix.valid, fix.lat, fix.lon, fix.alt_m, fix.satellites, fix.hdop);
      uart1_write_unsafe(buf);
    }

    uart1_release_lock();
    printf("[command_task] Text command: STATUS\r\n");
  }
  else if (strncmp(cmd, "FAULTS", 6) == 0)
  {
    fault_level_t level = fault_get_highest_level();
    const char *level_names[] = {"OK", "WARN", "ERROR", "CRITICAL"};
    char buf[32];
    snprintf(buf, sizeof(buf), "[CMD] FAULTS: %s\r\n", level_names[level > 3 ? 0 : level]);
    uart1_puts_safe(buf);
    printf("[command_task] Text command: FAULTS\r\n");
  }
  else if (strncmp(cmd, "ECHO", 4) == 0)
  {
    uart1_puts_safe("[CMD] ECHO OK\r\n");
    printf("[command_task] Text command: ECHO\r\n");
  }
  else if (strncmp(cmd, "MAG-CAL-START", 14) == 0)
  {
    mag_calib_start();
    uart1_puts_safe("[CMD] MAG-CAL: started — rotate CubeSat in all axes\r\n");
    printf("[command_task] Text command: MAG-CAL-START\r\n");
  }
  else if (strncmp(cmd, "MAG-CAL-STOP", 13) == 0)
  {
    mag_calib_finish();
    uart1_puts_safe("[CMD] MAG-CAL: finished — offsets computed\r\n");
    printf("[command_task] Text command: MAG-CAL-STOP\r\n");
  }
  else if (strncmp(cmd, "MAG-CAL-STATUS", 15) == 0)
  {
    char buf[64];
    if (mag_calib_is_valid())
    {
      mag_calib_t cal;
      mag_calib_get(&cal);
      snprintf(buf, sizeof(buf), "[CMD] MAG-CAL: VALID offsets=%.2f,%.2f,%.2f µT\r\n",
               cal.offset[0], cal.offset[1], cal.offset[2]);
    }
    else
    {
      snprintf(buf, sizeof(buf), "[CMD] MAG-CAL: NOT CALIBRATED\r\n");
    }
    uart1_puts_safe(buf);
    printf("[command_task] Text command: MAG-CAL-STATUS\r\n");
  }
  else if (strncmp(cmd, "IMU-CAL-START", 14) == 0)
  {
    imu_calib_start();
    uart1_puts_safe(
        "[CMD] IMU-CAL: started — keep stationary for gyro, then 6 orientations for accel\r\n");
    printf("[command_task] Text command: IMU-CAL-START\r\n");
  }
  else if (strncmp(cmd, "IMU-CAL-STOP", 13) == 0)
  {
    imu_calib_finish();
    uart1_puts_safe("[CMD] IMU-CAL: finished — offsets computed\r\n");
    printf("[command_task] Text command: IMU-CAL-STOP\r\n");
  }
  else if (strncmp(cmd, "IMU-CAL-STATUS", 15) == 0)
  {
    char buf[64];
    if (imu_calib_is_valid())
    {
      imu_calib_t cal;
      imu_calib_get(&cal);
      snprintf(
          buf, sizeof(buf),
          "[CMD] IMU-CAL: VALID gyro_bias=%.4f,%.4f,%.4f rad/s accel_offset=%.3f,%.3f,%.3f g\r\n",
          cal.gyro_bias_rads[0], cal.gyro_bias_rads[1], cal.gyro_bias_rads[2], cal.accel_offset[0],
          cal.accel_offset[1], cal.accel_offset[2]);
    }
    else
    {
      snprintf(buf, sizeof(buf), "[CMD] IMU-CAL: NOT CALIBRATED\r\n");
    }
    uart1_puts_safe(buf);
    printf("[command_task] Text command: IMU-CAL-STATUS\r\n");
  }
  else if (strncmp(cmd, "IMU-CAL-SAVE", 13) == 0)
  {
    imu_calib_save_to_flash();
    uart1_puts_safe("[CMD] IMU-CAL: saved to flash\r\n");
    printf("[command_task] Text command: IMU-CAL-SAVE\r\n");
  }
  else if (strncmp(cmd, "IMU-CAL-LOAD", 12) == 0)
  {
    imu_calib_load_from_flash();
    uart1_puts_safe("[CMD] IMU-CAL: loaded from flash\r\n");
    printf("[command_task] Text command: IMU-CAL-LOAD\r\n");
  }
  else if (strncmp(cmd, "CAPTURE", 7) == 0)
  {
    uart1_puts_safe("[CMD] CAPTURE OK\r\n");
    printf("[command_task] Text command: CAPTURE\r\n");
    TaskHandle_t h_payload = xTaskGetHandle("PayloadTask");
    if (h_payload != NULL)
    {
      xTaskNotify(h_payload, PAYLOAD_NOTIFY_CAPTURE_IMAGE, eSetBits);
    }
  }
  else if (strncmp(cmd, "MODE=", 5) == 0)
  {
    int mode = atoi(cmd + 5);
    if (mode >= 0 && mode <= 3)
    {
      char buf[48];
      fmm_result_t result = fmm_request_transition((flight_mode_t)mode);
      if (result == FMM_OK)
      {
        snprintf(buf, sizeof(buf), "[CMD] MODE=%d OK\r\n", mode);
      }
      else if (result == FMM_ERR_NOT_ALLOWED)
      {
        snprintf(buf, sizeof(buf), "[CMD] MODE=%d NOT ALLOWED\r\n", mode);
      }
      else if (result == FMM_ERR_FAULT_BLOCK)
      {
        snprintf(buf, sizeof(buf), "[CMD] MODE=%d BLOCKED BY FAULT\r\n", mode);
      }
      else
      {
        snprintf(buf, sizeof(buf), "[CMD] MODE=%d FAILED\r\n", mode);
      }
      uart1_puts_safe(buf);
      printf("[command_task] MODE=%d result=%d\r\n", mode, result);
    }
    else
    {
      uart1_puts_safe("[CMD] MODE INVALID\r\n");
    }
  }
  else if (strncmp(cmd, "MODE ", 5) == 0)
  {
    /* Also support "MODE 1" (space instead of =) */
    int mode = atoi(cmd + 5);
    if (mode >= 0 && mode <= 3)
    {
      char buf[48];
      fmm_result_t result = fmm_request_transition((flight_mode_t)mode);
      if (result == FMM_OK)
      {
        snprintf(buf, sizeof(buf), "[CMD] MODE=%d OK\r\n", mode);
      }
      else if (result == FMM_ERR_NOT_ALLOWED)
      {
        snprintf(buf, sizeof(buf), "[CMD] MODE=%d NOT ALLOWED\r\n", mode);
      }
      else if (result == FMM_ERR_FAULT_BLOCK)
      {
        snprintf(buf, sizeof(buf), "[CMD] MODE=%d BLOCKED BY FAULT\r\n", mode);
      }
      else
      {
        snprintf(buf, sizeof(buf), "[CMD] MODE=%d FAILED\r\n", mode);
      }
      uart1_puts_safe(buf);
      printf("[command_task] MODE %d result=%d\r\n", mode, result);
    }
    else
    {
      uart1_puts_safe("[CMD] MODE INVALID\r\n");
    }
  }
  else if (strncmp(cmd, "HELP", 4) == 0)
  {
    uart1_puts_safe(
        "[CMD] CMDS: REBOOT|STATUS|ECHO|CAPTURE|MODE=0-3|GPS|GPSSTATS|FAULTS|LOG|RESET|HELP|I2CSCAN|BH1750_TEST|RTC_TEST|POWER_TEST|SETTIME|TLMFMT|TLMFMT=JSON|TLMFMT=TEXT\r\n");
  }
  else if (strncmp(cmd, "LOG", 3) == 0)
  {
    uart1_puts_safe("[CMD] LOG: dump not implemented\r\n");
  }
  else if (strncmp(cmd, "RESET", 5) == 0)
  {
    if (strncmp(cmd + 5, "GPS", 3) == 0)
    {
      // Check if it's COLD START (RESETGPS COLD)
      if (strncmp(cmd + 8, "COLD", 4) == 0)
      {
        gps_cold_start();
        uart1_puts_safe("[CMD] RESET GPS COLD START OK\r\n");
      }
      else
      {
        gps_reset_stats();
        uart1_puts_safe("[CMD] RESET GPS OK\r\n");
      }
    }
    else
    {
      uart1_puts_safe("[CMD] RESET: usage: RESETGPS | RESETGPS COLD\r\n");
    }
  }
  else if (strncmp(cmd, "GPSSTATS", 7) == 0)
  {
    // Debug: show GPS statistics
    const GpsStats_t *stats = gps_get_stats();
    GpsFix_t fix = {0};
    gps_get_last_fix(&fix);
    char buf[128];
    snprintf(buf, sizeof(buf),
             "[CMD] GPS stats: rx=%lu, chksum_err=%lu, parse_err=%lu, "
             "fix_valid=%lu, fix_invalid=%lu, overflow=%lu\r\n"
             "       last fix: valid=%d, sats=%u, lat=%.6f, lon=%.6f, "
             "alt=%.1f, hdop=%.1f\r\n",
             (unsigned long)stats->sentences_received, (unsigned long)stats->checksum_errors,
             (unsigned long)stats->parse_errors, (unsigned long)stats->fixes_valid,
             (unsigned long)stats->fixes_invalid, (unsigned long)stats->buffer_overflows, fix.valid,
             fix.satellites, (double)fix.lat, (double)fix.lon, (double)fix.alt_m, (double)fix.hdop);
    uart1_puts_safe(buf);
  }
  else if (strncmp(cmd, "GPS", 3) == 0)
  {
    GpsFix_t fix = {0};
    if (gps_get_last_fix(&fix))
    {
      uart1_acquire_lock();

      char buf[96];
      snprintf(buf, sizeof(buf), "[CMD] GPS: v=%d lat=%.5f lon=%.5f alt=%.1f s=%d hdop=%.1f\r\n",
               fix.valid, fix.lat, fix.lon, fix.alt_m, fix.satellites, fix.hdop);
      uart1_write_unsafe(buf);

      const GpsStats_t *stats = gps_get_stats();
      snprintf(buf, sizeof(buf),
               "[CMD] GPS STATS: rx=%u chk_err=%u inv=%u valid=%u overflow=%u\r\n",
               (unsigned)stats->sentences_received, (unsigned)stats->checksum_errors,
               (unsigned)stats->fixes_invalid, (unsigned)stats->fixes_valid,
               (unsigned)stats->buffer_overflows);
      uart1_write_unsafe(buf);

      uart1_release_lock();
    }
    else
    {
      char buf[64];
      uint8_t sats = gps_get_satellites_in_view();
      snprintf(buf, sizeof(buf), "[CMD] GPS: no fix sats=%d\r\n", sats);
      uart1_puts_safe(buf);
    }
  }
  else if (strncmp(cmd, "RTC_TEST", 8) == 0)
  {
    char buf[96];
    /* Use data_layer_read() to get RTC from sensor_read_task */
    dl_snapshot_t snap;
    data_layer_read(&snap);

    if (snap.state.rtc_valid)
    {
      snprintf(buf, sizeof(buf), "[CMD] RTC: %u\r\n", (unsigned)snap.state.rtc_timestamp);
    }
    else
    {
      snprintf(buf, sizeof(buf), "[CMD] RTC: no data\r\n");
    }
    uart1_puts_safe(buf);
  }
  else if (strncmp(cmd, "SETTIME ", 8) == 0)
  {
    /* Format: SETTIME YYYY MM DD HH MM SS */
    int year, month, day, hour, minute, second;
    int n = sscanf(cmd + 8, "%d %d %d %d %d %d", &year, &month, &day, &hour, &minute, &second);
    if (n == 6)
    {
      if (ds3231_set_time((uint16_t)year, (uint8_t)month, (uint8_t)day, (uint8_t)hour,
                          (uint8_t)minute, (uint8_t)second))
      {
        uart1_puts_safe("[CMD] SETTIME OK\r\n");
      }
      else
      {
        uart1_puts_safe("[CMD] SETTIME FAILED\r\n");
      }
    }
    else
    {
      uart1_puts_safe("[CMD] SETTIME: usage: SETTIME YYYY MM DD HH MM SS\r\n");
    }
  }
  else if (strncmp(cmd, "I2CSCAN", 7) == 0)
  {
    uart1_puts_safe("[CMD] I2C: scanning...\r\n");
    printf("[command_task] Text command: I2CSCAN\r\n");
    int found = i2c_bus_scan(0x03, 0x77);
    char buf[128];
    snprintf(buf, sizeof(buf), "[CMD] I2C: found %d device(s)\r\n", found);
    uart1_puts_safe(buf);
  }
  else if (strncmp(cmd, "BH1750_TEST", 11) == 0)
  {
    // Parse optional address argument
    uint8_t addr = BH1750_ADDR_DEFAULT;  // 0x23
    if (strlen(cmd) > 12 && cmd[12] == '5' && cmd[13] == 'C')
    {
      addr = 0x5C;
    }
    char buf[96];
    snprintf(buf, sizeof(buf), "[CMD] BH1750: testing 0x%02X...\r\n", addr);
    uart1_puts_safe(buf);

    // Try to read with OT_H_RES2 command
    uint8_t cmd_byte = BH1750_CMD_OT_H_RES2;
    uint8_t data[2];
    int ret = i2c_bus_write_read(addr, &cmd_byte, 1, data, 2);

    if (ret == 0)
    {
      uint16_t raw = ((uint16_t)data[0] << 8) | data[1];
      float lux = (float)raw / 1.2f;
      snprintf(buf, sizeof(buf), "[CMD] BH1750: raw=%d lux=%.1f\r\n", raw, (double)lux);
    }
    else
    {
      snprintf(buf, sizeof(buf), "[CMD] BH1750: no response (err=%d)\r\n", ret);
    }
    uart1_puts_safe(buf);
  }
  else if (strncmp(cmd, "POWER_TEST", 10) == 0)
  {
    char buf[96];
    ina219_data_t data;
    snprintf(buf, sizeof(buf), "[CMD] POWER: reading INA219...\r\n");
    uart1_puts_safe(buf);

    if (ina219_read_power(&data))
    {
      int bus_v = data.bus_voltage_mv;
      int curr_ma = (abs(data.current_ua) + 500) / 1000;  // Round to nearest mA
      int pow_mw = abs(data.power_uw) / 1000;
      snprintf(buf, sizeof(buf), "[CMD] POWER: V=%d mV, I=%d mA, P=%d mW\r\n", bus_v, curr_ma,
               pow_mw);
    }
    else
    {
      snprintf(buf, sizeof(buf), "[CMD] POWER: read failed\r\n");
    }
    uart1_puts_safe(buf);
  }
  else if (strncmp(cmd, "SOLAR_TEST", 10) == 0)
  {
    char buf[96];
    ina219_data_t data;
    snprintf(buf, sizeof(buf), "[CMD] SOLAR: reading solar INA219...\r\n");
    uart1_puts_safe(buf);

    if (ina219_solar_read_power(&data))
    {
      int bus_v = data.bus_voltage_mv;
      int curr_ma = (abs(data.current_ua) + 500) / 1000;
      int pow_mw = abs(data.power_uw) / 1000;
      snprintf(buf, sizeof(buf), "[CMD] SOLAR: V=%d mV, I=%d mA, P=%d mW\r\n", bus_v, curr_ma,
               pow_mw);
    }
    else
    {
      snprintf(buf, sizeof(buf), "[CMD] SOLAR: read failed\r\n");
    }
    uart1_puts_safe(buf);
  }
  else if (strncmp(cmd, "SHT31_TEST", 10) == 0)
  {
    char buf[96];
    /* Use data_layer_read() to get temperature/humidity from sensor_read_task */
    dl_snapshot_t snap;
    data_layer_read(&snap);

    if (snap.state.temp_valid)
    {
      if (snap.state.humidity_valid)
      {
        snprintf(buf, sizeof(buf), "[CMD] SHT31: temp=%.1fC humidity=%.1f%%\r\n",
                 (double)snap.state.temp, (double)snap.state.humidity);
      }
      else
      {
        snprintf(buf, sizeof(buf), "[CMD] SHT31: temp=%.1fC humidity=N/A\r\n",
                 (double)snap.state.temp);
      }
    }
    else
    {
      snprintf(buf, sizeof(buf), "[CMD] SHT31: no data\r\n");
    }
    uart1_puts_safe(buf);
  }
  else if (strncmp(cmd, "TLMFMT=", 7) == 0)
  {
    /* Set telemetry format: TEXT or JSON */
    if (strncmp(cmd + 7, "JSON", 4) == 0)
    {
      telemetry_set_format(TLM_FORMAT_JSON);
      uart1_puts_safe("[CMD] TLMFMT=JSON OK\r\n");
    }
    else if (strncmp(cmd + 7, "TEXT", 4) == 0)
    {
      telemetry_set_format(TLM_FORMAT_TEXT);
      uart1_puts_safe("[CMD] TLMFMT=TEXT OK\r\n");
    }
    else
    {
      uart1_puts_safe("[CMD] TLMFMT: usage: TLMFMT=TEXT or TLMFMT=JSON\r\n");
    }
  }
  else if (strncmp(cmd, "TLMFMT", 6) == 0)
  {
    /* Show current format */
    telemetry_format_t fmt = telemetry_get_format();
    const char *fmt_name = (fmt == TLM_FORMAT_JSON) ? "JSON" : "TEXT";
    char buf[32];
    snprintf(buf, sizeof(buf), "[CMD] TLMFMT: %s\r\n", fmt_name);
    uart1_puts_safe(buf);
  }
  else
  {
    uart1_puts_safe("[CMD] UNKNOWN CMD\r\n");
  }
}

static void uart1_listen(void)
{
  static char buffer[64];
  static int pos = 0;

  while (uart_is_readable(uart1))
  {
    char c = uart_getc(uart1);

    if (c == '\r' || c == '\n')
    {
      if (pos > 0)
      {
        buffer[pos] = '\0';
        process_text_command(buffer);
        pos = 0;
      }
    }
    else if (c != '\0' && pos < (int)(sizeof(buffer) - 1))
    {
      buffer[pos++] = c;
    }
  }
}
#endif

void process_command_packet(csp_conn_t *conn, csp_packet_t *packet)
{
  // Check length to ensure we can read cmd_id
  if (packet->length < 1)
  {
    csp_buffer_free(packet);
    return;
  }

  const csp_command_packet_t *cmd = (const csp_command_packet_t *)packet->data;
  printf("[command_task] Received CMD_ID=%d from Addr=%d\n", cmd->cmd_id, csp_conn_src(conn));

  switch (cmd->cmd_id)
  {
  case CMD_ECHO:
    printf("[command_task] Executing ECHO command (payload '%.*s')\n", packet->length - 1,
           cmd->payload);
    // Echo back the same packet
    csp_send(conn, packet);
    packet = NULL;  // csp_send frees the packet or takes ownership
    break;

  case CMD_REBOOT:
    printf("[command_task] Executing REBOOT command. Rebooting system...\n");
    vTaskDelay(pdMS_TO_TICKS(100));  // allow logs to flush
#ifdef PICO_BUILD
    watchdog_reboot(0, 0, 10);
#else
    printf("Simulating reboot on host.\n");
#endif
    break;

  case CMD_SET_MODE:
  {
    flight_mode_t new_mode = (flight_mode_t)cmd->payload[0];
    printf("[command_task] Executing SET_MODE (mode=%d)\n", new_mode);
    fmm_request_transition(new_mode);
    break;
  }

  case CMD_PAYLOAD_CAPTURE:
  {
    printf("[command_task] Executing PAYLOAD_CAPTURE\n");
    TaskHandle_t h_payload = xTaskGetHandle("PayloadTask");
    if (h_payload != NULL)
    {
      xTaskNotify(h_payload, PAYLOAD_NOTIFY_CAPTURE_IMAGE, eSetBits);
    }
    break;
  }

  case CMD_GPS_STATUS:
  {
    printf("[command_task] Executing GPS_STATUS\n");

    GpsFix_t fix = {0};
    gps_get_last_fix(&fix);
    const GpsStats_t *stats = gps_get_stats();

    gps_status_response_t resp = {.valid = fix.valid ? 1 : 0,
                                  .lat_scaled = (int32_t)(fix.lat * 1000000),
                                  .lon_scaled = (int32_t)(fix.lon * 1000000),
                                  .alt_scaled = (int32_t)(fix.alt_m * 100),
                                  .satellites = fix.satellites,
                                  .hdop_scaled = (uint8_t)(fix.hdop * 10),
                                  .uptime_ms = fix.timestamp_ms,
                                  .sentences = (uint16_t)stats->sentences_received,
                                  .checksum_err = (uint8_t)stats->checksum_errors,
                                  .fixes_invalid = (uint8_t)stats->fixes_invalid};

    memcpy(cmd->payload, &resp, sizeof(resp));
    packet->length = sizeof(resp) + 1;
    csp_send(conn, packet);
    packet = NULL;
    break;
  }

  case CMD_STATUS:
  {
    printf("[command_task] Executing STATUS\n");

    flight_mode_t mode = fmm_get_mode();
    energy_state_t energy = data_layer_get_energy_state();

    dl_snapshot_t snapshot = {0};
    data_layer_read(&snapshot);

    system_status_response_t resp = {
        .mode = (uint8_t)mode,
        .energy = (uint8_t)energy,
        .flags = (snapshot.state.imu_valid ? 0x01 : 0) | (snapshot.state.temp_valid ? 0x02 : 0) |
                 (snapshot.state.mag_valid ? 0x04 : 0),
        .heap_free = 0,
        .uptime_sec = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS / 1000),
        .fault_count = 0};

    memcpy(cmd->payload, &resp, sizeof(resp));
    packet->length = sizeof(resp) + 1;
    csp_send(conn, packet);
    packet = NULL;
    break;
  }

  case CMD_FAULT_LIST:
  {
    printf("[command_task] Executing FAULT_LIST\n");

    fault_entry_t faults[4] = {0};
    uint8_t count = 0;

    fault_level_t level = fault_get_highest_level();
    if (level > 0)
    {
      faults[0].fault_id = 0xFF;
      faults[0].level = (uint8_t)level;
      faults[0].count = 1;
      count = 1;
    }

    memcpy(cmd->payload, &faults, count * sizeof(fault_entry_t));
    packet->length = count * sizeof(fault_entry_t) + 1;
    csp_send(conn, packet);
    packet = NULL;
    break;
  }

  case CMD_TELEMETRY_REQ:
  {
    printf("[command_task] Executing TELEMETRY_REQ\n");
    TaskHandle_t h_tlm = xTaskGetHandle("TelemetryTask");
    if (h_tlm != NULL)
    {
      xTaskNotify(h_tlm, 1, eSetBits);
    }
    csp_send(conn, packet);
    packet = NULL;
    break;
  }

  case CMD_LOG_DUMP:
  {
    printf("[command_task] Executing LOG_DUMP\n");
    uint8_t count = cmd->payload[0];
    if (count == 0 || count > 16)
    {
      count = 8;
    }
    uint8_t resp = count;
    memcpy(cmd->payload, &resp, 1);
    packet->length = 2;
    csp_send(conn, packet);
    packet = NULL;
    break;
  }

  case CMD_SENSOR_RESET:
  {
    printf("[command_task] Executing SENSOR_RESET\n");
    uint8_t sensor_id = cmd->payload[0];
    uint8_t result = 0;
    switch (sensor_id)
    {
    case 0:
    case 1:
    case 2:
    case 3:
      result = 1;
      break;
    default:
      result = 0;
      break;
    }
    memcpy(cmd->payload, &result, 1);
    packet->length = 2;
    csp_send(conn, packet);
    packet = NULL;
    break;
  }

  case CMD_GPS_RESET_STATS:
  {
    printf("[command_task] Executing GPS_RESET_STATS\n");
    gps_reset_stats();
    uint8_t resp = 1;
    memcpy(cmd->payload, &resp, 1);
    packet->length = 2;
    csp_send(conn, packet);
    packet = NULL;
    break;
  }

  case CMD_TELEMETRY_DUMP:
  {
    printf("[command_task] Executing TELEMETRY_DUMP\n");

    uint32_t start_seq = 0;
    if (packet->length >= 5)
    {
      memcpy(&start_seq, cmd->payload, sizeof(start_seq));
    }

    telemetry_dump_response_t resp = {0};
    resp.total_records = telemetry_storage_get_record_count();
    resp.last_seq = telemetry_storage_get_last_sequence();

    telemetry_record_t record = {0};
    uint32_t count = telemetry_storage_read_batch(start_seq, &record, 1);

    if (count > 0)
    {
      resp.current_seq = record.sequence;
      resp.record = record;
    }
    else
    {
      resp.current_seq = 0;  // End of data
    }

    /* Coverity CID 1654935/1654921: fix buffer overflow.
     * telemetry_dump_response_t is 76 bytes, larger than
     * csp_command_packet_t::payload[32]. Use packet->data
     * directly which has CSP_BUFFER_SIZE (256 bytes). */
    memcpy(packet->data, &resp, sizeof(resp));
    packet->length = (uint8_t)(sizeof(resp) + 1);
    csp_send(conn, packet);
    packet = NULL;
    break;
  }

  default:
    printf("[command_task] Unknown command id %d. Dropping packet.\n", cmd->cmd_id);
    break;
  }

  // If packet was not sent (which transfers ownership), free it.
  if (packet != NULL)
  {
    csp_buffer_free(packet);
  }
}

// Command task: listens for incoming commands on COMMAND_PORT
void vCommandTask(void *pvParameters)
{
  (void)pvParameters;

  printf("[command_task] Started listening on port %d (CSP) and UART1 (text)\n", COMMAND_PORT);
  fflush(stdout);

  // 1. Create socket and bind
  csp_socket_t sock = {0};
  csp_bind(&sock, COMMAND_PORT);

  // 2. Create backlog queue and listen
  csp_listen(&sock, 5);

  while (1)
  {
#ifdef PICO_BUILD
    // Check for text commands on UART1 (HC-12)
    uart1_listen();
#endif

    // 3. Accept a connection
    csp_conn_t *conn = csp_accept(&sock, 100);  // 100ms timeout to allow text commands
    if (conn == NULL)
    {
      continue;
    }

    wcet_task_begin(WCET_TASK_COMMAND);
    csp_packet_t *packet;
    while ((packet = csp_read(conn, 50)) != NULL)
    {
      process_command_packet(conn, packet);
    }
    wcet_task_end(WCET_TASK_COMMAND);

    csp_close(conn);
  }
}
