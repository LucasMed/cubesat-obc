/**
 * @file test_payload_integration.c
 * @brief Integration tests for the payload subsystem logic.
 *
 * Verifies the interaction between payload_task, payload_manager,
 * and the system state (data_layer, fmm) on the host environment.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ---- Logger stub (flight_mode_manager.c calls log_event) --------------- */
void log_event(uint16_t event_id, uint8_t log_class, const void *data, uint8_t data_len)
{
  (void)event_id;
  (void)log_class;
  (void)data;
  (void)data_len;
}

/* ---- FreeRTOS host stubs ----------------------------------------------- */
#include "FreeRTOS.h"
#include "task.h"

static uint32_t s_notify_value = 0;
static bool s_notify_pending = false;

BaseType_t xTaskNotifyWait_Mock(uint32_t ulBitsToClearOnEntry, uint32_t ulBitsToClearOnExit,
                                uint32_t *pulNotificationValue, TickType_t xTicksToWait)
{
  (void)ulBitsToClearOnEntry;
  (void)ulBitsToClearOnExit;
  (void)xTicksToWait;
  if (s_notify_pending)
  {
    *pulNotificationValue = s_notify_value;
    s_notify_pending = false;
    return pdTRUE;
  }
  return pdFALSE;
}

#include "camera_driver.h"
#include "data_layer.h"
#include "flight_mode.h"
#include "payload_manager.h"
#include "payload_task.h"
#include "radiation_driver.h"
#include "rm3100.h"
#include "storage_manager.h"

/* ---- Hardware Mocks ---------------------------------------------------- */
static bool g_rm3100_init_called = false;
static bool g_camera_init_called = false;
static int g_storage_log_calls = 0;
static int g_storage_img_calls = 0;

bool rm3100_init(void)
{
  g_rm3100_init_called = true;
  return true;
}
bool rm3100_read_vector(rm3100_vector_t *vec, uint32_t timeout_ms)
{
  (void)timeout_ms;
  vec->x_nT = 100.0f;
  vec->y_nT = 200.0f;
  vec->z_nT = 300.0f;
  return true;
}
void rm3100_get_last(rm3100_vector_t *vec)
{
  vec->x_nT = 100.0f;
  vec->y_nT = 200.0f;
  vec->z_nT = 300.0f;
}

bool camera_init(void)
{
  g_camera_init_called = true;
  return true;
}
bool camera_capture(uint32_t timeout_ms)
{
  (void)timeout_ms;
  return true;
}
uint32_t camera_get_fifo_length(void)
{
  return 1024;
}
bool camera_read_fifo_burst(uint8_t *buffer, size_t length)
{
  memset(buffer, 0xAA, length);
  return true;
}
bool camera_arduchip_diagnostic(void)
{
  return true;
}
void camera_verify_jpeg_config(void)
{
}
void camera_set_test_pattern(bool enable)
{
  (void)enable;
}
bool camera_write_sensor_reg(uint8_t reg, uint8_t val)
{
  (void)reg;
  (void)val;
  return true;
}
bool camera_read_sensor_reg(uint8_t reg, uint8_t *val)
{
  (void)reg;
  if (val) *val = 0x41; /* mimic COM7=UXGA+JPEG after init */
  return true;
}
void camera_diagnostic_readback(void)
{
}
bool camera_set_resolution(camera_res_t res)
{
  (void)res;
  return true;
}

bool radiation_driver_init(void)
{
  return true;
}
float radiation_driver_read_dose(void)
{
  return 1.23f;
}

storage_status_t storage_init(void)
{
  return STORAGE_OK;
}
storage_status_t storage_append_log(const char *filename, const void *data, size_t size)
{
  g_storage_log_calls++;
  return STORAGE_OK;
}
storage_status_t storage_write_image(const char *filename, const uint8_t *data, uint32_t size)
{
  g_storage_img_calls++;
  return STORAGE_OK;
}

#ifndef PICO_BUILD
void gpio_put(unsigned int pin, int value)
{
  (void)pin;
  (void)value;
}
void gpio_init(unsigned int pin)
{
  (void)pin;
}
void gpio_set_dir(unsigned int pin, bool out)
{
  (void)pin;
  (void)out;
}
void sleep_ms(uint32_t ms)
{
  (void)ms;
}
#endif

/* ---- Test helpers ------------------------------------------------------- */
static int g_failures = 0;
#define CHECK(cond, msg)                                                                           \
  do                                                                                               \
  {                                                                                                \
    if (!(cond))                                                                                   \
    {                                                                                              \
      printf("  FAIL [%s:%d] %s\n", __func__, __LINE__, (msg));                                    \
      g_failures++;                                                                                \
    }                                                                                              \
  } while (0)

