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
static flight_mode_t mock_current_mode = FM_NOMINAL;
fmm_result_t fmm_request_transition(flight_mode_t target)
{
  last_requested_mode = target;
  return FMM_OK;
}

flight_mode_t fmm_get_mode(void)
{
  return mock_current_mode;
}

const char *fmm_mode_name(flight_mode_t mode)
{
  switch (mode)
  {
  case FM_BOOT:       return "BOOT";
  case FM_SAFE:       return "SAFE";
  case FM_DETUMBLE:   return "DETUMBLE";
  case FM_NOMINAL:    return "NOMINAL";
  case FM_DIAGNOSTIC: return "DIAGNOSTIC";
  case FM_PAYLOAD:    return "PAYLOAD";
  default:            return "UNKNOWN";
  }
}

void fmm_force_safe(void) {}

// Mock Data Layer
#include "data_layer.h"
static dl_snapshot_t mock_snapshot = {
    .state = {
        .attitude = {0.0f, 0.0f, 0.0f},
        .q = {1.0f, 0.0f, 0.0f, 0.0f},
        .rates = {0.0f, 0.0f, 0.0f},
        .temp = 25.0f,
        .battery_v = 3.7f,
        .gyro_bias = {0.0f, 0.0f, 0.0f},
        .att_uncertainty = {0.0f, 0.0f, 0.0f, 0.0f},
        .mag_field = {0.0f, 0.0f, 0.0f},
        .imu_available = true,
        .imu_valid = true,
        .temp_available = true,
        .temp_valid = true,
        .imu_ekf_valid = false,
        .mag_available = false,
        .mag_valid = false,
        .radiation_dose = 0.0f,
        .image_count = 0,
        .payload_rail_enabled = false
    },
    .mode = FM_NOMINAL,
    .energy = ENERGY_NOMINAL,
    .gps_fix = {.valid = true, .lat = -31.4321f, .lon = -64.1812f, .alt_m = 431.5f, .hdop = 1.0f, .satellites = 6, .timestamp_ms = 0},
    .seq = 1
};

void data_layer_set_gps_fix(const GpsFix_t *fix) { (void)fix; }
void data_layer_get_gps_fix(GpsFix_t *out) { (void)out; }
void data_layer_init(void) {}
void data_layer_read(dl_snapshot_t *out) { memcpy(out, &mock_snapshot, sizeof(dl_snapshot_t)); }
void data_layer_write_imu(const float att_rad[3], const float rates_rad[3]) { (void)att_rad; (void)rates_rad; }
void data_layer_write_ekf(const float q[4], const float bias_rad[3], const float cov_diag[7]) { (void)q; (void)bias_rad; (void)cov_diag; }
void data_layer_write_temp(float temp_c) { (void)temp_c; }
void data_layer_set_sensor_avail(bool imu, bool temp) { (void)imu; (void)temp; }
void data_layer_write_mag(const float field_uT[3]) { (void)field_uT; }
void data_layer_set_mag_avail(bool mag) { (void)mag; }
void data_layer_write_radiation(float dose) { (void)dose; }
void data_layer_write_payload_status(bool rail_enabled, uint16_t img_count) { (void)rail_enabled; (void)img_count; }
void data_layer_set_flight_mode(flight_mode_t mode) { (void)mode; }
void data_layer_set_energy_state(energy_state_t energy) { (void)energy; }
flight_mode_t data_layer_get_flight_mode(void) { return mock_snapshot.mode; }
energy_state_t data_layer_get_energy_state(void) { return mock_snapshot.energy; }
uint32_t data_layer_get_seq(void) { return mock_snapshot.seq; }

// Mock Fault Manager
#include "fault_manager.h"
fault_level_t fault_get_highest_level(void) { return FAULT_LEVEL_NONE; }
void fault_manager_init(void) {}
void fault_manager_tick(void) {}
bool fault_get_event(uint16_t id, fault_event_t *out) { (void)id; (void)out; return false; }
void fault_manager_set_hm_task_handle(void *h_health) { (void)h_health; }

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

void gps_reset_stats(void)
{
  memset(&mock_gps_stats, 0, sizeof(mock_gps_stats));
}

// Mock UART (for text command testing)
#include <stdbool.h>
void uart1_puts_safe(const char *str) { (void)str; }
void uart1_acquire_lock(void) {}
void uart1_release_lock(void) {}
void uart1_write_unsafe(const char *str) { (void)str; }

// Mock POST
#include "post.h"
#include <string.h>
static post_record_t mock_post_rec = {0};
void data_layer_set_post_last(const post_record_t *rec) { if (rec) memcpy(&mock_post_rec, rec, sizeof(mock_post_rec)); }
void data_layer_get_post_last(post_record_t *out) { if (out) memcpy(out, &mock_post_rec, sizeof(post_record_t)); }
void data_layer_set_mode_entry_tick(uint32_t tick) { (void)tick; }
uint32_t data_layer_get_mode_entry_tick(void) { return 0; }

const char *post_boot_reason_name(uint32_t reason)
{
  (void)reason;
  return "POWER_ON";
}

bool post_is_critical_fail(const post_record_t *record)
{
  (void)record;
  return false;
}

// Forward declaration of the text command runner (defined in command_task_test.c)
void test_run_text_command(const char *cmd);

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

