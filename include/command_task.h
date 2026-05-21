// Command Task - Processes incoming CSP uplink packets
#ifndef COMMAND_TASK_H
#define COMMAND_TASK_H

#include "telemetry_storage.h"

#include <stdint.h>

#define COMMAND_PORT 20

typedef enum
{
  CMD_ECHO = 1,
  CMD_REBOOT = 2,
  CMD_SET_MODE = 3,
  CMD_PAYLOAD_CAPTURE = 4,
  CMD_GPS_STATUS = 5,
  CMD_STATUS = 6,
  CMD_FAULT_LIST = 7,
  CMD_TELEMETRY_REQ = 8,
  CMD_LOG_DUMP = 9,
  CMD_SENSOR_RESET = 10,
  CMD_GPS_RESET_STATS = 11,
  CMD_TELEMETRY_DUMP = 12, // Download telemetry from flash
  CMD_DEPLOY = 13          // Initiate deploy sequence
} command_id_t;

typedef struct __attribute__((packed))
{
  uint8_t cmd_id;
  uint8_t payload[32];  // Fixed max payload size for simplicity
} csp_command_packet_t;

typedef struct __attribute__((packed))
{
  uint8_t valid;
  int32_t lat_scaled;
  int32_t lon_scaled;
  int32_t alt_scaled;
  uint8_t satellites;
  uint8_t hdop_scaled;
  uint32_t uptime_ms;
  uint16_t sentences;
  uint8_t checksum_err;
  uint8_t fixes_invalid;
} gps_status_response_t;

typedef struct __attribute__((packed))
{
  uint8_t mode;
  uint8_t energy;
  uint8_t flags;
  uint16_t heap_free;
  uint32_t uptime_sec;
  uint8_t fault_count;
  uint32_t boot_count;      /**< Number of recorded boots (0 if POST never ran)  */
  uint8_t  post_pass_count; /**< Number of POST tests that passed                */
  uint8_t  post_total_count;/**< Total number of POST tests                      */
  uint8_t  boot_reason;     /**< POST_BOOT_* code                                */
} system_status_response_t;

typedef struct __attribute__((packed))
{
  uint8_t fault_id;
  uint8_t level;
  uint8_t count;
} fault_entry_t;

/**
 * @brief Telemetry dump response
 *
 * Each packet contains up to TELEMETRY_BATCH_SIZE records (4 records × 64 bytes = 256 bytes)
 * Ground station sends next sequence number to request next batch.
 */
#define TELEMETRY_BATCH_SIZE 4

typedef struct __attribute__((packed))
{
  uint32_t total_records;     // Total records in storage
  uint32_t last_seq;          // Last sequence number stored
  uint32_t current_seq;       // Sequence number of this record
  telemetry_record_t record;  // Single telemetry record (64 bytes)
} telemetry_dump_response_t;

void vCommandTask(void *pvParameters);

#include <csp/csp.h>
void process_command_packet(csp_conn_t *conn, csp_packet_t *packet);

#endif  // COMMAND_TASK_H