static void reset_all(void)
{
  data_layer_init();
  flight_mode_manager_init(); /* FM_BOOT */
  payload_task_reset();
  g_rm3100_init_called = false;
  g_camera_init_called = false;
  g_storage_log_calls = 0;
  g_storage_img_calls = 0;
  s_notify_pending = false;
  payload_manager_init();
}

/** Helper to transition to any mode by following allowed paths */
static void force_to_nominal(void)
{
  fmm_request_transition(FM_DETUMBLE);
  fmm_request_transition(FM_NOMINAL);
}

/* ========================================================================
 * T-PLD-INT-01: Rail state follows Mode
 * ======================================================================== */
static void test_rail_follows_mode(void)
{
  reset_all();
  CHECK(fmm_get_mode() == FM_BOOT, "T-PLD-INT-01: Start in BOOT");
  CHECK(!payload_manager_get_status().rail_enabled, "T-PLD-INT-01: Rail OFF in BOOT");

  force_to_nominal();
  vPayloadTask_Step();
  CHECK(fmm_get_mode() == FM_NOMINAL, "T-PLD-INT-01: Transitioned to NOMINAL");
  CHECK(!payload_manager_get_status().rail_enabled, "T-PLD-INT-01: Rail OFF in NOMINAL");

  fmm_request_transition(FM_PAYLOAD);
  vPayloadTask_Step();
  CHECK(fmm_get_mode() == FM_PAYLOAD, "T-PLD-INT-01: Transitioned to PAYLOAD");
  CHECK(payload_manager_get_status().rail_enabled, "T-PLD-INT-01: Rail ON in PAYLOAD");
  CHECK(g_rm3100_init_called, "T-PLD-INT-01: Devices init when rail enabled");

  fmm_request_transition(FM_NOMINAL);
  vPayloadTask_Step();
  CHECK(fmm_get_mode() == FM_NOMINAL, "T-PLD-INT-01: Back to NOMINAL");
  CHECK(!payload_manager_get_status().rail_enabled, "T-PLD-INT-01: Rail OFF when leaving PAYLOAD");

  printf("  PASS T-PLD-INT-01 Rail state follows Mode\n");
}

/* ========================================================================
 * T-PLD-INT-02: Capture Image via Notification
 * ======================================================================== */
static void test_capture_image(void)
{
  reset_all();
  force_to_nominal();
  fmm_request_transition(FM_PAYLOAD);
  vPayloadTask_Step(); /* Enable rail */

  CHECK(payload_manager_get_status().rail_enabled, "T-PLD-INT-02 pre: Rail must be ON");

  s_notify_value = PAYLOAD_NOTIFY_CAPTURE_IMAGE;
  s_notify_pending = true;

  vPayloadTask_Step();

  CHECK(g_camera_init_called, "T-PLD-INT-02: Camera init during capture");
  CHECK(g_storage_img_calls >= 1, "T-PLD-INT-02: Image saved to storage (test pattern capture may add extra)");

  printf("  PASS T-PLD-INT-02 Capture Image via Notification\n");
}

/* ========================================================================
 * T-PLD-INT-03: Periodic sampling and logging
 * ======================================================================== */
static void test_periodic_logging(void)
{
  reset_all();
  force_to_nominal();
  fmm_request_transition(FM_PAYLOAD);

  /* Step 0: will trigger 1Hz logging because counter % 10 == 0 */
  vPayloadTask_Step();
  CHECK(g_storage_log_calls == 1, "T-PLD-INT-03: First step logged");

  /* Run for 9 more steps (reaches counter 10) to trigger next 1Hz logging */
  for (int i = 0; i < 9; i++)
  {
    vPayloadTask_Step();
  }
  vPayloadTask_Step(); /* Counter 10 */

  CHECK(g_storage_log_calls >= 2, "T-PLD-INT-03: Science data logged again at 1s mark");

  /* Verify DLA updated */
  dl_snapshot_t snap;
  data_layer_read(&snap);
  CHECK(snap.state.mag_field[0] == 100.0f, "T-PLD-INT-03: DLA Mag X updated");
  CHECK(snap.state.radiation_dose == 1.23f, "T-PLD-INT-03: DLA Radiation updated");

  printf("  PASS T-PLD-INT-03 Periodic sampling and logging\n");
}

int main(void)
{
  printf("=== Payload Integration Tests (WP-7.8) ===\n");

  test_rail_follows_mode();
  test_capture_image();
  test_periodic_logging();

  if (g_failures == 0)
  {
    printf("ALL WP-7.8 TESTS PASSED\n");
    return 0;
  }

  printf("%d TEST(S) FAILED\n", g_failures);
  return 1;
}