static bool last_deploy_flag = false;
void data_layer_set_deploy_in_progress(bool val) { last_deploy_flag = val; }
bool data_layer_get_deploy_in_progress(void) { return last_deploy_flag; }

void reset_mocks()
{
  last_freed = 0;
  last_csp_sent = NULL;
  last_delay = 0;
  last_requested_mode = FM_BOOT;
  last_notified_value = 0;
  mock_current_mode = FM_NOMINAL;
  last_deploy_flag = false;
  memset(&mock_post_rec, 0, sizeof(mock_post_rec));
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

/* ---- CSP CMD_DEPLOY ---- */

void test_command_csp_deploy()
{
  reset_mocks();
  mock_current_mode = FM_BOOT;

  csp_conn_t *mock_conn = NULL;
  csp_packet_t *pkt = csp_buffer_get(0);

  csp_command_packet_t *cmd = (csp_command_packet_t *)pkt->data;
  cmd->cmd_id = CMD_DEPLOY;
  pkt->length = 1;

  process_command_packet(mock_conn, pkt);

  assert(last_requested_mode == FM_DETUMBLE);
  assert(last_deploy_flag == true);
  printf("test_command_csp_deploy PASS\n");
}

/* ---- Text DEPLOY from BOOT ---- */

void test_text_deploy_from_boot()
{
  reset_mocks();
  mock_current_mode = FM_BOOT;

  test_run_text_command("DEPLOY");

  assert(last_requested_mode == FM_DETUMBLE);
  assert(last_deploy_flag == true);
  printf("test_text_deploy_from_boot PASS\n");
}

/* ---- Text DEPLOY from NOMINAL (rejected) ---- */

void test_text_deploy_from_nominal()
{
  reset_mocks();
  mock_current_mode = FM_NOMINAL;

  test_run_text_command("DEPLOY");

  /* Should NOT request transition or set flag from NOMINAL */
  assert(last_requested_mode == FM_BOOT); /* unchanged from reset */
  assert(last_deploy_flag == false);
  printf("test_text_deploy_from_nominal PASS\n");
}

/* ---- Text DEPLOYCLEAR ---- */

void test_text_deploy_clear()
{
  reset_mocks();
  last_deploy_flag = true;

  test_run_text_command("DEPLOYCLEAR");

  assert(last_deploy_flag == false);
  assert(last_requested_mode == FM_BOOT); /* mode unchanged */
  printf("test_text_deploy_clear PASS\n");
}

/* ---- Text MODE name parsing ---- */

void test_text_mode_name()
{
  reset_mocks();
  mock_current_mode = FM_BOOT;

  test_run_text_command("MODE=DETUMBLE");
  assert(last_requested_mode == FM_DETUMBLE);

  reset_mocks();
  mock_current_mode = FM_SAFE;
  test_run_text_command("MODE=nominal");
  assert(last_requested_mode == FM_NOMINAL);

  printf("test_text_mode_name PASS\n");
}

/* ---- Text MODE range 0-5 ---- */

void test_text_mode_range()
{
  reset_mocks();
  mock_current_mode = FM_BOOT;

  test_run_text_command("MODE=4");
  assert(last_requested_mode == FM_DIAGNOSTIC);

  reset_mocks();
  mock_current_mode = FM_BOOT;
  test_run_text_command("MODE=5");
  assert(last_requested_mode == FM_PAYLOAD);

  printf("test_text_mode_range PASS\n");
}

/* ---- CSP CMD_STATUS with POST data ---- */

void test_command_status_post()
{
  reset_mocks();

  /* Set up a mock POST record */
  post_record_t post_rec;
  memset(&post_rec, 0, sizeof(post_rec));
  post_rec.magic = POST_MAGIC;
  post_rec.boot_count = 42;
  post_rec.boot_reason = POST_BOOT_WATCHDOG;
  post_rec.test_bitmap = (1u << 10) - 1u; /* all 10 tests pass */
  data_layer_set_post_last(&post_rec);

  csp_conn_t *mock_conn = NULL;
  csp_packet_t *pkt = csp_buffer_get(0);

  csp_command_packet_t *cmd = (csp_command_packet_t *)pkt->data;
  cmd->cmd_id = CMD_STATUS;
  pkt->length = 1;

  process_command_packet(mock_conn, pkt);

  /* Verify response contains POST fields */
  system_status_response_t *resp = (system_status_response_t *)pkt->data;
  assert(resp->boot_count == 42);
  assert(resp->post_pass_count == 10);
  assert(resp->post_total_count == 10);
  assert(resp->boot_reason == POST_BOOT_WATCHDOG);

  /* last_csp_sent was freed by mock_csp_send */
  printf("test_command_status_post PASS\n");
}

/* ---- Text DEPLOY from SAFE ---- */

void test_text_deploy_from_safe()
{
  reset_mocks();
  mock_current_mode = FM_SAFE;

  test_run_text_command("DEPLOY");

  assert(last_requested_mode == FM_DETUMBLE);
  assert(last_deploy_flag == true);
  printf("test_text_deploy_from_safe PASS\n");
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
  test_command_csp_deploy();
  test_text_deploy_from_boot();
  test_text_deploy_from_nominal();
  test_text_deploy_clear();
  test_text_mode_name();
  test_text_mode_range();
  test_command_status_post();
  test_text_deploy_from_safe();
  printf("All tests passed!\n");
  return 0;
}
