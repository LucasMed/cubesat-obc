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
#include <string.h>
#include <stdio.h>

/* UART output capture buffer for approval tests */
#define UART_OUTPUT_BUF_SIZE 4096
static char s_uart_output[UART_OUTPUT_BUF_SIZE];
static size_t s_uart_output_len = 0;

void uart1_puts_safe(const char *str)
{
  size_t len = strlen(str);
  if (s_uart_output_len + len < UART_OUTPUT_BUF_SIZE)
  {
    memcpy(s_uart_output + s_uart_output_len, str, len);
    s_uart_output_len += len;
  }
}

const char *test_get_uart_output(void)
{
  s_uart_output[s_uart_output_len] = '\0';
  return s_uart_output;
}

void test_clear_uart_output(void)
{
  s_uart_output_len = 0;
  s_uart_output[0] = '\0';
}

void uart1_acquire_lock(void) {}
void uart1_release_lock(void) {}

void uart1_write_unsafe(const char *str)
{
  /* Also capture to the output buffer for STATUS (which uses write_unsafe) */
  size_t len = strlen(str);
  if (s_uart_output_len + len < UART_OUTPUT_BUF_SIZE)
  {
    memcpy(s_uart_output + s_uart_output_len, str, len);
    s_uart_output_len += len;
  }
}

/* PICO hardware stubs for text command testing */
#include "pico_stubs.h"

/* Type declarations for stubs below.  These headers are also included
   via command_task.c (common section) when PICO_BUILD is active, but
   test_command.c is a separate translation unit and needs them directly. */
#include "ina219.h"
#include "mag_calib.h"
#include "drivers/imu/imu_calib.h"

/* watchdog_reboot — provided by include/host/hardware/watchdog.h but we
   need a strong symbol for the linker. */
void watchdog_reboot(uint32_t pc, uint32_t sp, uint32_t delay_ms)
{
  (void)pc; (void)sp; (void)delay_ms;
}

/* uart1 extern — satisfied here since pico_stubs.h declares the type */
uart_inst_t *const uart1 = NULL;

/* I2C bus stubs */
int i2c_bus_scan(uint8_t start_addr, uint8_t end_addr)
{
  (void)start_addr; (void)end_addr;
  return 0; /* no devices found */
}

int i2c_bus_write_read(uint8_t addr, const uint8_t *tx, size_t tx_len,
                       uint8_t *rx, size_t rx_len)
{
  (void)addr; (void)tx; (void)tx_len;
  memset(rx, 0, rx_len);
  return 0;
}

/* INA219 power monitor stubs */
bool ina219_read_power(ina219_data_t *data)
{
  if (data)
  {
    data->bus_voltage_mv = 3300;
    data->shunt_voltage_uv = 5000;
    data->current_ua = 100000;
    data->power_uw = 330000;
  }
  return true;
}

bool ina219_solar_read_power(ina219_data_t *data)
{
  if (data)
  {
    data->bus_voltage_mv = 4200;
    data->shunt_voltage_uv = 10000;
    data->current_ua = 50000;
    data->power_uw = 210000;
  }
  return true;
}

/* Mag calibration stubs */
static mag_calib_t s_mag_cal = {{0.5f, 0.3f, -0.2f}, {1.0f, 1.0f, 1.0f}, true};

void mag_calib_start(void) { s_mag_cal.calibrated = false; }
void mag_calib_finish(void) { s_mag_cal.calibrated = true; }
bool mag_calib_is_valid(void) { return s_mag_cal.calibrated; }
void mag_calib_get(mag_calib_t *out) { if (out) memcpy(out, &s_mag_cal, sizeof(s_mag_cal)); }

/* IMU calibration stubs */
static imu_calib_t s_imu_cal = {
  .accel_offset = {0.01f, 0.02f, -0.01f},
  .accel_scale = {1.0f, 1.0f, 1.0f},
  .gyro_offset_raw = {0, 0, 0},
  .gyro_bias_rads = {0.001f, -0.002f, 0.0005f},
  .calibrated = true
};

