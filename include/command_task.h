// Command Task - Processes incoming CSP uplink packets
#ifndef COMMAND_TASK_H
#define COMMAND_TASK_H

#include <stdint.h>

#define COMMAND_PORT 20

typedef enum
{
  CMD_ECHO = 1,
  CMD_REBOOT = 2,
  CMD_SET_MODE = 3,
  CMD_PAYLOAD_CAPTURE = 4
} command_id_t;

typedef struct __attribute__((packed))
{
  uint8_t cmd_id;
  uint8_t payload[32];  // Fixed max payload size for simplicity
} csp_command_packet_t;

void vCommandTask(void *pvParameters);

#include <csp/csp.h>
void process_command_packet(csp_conn_t *conn, csp_packet_t *packet);

#endif  // COMMAND_TASK_H
