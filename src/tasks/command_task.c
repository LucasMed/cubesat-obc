#include "command_task.h"

#include "FreeRTOS.h"
#include "data_layer.h"
#include "fault_manager.h"
#include "flight_mode.h"
#include "gps_driver.h"
#include "payload_task.h"
#include "system_state.h"
#include "task.h"
#include "telemetry_storage.h"

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
  #include "hardware/uart.h"
  #include "hardware/watchdog.h"
  #include "pico/stdlib.h"
  #include "pico_pins.h"
  #include "drivers/i2c_interface.h"
#endif

#ifdef PICO_BUILD
static void process_text_command(const char *cmd)
{
  uart_puts(uart1, "[CMD] ");

  if (strncmp(cmd, "REBOOT", 6) == 0)
  {
    uart_puts(uart1, "REBOOT OK\r\n");
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

    char buf[96];
    const char *mode_names[] = {"BOOT", "SAFE", "DETUMBLE", "NOMINAL", "DIAG", "PAYLOAD"};
    const char *energy_names[] = {"LOW", "NOMINAL", "HIGH"};
    snprintf(buf, sizeof(buf), "SYSTEM: mode=%s energy=%s imu=%s temp=%s mag=%s\r\n",
             mode_names[mode], energy_names[energy], snapshot.state.imu_valid ? "OK" : "FAIL",
             snapshot.state.temp_valid ? "OK" : "FAIL", snapshot.state.mag_valid ? "OK" : "FAIL");
    uart_puts(uart1, buf);

    GpsFix_t fix = {0};
    if (gps_get_last_fix(&fix))
    {
      snprintf(buf, sizeof(buf), "GPS: v=%d lat=%.5f lon=%.5f alt=%.1f s=%d hdop=%.1f\r\n",
               fix.valid, fix.lat, fix.lon, fix.alt_m, fix.satellites, fix.hdop);
      uart_puts(uart1, buf);
    }
    printf("[command_task] Text command: STATUS\r\n");
  }
  else if (strncmp(cmd, "FAULTS", 6) == 0)
  {
    fault_level_t level = fault_get_highest_level();
    const char *level_names[] = {"OK", "WARN", "ERROR", "CRITICAL"};
    char buf[32];
    snprintf(buf, sizeof(buf), "FAULTS: %s\r\n", level_names[level > 3 ? 0 : level]);
    uart_puts(uart1, buf);
    printf("[command_task] Text command: FAULTS\r\n");
  }
  else if (strncmp(cmd, "ECHO", 4) == 0)
  {
    uart_puts(uart1, "ECHO OK\r\n");
    printf("[command_task] Text command: ECHO\r\n");
  }
  else if (strncmp(cmd, "CAPTURE", 7) == 0)
  {
    uart_puts(uart1, "CAPTURE OK\r\n");
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
    if (mode >= 1 && mode <= 3)
    {
      char buf[32];
      snprintf(buf, sizeof(buf), "MODE=%d OK\r\n", mode);
      uart_puts(uart1, buf);
      printf("[command_task] Text command: MODE=%d\r\n", mode);
      fmm_request_transition((flight_mode_t)mode);
    }
    else
    {
      uart_puts(uart1, "MODE INVALID\r\n");
    }
  }
  else if (strncmp(cmd, "HELP", 4) == 0)
  {
    uart_puts(uart1, "COMMANDS: REBOOT|STATUS|ECHO|CAPTURE|MODE=0-3|GPS|FAULTS|LOG|RESET|HELP\r\n");
  }
  else if (strncmp(cmd, "LOG", 3) == 0)
  {
    uart_puts(uart1, "LOG: dump not implemented\r\n");
  }
  else if (strncmp(cmd, "RESET", 5) == 0)
  {
    if (strncmp(cmd + 5, "GPS", 3) == 0)
    {
      // Check if it's COLD START (RESETGPS COLD)
      if (strncmp(cmd + 8, "COLD", 4) == 0)
      {
        gps_cold_start();
        uart_puts(uart1, "RESET GPS COLD START OK\r\n");
      }
      else
      {
        gps_reset_stats();
        uart_puts(uart1, "RESET GPS OK\r\n");
      }
    }
    else
    {
      uart_puts(uart1, "RESET: usage: RESETGPS | RESETGPS COLD\r\n");
    }
  }
  else if (strncmp(cmd, "GPS", 3) == 0)
  {
    GpsFix_t fix = {0};
    if (gps_get_last_fix(&fix))
    {
      char buf[96];
      snprintf(buf, sizeof(buf), "GPS: v=%d lat=%.5f lon=%.5f alt=%.1f s=%d hdop=%.1f\r\n",
               fix.valid, fix.lat, fix.lon, fix.alt_m, fix.satellites, fix.hdop);
      uart_puts(uart1, buf);

      const GpsStats_t *stats = gps_get_stats();
      snprintf(buf, sizeof(buf), "GPS STATS: rx=%u chk_err=%u inv=%u valid=%u overflow=%u\r\n",
               (unsigned)stats->sentences_received, (unsigned)stats->checksum_errors,
               (unsigned)stats->fixes_invalid, (unsigned)stats->fixes_valid,
               (unsigned)stats->buffer_overflows);
      uart_puts(uart1, buf);
    }
    else
    {
      char buf[64];
      uint8_t sats = gps_get_satellites_in_view();
      snprintf(buf, sizeof(buf), "GPS: no fix sats=%d\r\n", sats);
      uart_puts(uart1, buf);
    }
  }
  else if (strncmp(cmd, "I2CSCAN", 7) == 0)
  {
    int found = i2c_bus_scan(0x03, 0x77);
    char buf[64];
    snprintf(buf, sizeof(buf), "I2C: found %d device(s)\r\n", found);
    uart_puts(uart1, buf);
    printf("[command_task] Text command: I2CSCAN\r\n");
  }
  else
  {
    uart_puts(uart1, "UNKNOWN CMD\r\n");
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
    else if (pos < (int)(sizeof(buffer) - 1))
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

    // cppcheck-suppress bufferAccessOutOfBounds
    // Intentional: CSP packets support larger payloads; this is the telemetry
    // download protocol using sizeof(telemetry_dump_response_t) = 76 bytes
    memcpy(cmd->payload, &resp, sizeof(resp));
    packet->length = sizeof(resp) + 1;
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

    csp_packet_t *packet;
    while ((packet = csp_read(conn, 50)) != NULL)
    {
      process_command_packet(conn, packet);
    }

    csp_close(conn);
  }
}