void imu_calib_start(void) { s_imu_cal.calibrated = false; }
void imu_calib_finish(void) { s_imu_cal.calibrated = true; }
bool imu_calib_is_valid(void) { return s_imu_cal.calibrated; }
void imu_calib_get(imu_calib_t *out) { if (out) memcpy(out, &s_imu_cal, sizeof(s_imu_cal)); }
void imu_calib_save_to_flash(void) {}
void imu_calib_load_from_flash(void) {}

/* GPS cold start stub */
void gps_cold_start(void) {}

/* GPS satellites-in-view stub */
uint8_t gps_get_satellites_in_view(void) { return 8; }

/* BH1750 — the #include "bh1750.h" in the PICO section provides the
   BH1750_ADDR_DEFAULT and BH1750_CMD_OT_H_RES2 macros.  The header is
   already included in common section so just ensure the types are seen. */

// Mock DS3231 RTC (for SETTIME tests)
static bool s_ds3231_set_time_ret = true;
static bool s_ds3231_set_time_called = false;
static uint16_t s_last_set_year = 0;
static uint8_t s_last_set_month = 0;
static uint8_t s_last_set_day = 0;

#include <stdbool.h>
#include "ds3231.h"
bool ds3231_set_time(uint16_t year, uint8_t month, uint8_t day,
                     uint8_t hour, uint8_t minute, uint8_t second)
{
    (void)hour; (void)minute; (void)second;
    s_ds3231_set_time_called = true;
    s_last_set_year = year;
    s_last_set_month = month;
    s_last_set_day = day;
    return s_ds3231_set_time_ret;
}

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
/* Saved response data — copied before free() so tests can read the response
   after process_command_packet() returns (avoids use-after-free when the
   handler calls csp_send which frees the packet). */
