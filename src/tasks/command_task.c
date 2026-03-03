#include "command_task.h"

#include "FreeRTOS.h"
#include "task.h"

#include <csp/csp.h>
#include <stdio.h>
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
  #include "hardware/watchdog.h"
  #include "pico/stdlib.h"
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
    printf("[command_task] Executing SET_MODE (mode=%d). Not fully implemented yet.\n",
           cmd->payload[0]);
    break;

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

  printf("[command_task] Started listening on port %d\n", COMMAND_PORT);
  fflush(stdout);

  // 1. Create socket and bind
  csp_socket_t sock = {0};
  csp_bind(&sock, COMMAND_PORT);

  // 2. Create backlog queue and listen
  csp_listen(&sock, 5);

  while (1)
  {
    // 3. Accept a connection
    csp_conn_t *conn = csp_accept(&sock, CSP_MAX_TIMEOUT);
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
