#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Mock FreeRTOS
#include "FreeRTOS.h"
#include "task.h"
#include "payload_task.h"

uint32_t mock_tick_count = 1000;
uint32_t mock_xTaskGetTickCount(void)
{
  return mock_tick_count;
}
void mock_vTaskDelayUntil(uint32_t *pxPreviousWakeTime, uint32_t xTimeIncrement)
{
  *pxPreviousWakeTime += xTimeIncrement;
  mock_tick_count += xTimeIncrement;
}

#undef xTaskGetTickCount
#undef vTaskDelayUntil
#define xTaskGetTickCount mock_xTaskGetTickCount
#define vTaskDelayUntil mock_vTaskDelayUntil

// Mock vTaskDelay used in command_task
uint32_t last_delay = 0;
#undef vTaskDelay
void vTaskDelay(uint32_t ticks)
{
  last_delay = ticks;
  mock_tick_count += ticks;
}

// Mock FMM
#include "flight_mode.h"
static flight_mode_t last_requested_mode = FM_BOOT;
fmm_result_t fmm_request_transition(flight_mode_t target)
{
  last_requested_mode = target;
  return FMM_OK;
}

// Mock GPS
#include "gps_driver.h"
static GpsFix_t mock_gps_fix = {
    .lat = -31.43210f,
    .lon = -64.18123f,
    .alt_m = 431.5f,
    .hdop = 1.2f,
    .satellites = 6,
    .valid = true,
    .timestamp_ms = 45000
};
static GpsStats_t mock_gps_stats = {
    .sentences_received = 1234,
    .checksum_errors = 0,
    .parse_errors = 0,
    .fixes_valid = 45,
    .fixes_invalid = 2,
    .buffer_overflows = 0
};

bool gps_get_last_fix(GpsFix_t *out)
{
  if (out) {
    memcpy(out, &mock_gps_fix, sizeof(GpsFix_t));
  }
  return mock_gps_fix.valid;
}

const GpsStats_t *gps_get_stats(void)
{
  return &mock_gps_stats;
}

// Mock xTaskGetHandle
#undef xTaskGetHandle
TaskHandle_t xTaskGetHandle(const char *pcName)
{
  (void)pcName;
  return (TaskHandle_t)0x1234; /* dummy handle */
}

// Mock xTaskNotify
static uint32_t last_notified_value = 0;
BaseType_t xTaskNotify_Stub(TaskHandle_t xTask, uint32_t ulValue, eNotifyAction eAction)
{
  (void)xTask; (void)eAction;
  last_notified_value = ulValue;
  return pdPASS;
}
#undef xTaskNotify
#define xTaskNotify xTaskNotify_Stub

// Actual file to test
#include "command_task.h"

// Mock libcsp

static int last_freed = 0;
void mock_csp_buffer_free(void *packet)
{
  last_freed++;
  free(packet);
}

csp_packet_t *mock_csp_buffer_get(size_t size)
{
  csp_packet_t *packet = calloc(1, sizeof(csp_packet_t) + 256);
  packet->length = 0;
  return packet;
}
#define csp_buffer_get mock_csp_buffer_get

static csp_packet_t *last_csp_sent = NULL;
void mock_csp_send(csp_conn_t *conn, csp_packet_t *packet)
{
  last_csp_sent = packet;
  // in real life csp_send takes ownership, so we free it to simulate that taking ownership
  free(packet);
}

int mock_csp_conn_src(const csp_conn_t *conn)
{
  return 1;  // Simulated GN Address
}

void reset_mocks()
{
  last_freed = 0;
  last_csp_sent = NULL;
  last_delay = 0;
  last_requested_mode = FM_BOOT;
  last_notified_value = 0;
}

void test_command_echo()
{
  reset_mocks();
  csp_conn_t *mock_conn = NULL;
  csp_packet_t *pkt = csp_buffer_get(0);

  csp_command_packet_t *cmd = (csp_command_packet_t *)pkt->data;
  cmd->cmd_id = CMD_ECHO;
  strcpy((char *)cmd->payload, "HELLO");
  pkt->length = 1 + strlen("HELLO");

  process_command_packet(mock_conn, pkt);

  // Memory ownership passed to csp_send, which freed it.
  assert(last_csp_sent != NULL);
  assert(last_freed == 0);  // No explicit free, csp_send took ownership
  printf("test_command_echo PASS\n");
}

void test_command_reboot()
{
  reset_mocks();
  csp_conn_t *mock_conn = NULL;
  csp_packet_t *pkt = csp_buffer_get(0);

  csp_command_packet_t *cmd = (csp_command_packet_t *)pkt->data;
  cmd->cmd_id = CMD_REBOOT;
  pkt->length = 1;

  process_command_packet(mock_conn, pkt);

  assert(last_csp_sent == NULL);
  assert(last_freed == 1);                   // Freeed explicitly
  assert(last_delay == pdMS_TO_TICKS(100));  // Delay for logs to flush
  printf("test_command_reboot PASS\n");
}

void test_command_invalid()
{
  reset_mocks();
  csp_conn_t *mock_conn = NULL;
  csp_packet_t *pkt = csp_buffer_get(0);

  csp_command_packet_t *cmd = (csp_command_packet_t *)pkt->data;
  cmd->cmd_id = 99;  // unknown
  pkt->length = 1;

  process_command_packet(mock_conn, pkt);

  assert(last_csp_sent == NULL);
  assert(last_freed == 1);  // Dropped and freed
  printf("test_command_invalid PASS\n");
}

void test_command_short_packet()
{
  reset_mocks();
  csp_conn_t *mock_conn = NULL;
  csp_packet_t *pkt = csp_buffer_get(0);

  pkt->length = 0;  // too short

  process_command_packet(mock_conn, pkt);

  assert(last_csp_sent == NULL);
  assert(last_freed == 1);  // Dropped and freed
  printf("test_command_short_packet PASS\n");
}

void test_command_set_mode()
{
  reset_mocks();
  csp_conn_t *mock_conn = NULL;
  csp_packet_t *pkt = csp_buffer_get(0);

  csp_command_packet_t *cmd = (csp_command_packet_t *)pkt->data;
  cmd->cmd_id = CMD_SET_MODE;
  cmd->payload[0] = FM_PAYLOAD;
  pkt->length = 1 + 1;

  process_command_packet(mock_conn, pkt);

  assert(last_requested_mode == FM_PAYLOAD);
  assert(last_freed == 1);
  printf("test_command_set_mode PASS\n");
}

void test_command_payload_capture()
{
  reset_mocks();
  csp_conn_t *mock_conn = NULL;
  csp_packet_t *pkt = csp_buffer_get(0);

  csp_command_packet_t *cmd = (csp_command_packet_t *)pkt->data;
  cmd->cmd_id = CMD_PAYLOAD_CAPTURE;
  pkt->length = 1;

  process_command_packet(mock_conn, pkt);

  assert(last_notified_value == PAYLOAD_NOTIFY_CAPTURE_IMAGE);
  assert(last_freed == 1);
  printf("test_command_payload_capture PASS\n");
}

int main()
{
  printf("Running Command Task tests...\n");
  test_command_echo();
  test_command_reboot();
  test_command_invalid();
  test_command_short_packet();
  test_command_set_mode();
  test_command_payload_capture();
  printf("All tests passed!\n");
  return 0;
}