#define LAST_RESPONSE_MAX 256
static uint8_t last_response_data[LAST_RESPONSE_MAX];
static size_t last_response_len = 0;
void mock_csp_send(csp_conn_t *conn, csp_packet_t *packet)
{
  last_csp_sent = packet;
  size_t copy_len = packet->length;
  if (copy_len > LAST_RESPONSE_MAX) copy_len = LAST_RESPONSE_MAX;
  memcpy(last_response_data, packet->data, copy_len);
  last_response_len = copy_len;
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
  s_ds3231_set_time_ret = true;
  s_ds3231_set_time_called = false;
  s_last_set_year = 0;
  s_last_set_month = 0;
  s_last_set_day = 0;
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

/* ---- Additional CSP mock functions for vCommandTask socket ops ---- */
/* These allow the full CSP stack to be mocked when activated via CSP_MOCK.
   vCommandTask() references these, even though the test only exercises
   process_command_packet() directly. */

/* Simple stubs — tests don't call vCommandTask() so these just need to exist */
int mock_csp_bind(csp_socket_t *sock, uint16_t port) { (void)sock; (void)port; return 0; }
int mock_csp_listen(csp_socket_t *sock, size_t backlog) { (void)sock; (void)backlog; return 0; }
csp_conn_t *mock_csp_accept(csp_socket_t *sock, uint32_t timeout) { (void)sock; (void)timeout; return NULL; }
csp_packet_t *mock_csp_read(csp_conn_t *conn, uint32_t timeout) { (void)conn; (void)timeout; return NULL; }
void mock_csp_close(csp_conn_t *conn) { (void)conn; }

/* ---- CSP CMD_GPS_STATUS ---- */

void test_command_gps_status()
{
  reset_mocks();
  csp_conn_t *mock_conn = NULL;
  csp_packet_t *pkt = csp_buffer_get(0);

  csp_command_packet_t *cmd = (csp_command_packet_t *)pkt->data;
  cmd->cmd_id = CMD_GPS_STATUS;
  pkt->length = 1;

  process_command_packet(mock_conn, pkt);

  /* Should have sent a response */
  assert(last_csp_sent != NULL);
  printf("test_command_gps_status PASS\n");
}

/* ---- CSP CMD_FAULT_LIST ---- */

void test_command_fault_list()
{
  reset_mocks();
  csp_conn_t *mock_conn = NULL;
  csp_packet_t *pkt = csp_buffer_get(0);

  csp_command_packet_t *cmd = (csp_command_packet_t *)pkt->data;
  cmd->cmd_id = CMD_FAULT_LIST;
  pkt->length = 1;

  process_command_packet(mock_conn, pkt);

  /* Should have sent a response (even with no faults) */
  assert(last_csp_sent != NULL);
  printf("test_command_fault_list PASS\n");
}

/* ---- CSP CMD_SENSOR_RESET ---- */

void test_command_sensor_reset()
{
  reset_mocks();
  csp_conn_t *mock_conn = NULL;
  csp_packet_t *pkt = csp_buffer_get(0);

  csp_command_packet_t *cmd = (csp_command_packet_t *)pkt->data;
  cmd->cmd_id = CMD_SENSOR_RESET;
  cmd->payload[0] = 1; /* valid sensor ID */
  pkt->length = 2;

  process_command_packet(mock_conn, pkt);

  assert(last_csp_sent != NULL);
  printf("test_command_sensor_reset PASS\n");
}

/* ---- CSP CMD_GPS_RESET_STATS ---- */

void test_command_gps_reset_stats()
{
  reset_mocks();
  csp_conn_t *mock_conn = NULL;
  csp_packet_t *pkt = csp_buffer_get(0);

  csp_command_packet_t *cmd = (csp_command_packet_t *)pkt->data;
  cmd->cmd_id = CMD_GPS_RESET_STATS;
  pkt->length = 1;

  process_command_packet(mock_conn, pkt);

  assert(last_csp_sent != NULL);
  printf("test_command_gps_reset_stats PASS\n");
}

/* ---- CSP CMD_TELEMETRY_REQ ---- */

void test_command_telemetry_req()
{
  reset_mocks();
  csp_conn_t *mock_conn = NULL;
  csp_packet_t *pkt = csp_buffer_get(0);

  csp_command_packet_t *cmd = (csp_command_packet_t *)pkt->data;
  cmd->cmd_id = CMD_TELEMETRY_REQ;
  pkt->length = 1;

  process_command_packet(mock_conn, pkt);

  /* Should send a response (csp_send called) */
  assert(last_csp_sent != NULL);
  printf("test_command_telemetry_req PASS\n");
}

/* ---- CSP CMD_LOG_DUMP ---- */

void test_command_log_dump()
{
  reset_mocks();
  csp_conn_t *mock_conn = NULL;
  csp_packet_t *pkt = csp_buffer_get(0);

  csp_command_packet_t *cmd = (csp_command_packet_t *)pkt->data;
  cmd->cmd_id = CMD_LOG_DUMP;
  cmd->payload[0] = 8;
  pkt->length = 2;

  process_command_packet(mock_conn, pkt);

  assert(last_csp_sent != NULL);
  printf("test_command_log_dump PASS\n");
}

/* CSP send returns ownership: null packet after send (packet != NULL fallback) */

void test_command_packet_not_freed_after_default()
{
  reset_mocks();
  csp_conn_t *mock_conn = NULL;
  csp_packet_t *pkt = csp_buffer_get(0);

  csp_command_packet_t *cmd = (csp_command_packet_t *)pkt->data;
  cmd->cmd_id = 99; /* unknown — no csp_send, falls to free at end */
  pkt->length = 1;

  process_command_packet(mock_conn, pkt);

  /* Default case: packet not sent, should be freed by csp_buffer_free */
  assert(last_csp_sent == NULL);
  assert(last_freed == 1);
  printf("test_command_packet_not_freed_after_default PASS\n");
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

  /* Response is written to cmd->payload (packet->data + 1), so read from there.
     mock_csp_send saves a copy in last_response_data before freeing. */
  csp_command_packet_t *resp_pkt = (csp_command_packet_t *)last_response_data;
  system_status_response_t *resp = (system_status_response_t *)resp_pkt->payload;
  assert(resp->boot_count == 42);
  assert(resp->post_pass_count == 10);
  assert(resp->post_total_count == 10);
  assert(resp->boot_reason == POST_BOOT_WATCHDOG);
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

/* ---- Regression: atoi("abc") must NOT be treated as FM_BOOT ---- */

void test_text_mode_atoi_non_numeric_rejected(void)
{
  reset_mocks();
  last_requested_mode = FM_DIAGNOSTIC; /* sentinel — non-default */

  test_run_text_command("MODE=abc"); /* non-numeric, should reject */

  /* If atoi("abc") returned 0 (FM_BOOT), last_requested_mode would
     be FM_BOOT. After strtol fix, it stays at the sentinel. */
  assert(last_requested_mode == FM_DIAGNOSTIC);
  printf("test_text_mode_atoi_non_numeric_rejected PASS\n");
}

void test_text_mode_numeric_zero_parsed_correctly(void)
{
  reset_mocks();
  last_requested_mode = FM_DIAGNOSTIC; /* sentinel */

  test_run_text_command("MODE=0"); /* FM_BOOT = 0, must parse correctly */

  /* Valid numeric "0" must transition to FM_BOOT, not be rejected */
  assert(last_requested_mode == FM_BOOT);
  printf("test_text_mode_numeric_zero_parsed_correctly PASS\n");
}

/* ---- SETTIME range validation tests ---- */

void test_settime_valid(void)
{
  reset_mocks();

  test_run_text_command("SETTIME 2024 06 15 12 30 45");

  assert(s_ds3231_set_time_called == true);
  assert(s_last_set_year == 2024);
  assert(s_last_set_month == 6);
  assert(s_last_set_day == 15);
  printf("test_settime_valid PASS\n");
}

void test_settime_year_out_of_range(void)
{
  reset_mocks();

  test_run_text_command("SETTIME 1999 06 15 12 30 45");
  assert(s_ds3231_set_time_called == false);

  reset_mocks();
  test_run_text_command("SETTIME 2101 06 15 12 30 45");
  assert(s_ds3231_set_time_called == false);

  printf("test_settime_year_out_of_range PASS\n");
}

void test_settime_month_out_of_range(void)
{
  reset_mocks();

  test_run_text_command("SETTIME 2024 00 15 12 30 45");
  assert(s_ds3231_set_time_called == false);

  reset_mocks();
  test_run_text_command("SETTIME 2024 13 15 12 30 45");
  assert(s_ds3231_set_time_called == false);

  printf("test_settime_month_out_of_range PASS\n");
}

void test_settime_day_out_of_range(void)
{
  reset_mocks();

  test_run_text_command("SETTIME 2024 06 00 12 30 45");
  assert(s_ds3231_set_time_called == false);

  reset_mocks();
  test_run_text_command("SETTIME 2024 06 32 12 30 45");
  assert(s_ds3231_set_time_called == false);

  printf("test_settime_day_out_of_range PASS\n");
}

void test_settime_hour_out_of_range(void)
{
  reset_mocks();

  test_run_text_command("SETTIME 2024 06 15 24 30 45");
  assert(s_ds3231_set_time_called == false);

  reset_mocks();
  test_run_text_command("SETTIME 2024 06 15 255 30 45");
  assert(s_ds3231_set_time_called == false);

  printf("test_settime_hour_out_of_range PASS\n");
}

void test_settime_minute_out_of_range(void)
{
  reset_mocks();

  test_run_text_command("SETTIME 2024 06 15 12 60 45");
  assert(s_ds3231_set_time_called == false);

  printf("test_settime_minute_out_of_range PASS\n");
}

void test_settime_second_out_of_range(void)
{
  reset_mocks();

  test_run_text_command("SETTIME 2024 06 15 12 30 60");
  assert(s_ds3231_set_time_called == false);

  printf("test_settime_second_out_of_range PASS\n");
}

void test_settime_too_few_fields(void)
{
  reset_mocks();

  test_run_text_command("SETTIME 2024 06 15");
  assert(s_ds3231_set_time_called == false);

  printf("test_settime_too_few_fields PASS\n");
}

/* ================================================================ *
 *  TEXT COMMAND REGRESSION TESTS — comprehensive coverage of ALL    *
 *  process_text_command() branches.  These are APPROVAL TESTS that  *
 *  capture current behavior BEFORE the if-else → table refactor.    *
 * ================================================================ */

/* Helper: run a text command and return the UART output string */
static const char *run_cmd(const char *cmd)
{
  test_clear_uart_output();
  reset_mocks();
  test_run_text_command(cmd);
  return test_get_uart_output();
}

/* Standard/system commands */
void test_text_echo(void)
{
  const char *out = run_cmd("ECHO");
  assert(strstr(out, "ECHO OK") != NULL);
  printf("test_text_echo PASS\n");
}

void test_text_reboot(void)
{
  test_clear_uart_output();
  reset_mocks();

  /* REBOOT calls watchdog_reboot + vTaskDelay.  The uart output should
     contain the REBOOT OK message BEFORE the reboot call. */
  test_run_text_command("REBOOT");
  const char *out = test_get_uart_output();
  assert(strstr(out, "REBOOT OK") != NULL);
  assert(last_delay == pdMS_TO_TICKS(100));
  printf("test_text_reboot PASS\n");
}

void test_text_status(void)
{
  test_clear_uart_output();
  reset_mocks();

  /* Set up POST record so STATUS includes POST summary */
  post_record_t post_rec;
  memset(&post_rec, 0, sizeof(post_rec));
  post_rec.magic = POST_MAGIC;
  post_rec.boot_count = 42;
  post_rec.boot_reason = POST_BOOT_POWER_ON;
  post_rec.test_bitmap = (1u << 10) - 1u;
  data_layer_set_post_last(&post_rec);

  test_run_text_command("STATUS");
  const char *out = test_get_uart_output();
  assert(strstr(out, "SYSTEM:") != NULL);
  assert(strstr(out, "GPS:") != NULL);
  assert(strstr(out, "POST:") != NULL);
  assert(strstr(out, "boot=42") != NULL);
  printf("test_text_status PASS\n");
}

void test_text_faults(void)
{
  const char *out = run_cmd("FAULTS");
  assert(strstr(out, "FAULTS:") != NULL);
  assert(strstr(out, "OK") != NULL);
  printf("test_text_faults PASS\n");
}

void test_text_capture(void)
{
  test_clear_uart_output();
  reset_mocks();

  test_run_text_command("CAPTURE");
  const char *out = test_get_uart_output();
  assert(strstr(out, "CAPTURE OK") != NULL);
  /* Should have notified payload task */
  assert(last_notified_value == PAYLOAD_NOTIFY_CAPTURE_IMAGE);
  printf("test_text_capture PASS\n");
}

void test_text_imgdump(void)
{
  test_clear_uart_output();
  reset_mocks();

  test_run_text_command("IMGDUMP");
  const char *out = test_get_uart_output();
  assert(strstr(out, "IMGDUMP OK") != NULL);
  assert(last_notified_value == PAYLOAD_NOTIFY_DUMP_IMAGE);
  printf("test_text_imgdump PASS\n");
}

/* MODE command variants */
void test_text_mode_equals_name(void)
{
  const char *out = run_cmd("MODE=DETUMBLE");
  assert(last_requested_mode == FM_DETUMBLE);
  assert(strstr(out, "MODE=2 OK") != NULL);
  printf("test_text_mode_equals_name PASS\n");
}

void test_text_mode_space_name(void)
{
  const char *out = run_cmd("MODE NOMINAL");
  assert(last_requested_mode == FM_NOMINAL);
  assert(strstr(out, "MODE=3 OK") != NULL);
  printf("test_text_mode_space_name PASS\n");
}

void test_text_mode_equals_number(void)
{
  const char *out = run_cmd("MODE=5");
  assert(last_requested_mode == FM_PAYLOAD);
  assert(strstr(out, "MODE=5 OK") != NULL);
  printf("test_text_mode_equals_number PASS\n");
}

void test_text_mode_space_number(void)
{
  const char *out = run_cmd("MODE 4");
  assert(last_requested_mode == FM_DIAGNOSTIC);
  assert(strstr(out, "MODE=4 OK") != NULL);
  printf("test_text_mode_space_number PASS\n");
}

void test_text_mode_invalid_name(void)
{
  const char *out = run_cmd("MODE=hyperdrive");
  assert(last_requested_mode == FM_BOOT); /* unchanged from reset */
  assert(strstr(out, "unknown mode name") != NULL);
  printf("test_text_mode_invalid_name PASS\n");
}

void test_text_mode_invalid_number(void)
{
  const char *out = run_cmd("MODE=99");
  assert(last_requested_mode == FM_BOOT);
  assert(strstr(out, "MODE INVALID") != NULL);
  printf("test_text_mode_invalid_number PASS\n");
}

void test_text_mode_non_numeric(void)
{
  reset_mocks();
  last_requested_mode = FM_DIAGNOSTIC; /* sentinel */
  test_run_text_command("MODE=abc");
  /* After the strtol fix, non-numeric stays at sentinel, not FM_BOOT */
  assert(last_requested_mode == FM_DIAGNOSTIC);
  printf("test_text_mode_non_numeric PASS\n");
}

void test_text_mode_parse_zero(void)
{
  reset_mocks();
  last_requested_mode = FM_DIAGNOSTIC; /* sentinel */
  test_run_text_command("MODE=0");
  assert(last_requested_mode == FM_BOOT); /* MODE=0 is valid numeric 0 */
  printf("test_text_mode_parse_zero PASS\n");
}

/* GPS commands */
void test_text_gps_with_fix(void)
{
  test_clear_uart_output();
  reset_mocks();

  /* Default mock has a valid fix */
  test_run_text_command("GPS");
  const char *out = test_get_uart_output();
  assert(strstr(out, "GPS:") != NULL);
  assert(strstr(out, "lat=") != NULL);
  assert(strstr(out, "GPS STATS:") != NULL);
  printf("test_text_gps_with_fix PASS\n");
}

void test_text_gps_no_fix(void)
{
  test_clear_uart_output();
  reset_mocks();

  /* Force GPS fix to be invalid (mock is static, accessible in this file) */
  mock_gps_fix.valid = false;

  test_run_text_command("GPS");
  const char *out = test_get_uart_output();
  assert(strstr(out, "no fix") != NULL);
  assert(strstr(out, "sats=") != NULL);

  /* Restore for other tests */
  mock_gps_fix.valid = true;
  printf("test_text_gps_no_fix PASS\n");
}

void test_text_gpsstats(void)
{
  const char *out = run_cmd("GPSSTATS");
  assert(strstr(out, "GPS stats:") != NULL);
  assert(strstr(out, "rx=") != NULL);
  assert(strstr(out, "last fix:") != NULL);
  printf("test_text_gpsstats PASS\n");
}

void test_text_resetgps(void)
{
  const char *out = run_cmd("RESETGPS");
  assert(strstr(out, "RESET GPS OK") != NULL);
  printf("test_text_resetgps PASS\n");
}

void test_text_resetgps_cold(void)
{
  const char *out = run_cmd("RESETGPS COLD");
  assert(strstr(out, "RESET GPS COLD START OK") != NULL);
  printf("test_text_resetgps_cold PASS\n");
}

void test_text_reset_usage(void)
{
  const char *out = run_cmd("RESET");
  assert(strstr(out, "RESET: usage:") != NULL);
  printf("test_text_reset_usage PASS\n");
}

/* Calibration commands */
void test_text_mag_cal_start(void)
{
  const char *out = run_cmd("MAG-CAL-START");
  assert(strstr(out, "MAG-CAL: started") != NULL);
  assert(mag_calib_is_valid() == false); /* start invalidates cal */
  printf("test_text_mag_cal_start PASS\n");
}

void test_text_mag_cal_stop(void)
{
  /* First start (invalidates), then finish */
  test_clear_uart_output();
  reset_mocks();
  mag_calib_start(); /* invalidate */
  test_run_text_command("MAG-CAL-STOP");
  const char *out = test_get_uart_output();
  assert(strstr(out, "MAG-CAL: finished") != NULL);
  assert(mag_calib_is_valid() == true);
  printf("test_text_mag_cal_stop PASS\n");
}

void test_text_mag_cal_status_valid(void)
{
  test_clear_uart_output();
  reset_mocks();
  /* Ensure cal is valid */
  mag_calib_finish();

  test_run_text_command("MAG-CAL-STATUS");
  const char *out = test_get_uart_output();
  assert(strstr(out, "MAG-CAL: VALID") != NULL);
  assert(strstr(out, "offsets=") != NULL);
  printf("test_text_mag_cal_status_valid PASS\n");
}

void test_text_mag_cal_status_invalid(void)
{
  test_clear_uart_output();
  reset_mocks();
  mag_calib_start(); /* invalidate */

  test_run_text_command("MAG-CAL-STATUS");
  const char *out = test_get_uart_output();
  assert(strstr(out, "MAG-CAL: NOT CALIBRATED") != NULL);
  printf("test_text_mag_cal_status_invalid PASS\n");
}

void test_text_imu_cal_start(void)
{
  const char *out = run_cmd("IMU-CAL-START");
  assert(strstr(out, "IMU-CAL: started") != NULL);
  assert(imu_calib_is_valid() == false);
  printf("test_text_imu_cal_start PASS\n");
}

void test_text_imu_cal_stop(void)
{
  test_clear_uart_output();
  reset_mocks();
  imu_calib_start();
  test_run_text_command("IMU-CAL-STOP");
  const char *out = test_get_uart_output();
  assert(strstr(out, "IMU-CAL: finished") != NULL);
  assert(imu_calib_is_valid() == true);
  printf("test_text_imu_cal_stop PASS\n");
}

void test_text_imu_cal_status_valid(void)
{
  test_clear_uart_output();
  reset_mocks();
  imu_calib_finish();

  test_run_text_command("IMU-CAL-STATUS");
  const char *out = test_get_uart_output();
  assert(strstr(out, "IMU-CAL: VALID") != NULL);
  assert(strstr(out, "gyro_bias=") != NULL);
  printf("test_text_imu_cal_status_valid PASS\n");
}

void test_text_imu_cal_status_invalid(void)
{
  test_clear_uart_output();
  reset_mocks();
  imu_calib_start();

  test_run_text_command("IMU-CAL-STATUS");
  const char *out = test_get_uart_output();
  assert(strstr(out, "IMU-CAL: NOT CALIBRATED") != NULL);
  printf("test_text_imu_cal_status_invalid PASS\n");
}

void test_text_imu_cal_save(void)
{
  const char *out = run_cmd("IMU-CAL-SAVE");
  assert(strstr(out, "IMU-CAL: saved to flash") != NULL);
  printf("test_text_imu_cal_save PASS\n");
}

void test_text_imu_cal_load(void)
{
  const char *out = run_cmd("IMU-CAL-LOAD");
  assert(strstr(out, "IMU-CAL: loaded from flash") != NULL);
  printf("test_text_imu_cal_load PASS\n");
}

/* Diagnostic / test commands */
void test_text_i2cscan(void)
{
  const char *out = run_cmd("I2CSCAN");
  assert(strstr(out, "I2C: scanning") != NULL);
  assert(strstr(out, "found 0 device") != NULL);
  printf("test_text_i2cscan PASS\n");
}

void test_text_bh1750_test(void)
{
  const char *out = run_cmd("BH1750_TEST");
  assert(strstr(out, "BH1750:") != NULL);
  printf("test_text_bh1750_test PASS\n");
}

void test_text_bh1750_test_5c(void)
{
  const char *out = run_cmd("BH1750_TEST5C");
  assert(strstr(out, "BH1750:") != NULL);
  assert(strstr(out, "0x5C") != NULL);
  printf("test_text_bh1750_test_5c PASS\n");
}

void test_text_power_test(void)
{
  const char *out = run_cmd("POWER_TEST");
  assert(strstr(out, "POWER:") != NULL);
  assert(strstr(out, "V=") != NULL);
  assert(strstr(out, "3300 mV") != NULL);
  printf("test_text_power_test PASS\n");
}

void test_text_solar_test(void)
{
  const char *out = run_cmd("SOLAR_TEST");
  assert(strstr(out, "SOLAR:") != NULL);
  assert(strstr(out, "V=") != NULL);
  assert(strstr(out, "4200 mV") != NULL);
  printf("test_text_solar_test PASS\n");
}

void test_text_sht31_test(void)
{
  test_clear_uart_output();
  reset_mocks();
  /* mock_snapshot already has temp_valid=true, humidity_valid=true */
  test_run_text_command("SHT31_TEST");
  const char *out = test_get_uart_output();
  assert(strstr(out, "SHT31:") != NULL);
  assert(strstr(out, "temp=") != NULL);
  assert(strstr(out, "humidity=") != NULL);
  printf("test_text_sht31_test PASS\n");
}

void test_text_rtc_test(void)
{
  const char *out = run_cmd("RTC_TEST");
  assert(strstr(out, "RTC:") != NULL);
  assert(strstr(out, "no data") != NULL || strstr(out, "RTC:") != NULL);
  printf("test_text_rtc_test PASS\n");
}

/* Misc commands */
void test_text_log(void)
{
  const char *out = run_cmd("LOG");
  assert(strstr(out, "LOG: dump not implemented") != NULL);
  printf("test_text_log PASS\n");
}

void test_text_help(void)
{
  const char *out = run_cmd("HELP");
  assert(strstr(out, "System") != NULL);
  assert(strstr(out, "Mode") != NULL);
  assert(strstr(out, "GPS") != NULL);
  assert(strstr(out, "Calibration") != NULL);
  assert(strstr(out, "Tests") != NULL);
  assert(strstr(out, "Misc") != NULL);
  assert(strstr(out, "REBOOT") != NULL);
  assert(strstr(out, "STATUS") != NULL);
  assert(strstr(out, "HELP") != NULL);
  printf("test_text_help PASS\n");
}

void test_text_unknown(void)
{
  const char *out = run_cmd("BOGUS_COMMAND_XYZ");
  assert(strstr(out, "UNKNOWN CMD") != NULL);
  printf("test_text_unknown PASS\n");
}

void test_text_empty(void)
{
  const char *out = run_cmd("");
  /* Empty command — no match, should fall through to UNKNOWN */
  assert(strstr(out, "UNKNOWN CMD") != NULL);
  printf("test_text_empty PASS\n");
}

int main()
{
  printf("Running Command Task tests...\n");

  /* CSP packet command tests (existing) */
  test_command_echo();
  test_command_reboot();
  test_command_invalid();
  test_command_short_packet();
  test_command_set_mode();
  test_command_payload_capture();
  test_command_csp_deploy();
  test_command_gps_status();
  test_command_fault_list();
  test_command_sensor_reset();
  test_command_gps_reset_stats();
  test_command_telemetry_req();
  test_command_log_dump();
  test_command_packet_not_freed_after_default();
  test_command_status_post();

  /* Text command regression tests (existing) */
  test_text_deploy_from_boot();
  test_text_deploy_from_nominal();
  test_text_deploy_clear();
  test_text_mode_name();
  test_text_mode_range();
  test_text_deploy_from_safe();
  test_text_mode_atoi_non_numeric_rejected();
  test_text_mode_numeric_zero_parsed_correctly();
  test_settime_valid();
  test_settime_year_out_of_range();
  test_settime_month_out_of_range();
  test_settime_day_out_of_range();
  test_settime_hour_out_of_range();
  test_settime_minute_out_of_range();
  test_settime_second_out_of_range();
  test_settime_too_few_fields();

  /* Text command regression tests (new — comprehensive coverage) */
  test_text_echo();
  test_text_reboot();
  test_text_status();
  test_text_faults();
  test_text_capture();
  test_text_imgdump();

  test_text_mode_equals_name();
  test_text_mode_space_name();
  test_text_mode_equals_number();
  test_text_mode_space_number();
  test_text_mode_invalid_name();
  test_text_mode_invalid_number();
  test_text_mode_non_numeric();
  test_text_mode_parse_zero();

  test_text_gps_with_fix();
  test_text_gps_no_fix();
  test_text_gpsstats();
  test_text_resetgps();
  test_text_resetgps_cold();
  test_text_reset_usage();

  test_text_mag_cal_start();
  test_text_mag_cal_stop();
  test_text_mag_cal_status_valid();
  test_text_mag_cal_status_invalid();

  test_text_imu_cal_start();
  test_text_imu_cal_stop();
  test_text_imu_cal_status_valid();
  test_text_imu_cal_status_invalid();
  test_text_imu_cal_save();
  test_text_imu_cal_load();

  test_text_i2cscan();
  test_text_bh1750_test();
  test_text_bh1750_test_5c();
  test_text_power_test();
  test_text_solar_test();
  test_text_sht31_test();
  test_text_rtc_test();

  test_text_log();
  test_text_help();
  test_text_unknown();
  test_text_empty();

  printf("All tests passed!\n");
  return 0;
}
