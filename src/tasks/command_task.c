#include "command_task.h"

#include "FreeRTOS.h"
#include "flight_mode.h"
#include "gps_driver.h"
#include "payload_task.h"
#include "task.h"

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
    uart_puts(uart1, "STATUS OK\r\n");
    printf("[command_task] Text command: STATUS\r\n");
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
    uart_puts(uart1, "COMMANDS: REBOOT|STATUS|ECHO|CAPTURE|MODE=0-3|GPS|HELP\r\n");
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
      uart_puts(uart1, "GPS: no fix\r\n");
    }
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
