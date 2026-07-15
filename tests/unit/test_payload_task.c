/**
 * @file test_payload_task.c
 * @brief Regression tests for payload_task.c — JPEG image capture path.
 *
 * Provides strong-symbol stubs for all hardware dependencies so the
 * camera-init → capture → read → validate → store pipeline can be
 * exercised without real hardware.
 *
 * Tests
 * -----
 *   T-PAYLOAD-01 — storage_write_image is called with actual JPEG size
 *                   (img_size from EOI marker), not sizeof(full buffer)
 */

#include "camera_driver.h"
#include "data_layer.h"
#include "flight_mode.h"
#include "payload_manager.h"
#include "radiation_driver.h"
#include "rm3100.h"
#include "storage_manager.h"
#include "wcet_profiler.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Test infrastructure                                                 */
/* ------------------------------------------------------------------ */

#define PASS(label) printf("  PASS %s\n", (label))
#define FAIL(label, msg)                                                                           \
  do                                                                                               \
  {                                                                                                \
    printf("  FAIL %s: %s\n", (label), (msg));                                                     \
    g_failures++;                                                                                  \
  } while (0)

static int g_failures = 0;

/* ------------------------------------------------------------------ */
/* FreeRTOS mocks (must appear before including payload_task.h)        */
/* ------------------------------------------------------------------ */

#include "FreeRTOS.h"
#include "task.h"

static uint32_t s_tick = 0;
static int s_notify_wait_ret = pdFAIL;
static uint32_t s_notify_writeme = 0;

/* xTaskNotifyWait_Mock is declared in host/FreeRTOS.h and called by
 * payload_task.c via the xTaskNotifyWait macro. Provide strong symbol. */
BaseType_t xTaskNotifyWait_Mock(uint32_t ulBitsToClearOnEntry, uint32_t ulBitsToClearOnExit,
                                uint32_t *pulNotificationValue, TickType_t xTicksToWait)
{
  (void)ulBitsToClearOnEntry;
  (void)ulBitsToClearOnExit;
  (void)xTicksToWait;
  if (pulNotificationValue)
    *pulNotificationValue = s_notify_writeme;
  return s_notify_wait_ret;
}

/* ------------------------------------------------------------------ */
/* Camera driver stubs                                                 */
/* ------------------------------------------------------------------ */

static bool s_camera_init_ret = false;
static bool s_camera_capture_ret = false;
static bool s_camera_fifo_read_ret = false;

/* Buffer holding the JPEG data that camera_read_fifo_burst will "read" */
#define JPEG_BUF_SIZE (128u * 1024u)
static uint8_t s_camera_fifo_data[JPEG_BUF_SIZE];
static size_t s_camera_fifo_data_len = 0;

bool camera_init(void)
{
  return s_camera_init_ret;
}

bool camera_capture(uint32_t timeout_ms)
{
  (void)timeout_ms;
  return s_camera_capture_ret;
}

bool camera_read_fifo_burst(uint8_t *buffer, size_t length)
{
  if (!s_camera_fifo_read_ret)
    return false;

  size_t copy_len = (length < s_camera_fifo_data_len) ? length : s_camera_fifo_data_len;
  (void)memcpy(buffer, s_camera_fifo_data, copy_len);
  /* Fill remainder with 0xFF (common for unread FIFO areas) */
  if (copy_len < length)
  {
    (void)memset(buffer + copy_len, 0xFF, length - copy_len);
  }
  return true;
}

void camera_clear_fifo(void) {}

/* ------------------------------------------------------------------ */
/* Storage manager stubs                                               */
/* ------------------------------------------------------------------ */

static int s_storage_write_calls = 0;
static uint32_t s_last_write_size = 0;
static char s_last_write_filename[64];
static storage_status_t s_storage_write_ret = STORAGE_OK;

storage_status_t storage_write_image(const char *filename, const uint8_t *data, uint32_t size)
{
  (void)data;
  s_storage_write_calls++;
  s_last_write_size = size;
  (void)snprintf(s_last_write_filename, sizeof(s_last_write_filename), "%s", filename);
  return s_storage_write_ret;
}

/* Unused in capture path but required at link time */
storage_status_t storage_init(void)
{
  return STORAGE_OK;
}
storage_status_t storage_append_log(const char *filename, const void *data, size_t size)
{
  (void)filename;
  (void)data;
  (void)size;
  return STORAGE_OK;
}
uint64_t storage_get_free_space(void)
{
  return 0;
}

/* ------------------------------------------------------------------ */
/* Flight mode stub — always return NON_PAYLOAD so sampling is skipped */
/* ------------------------------------------------------------------ */

flight_mode_t fmm_get_mode(void)
{
  return FM_NOMINAL;
}

/* ------------------------------------------------------------------ */
/* Payload manager stubs                                               */
/* ------------------------------------------------------------------ */

static int s_payload_manager_init_calls = 0;
void payload_manager_init(void)
{
  s_payload_manager_init_calls++;
}
void payload_manager_increment_image_count(void) {}
payload_status_t payload_manager_get_status(void)
{
  payload_status_t s;
  (void)memset(&s, 0, sizeof(s));
  s.rail_enabled = false;
  return s;
}
bool payload_manager_enable(bool on)
{
  (void)on;
  return true;
}

/* ------------------------------------------------------------------ */
/* Radiation driver stub                                               */
/* ------------------------------------------------------------------ */

float radiation_driver_read_dose(void)
{
  return 0.0f;
}

/* ------------------------------------------------------------------ */
/* RM3100 stub                                                         */
/* ------------------------------------------------------------------ */

bool rm3100_read_vector(rm3100_vector_t *vec, uint32_t timeout_ms)
{
  (void)vec;
  (void)timeout_ms;
  return false;
}

/* ------------------------------------------------------------------ */
/* Data layer stubs                                                    */
/* ------------------------------------------------------------------ */

void data_layer_write_mag(const float field[3]) { (void)field; }
void data_layer_write_radiation(float dose) { (void)dose; }
void data_layer_write_payload_status(bool rail_enabled, uint16_t image_count)
{
  (void)rail_enabled;
  (void)image_count;
}

/* ------------------------------------------------------------------ */
/* Unit under test                                                     */
/* ------------------------------------------------------------------ */

#include "payload_task.h"

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

/* JPEG_BUF_SIZE from the camera stubs — should NOT be the size passed to storage */

static void reset_all(void)
{
  s_tick = 0;
  s_notify_wait_ret = pdFAIL;
  s_notify_writeme = 0;

  s_camera_init_ret = false;
  s_camera_capture_ret = false;
  s_camera_fifo_read_ret = false;
  s_camera_fifo_data_len = 0;
  (void)memset(s_camera_fifo_data, 0, sizeof(s_camera_fifo_data));

  s_storage_write_calls = 0;
  s_last_write_size = 0;
  s_storage_write_ret = STORAGE_OK;
  (void)memset(s_last_write_filename, 0, sizeof(s_last_write_filename));

  s_payload_manager_init_calls = 0;

  payload_task_reset();
}

/**
 * Build a minimal valid JPEG in s_camera_fifo_data:
 *   SOI (0xFF 0xD8) + a few bytes + EOI (0xFF 0xD9)
 * Returns the total JPEG size.
 */
static size_t build_test_jpeg(uint32_t extra_bytes)
{
  size_t pos = 0;
  s_camera_fifo_data[pos++] = 0xFF;
  s_camera_fifo_data[pos++] = 0xD8; /* SOI */

  /* Add plausible JPEG marker segments: APP0 + DQT + SOF + DHT + SOS */
  const uint8_t filler[] = {
      0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x00, /* APP0 */
      0xFF, 0xDB, 0x00, 0x43, 0x00,                         /* DQT */
      0xFF, 0xC0, 0x00, 0x0B, 0x08, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x11, 0x00, /* SOF */
      0xFF, 0xC4, 0x00, 0x1F, 0x00, 0x00,                         /* DHT */
      0xFF, 0xDA, 0x00, 0x08, 0x01, 0x01, 0x00, 0x00, 0x3F, 0x00, /* SOS */
  };
  for (size_t i = 0; i < sizeof(filler); i++)
  {
    if (pos < sizeof(s_camera_fifo_data))
      s_camera_fifo_data[pos++] = filler[i];
  }

  /* Extra entropy bytes (compressed data) */
  for (uint32_t i = 0; i < extra_bytes && pos < sizeof(s_camera_fifo_data); i++)
  {
    s_camera_fifo_data[pos++] = (uint8_t)(i & 0xFF);
  }

  if (pos >= sizeof(s_camera_fifo_data) - 2)
  {
    s_camera_fifo_data_len = 0;
    return 0;
  }
  s_camera_fifo_data[pos++] = 0xFF;
  s_camera_fifo_data[pos++] = 0xD9; /* EOI */

  s_camera_fifo_data_len = pos;
  return pos;
}

/* ------------------------------------------------------------------ */
/* T-PAYLOAD-01: storage_write_image uses actual JPEG size, not buf    */
/* ------------------------------------------------------------------ */

static void test_storage_uses_actual_jpeg_size(void)
{
  reset_all();

  /* Arrange: valid JPEG with some extra entropy bytes */
  uint32_t extra = 64;
  size_t jpeg_size = build_test_jpeg(extra);
  (void)jpeg_size;

  s_camera_init_ret = true;
  s_camera_capture_ret = true;
  s_camera_fifo_read_ret = true;

  /* Trigger a CAPTURE_IMAGE notification */
  s_notify_wait_ret = pdTRUE;
  s_notify_writeme = PAYLOAD_NOTIFY_CAPTURE_IMAGE;

  /* Act */
  vPayloadTask_Step();

  /* Assert: storage_write_image was called with jpeg_size, NOT sizeof(full buf) */
  if (s_storage_write_calls != 1)
  {
    FAIL("T-PAYLOAD-01", "storage_write_image was not called");
    return;
  }

  if (s_last_write_size == jpeg_size)
  {
    PASS("T-PAYLOAD-01 storage_write_image called with actual JPEG size");
  }
  else if (s_last_write_size == JPEG_BUF_SIZE)
  {
    FAIL("T-PAYLOAD-01", "storage_write_image called with full buffer size instead of actual JPEG size");
    printf("       JPEG size = %zu, write size = %lu (BUG: used sizeof buffer)\n",
           jpeg_size, (unsigned long)s_last_write_size);
  }
  else
  {
    FAIL("T-PAYLOAD-01", "storage_write_image called with unexpected size");
    printf("       expected %zu, got %lu\n", jpeg_size,
           (unsigned long)s_last_write_size);
  }
}

/* ------------------------------------------------------------------ */
/* T-PAYLOAD-02: storage_write_image NOT called when camera init fails */
/* ------------------------------------------------------------------ */

static void test_no_storage_on_init_fail(void)
{
  reset_all();

  s_camera_init_ret = false;
  s_notify_wait_ret = pdTRUE;
  s_notify_writeme = PAYLOAD_NOTIFY_CAPTURE_IMAGE;

  vPayloadTask_Step();

  if (s_storage_write_calls == 0)
  {
    PASS("T-PAYLOAD-02 no storage write on camera_init failure");
  }
  else
  {
    FAIL("T-PAYLOAD-02", "storage_write_image called despite camera_init failure");
  }
}

/* ------------------------------------------------------------------ */
/* T-PAYLOAD-03: storage_write_image NOT called when FIFO read fails   */
/* ------------------------------------------------------------------ */

static void test_no_storage_on_fifo_read_fail(void)
{
  reset_all();

  s_camera_init_ret = true;
  s_camera_capture_ret = true;
  s_camera_fifo_read_ret = false;
  s_notify_wait_ret = pdTRUE;
  s_notify_writeme = PAYLOAD_NOTIFY_CAPTURE_IMAGE;

  vPayloadTask_Step();

  if (s_storage_write_calls == 0)
  {
    PASS("T-PAYLOAD-03 no storage write on FIFO read failure");
  }
  else
  {
    FAIL("T-PAYLOAD-03", "storage_write_image called despite FIFO read failure");
  }
}

/* ------------------------------------------------------------------ */
/* T-PAYLOAD-04: storage_write_image failure is handled gracefully     */
/* ------------------------------------------------------------------ */

static void test_storage_failure_does_not_crash(void)
{
  reset_all();

  /* Build a valid JPEG */
  build_test_jpeg(64);
  s_camera_init_ret = true;
  s_camera_capture_ret = true;
  s_camera_fifo_read_ret = true;
  s_storage_write_ret = STORAGE_ERR_WRITE; /* Simulate write failure */

  s_notify_wait_ret = pdTRUE;
  s_notify_writeme = PAYLOAD_NOTIFY_CAPTURE_IMAGE;

  vPayloadTask_Step();

  /* Should have attempted the write */
  if (s_storage_write_calls == 1)
  {
    PASS("T-PAYLOAD-04 storage failure does not crash, write was attempted");
  }
  else
  {
    FAIL("T-PAYLOAD-04", "storage_write_image was not called on failure path");
  }
}

/* ------------------------------------------------------------------ */
/* T-PAYLOAD-05: camera_capture fail in CAPTURE path does not write    */
/* ------------------------------------------------------------------ */

static void test_capture_capture_fail(void)
{
  reset_all();

  s_camera_init_ret = true;
  s_camera_capture_ret = false;
  s_notify_wait_ret = pdTRUE;
  s_notify_writeme = PAYLOAD_NOTIFY_CAPTURE_IMAGE;

  vPayloadTask_Step();

  if (s_storage_write_calls == 0)
  {
    PASS("T-PAYLOAD-05 no storage write on camera_capture failure");
  }
  else
  {
    FAIL("T-PAYLOAD-05", "storage_write_image called despite camera_capture failure");
  }
}

/* ------------------------------------------------------------------ */
/* T-PAYLOAD-06: payload_task_init calls payload_manager_init          */
/* ------------------------------------------------------------------ */

static void test_payload_task_init(void)
{
  reset_all();

  payload_task_init();

  if (s_payload_manager_init_calls > 0)
  {
    PASS("T-PAYLOAD-06 payload_task_init called payload_manager_init");
  }
  else
  {
    FAIL("T-PAYLOAD-06", "payload_manager_init was NOT called");
  }
}

/* ------------------------------------------------------------------ */
/* T-PAYLOAD-07: DUMP_IMAGE path succeeds with all camera ops OK       */
/* ------------------------------------------------------------------ */

static void test_dump_image_success(void)
{
  reset_all();

  /* Small JPEG so hex dump loop finishes quickly */
  build_test_jpeg(32);
  s_camera_init_ret = true;
  s_camera_capture_ret = true;
  s_camera_fifo_read_ret = true;

  s_notify_wait_ret = pdTRUE;
  s_notify_writeme = PAYLOAD_NOTIFY_DUMP_IMAGE;

  vPayloadTask_Step();

  /* No assertion hooks for hex dump — just verify no crash */
  PASS("T-PAYLOAD-07 DUMP_IMAGE success path completed without crash");
}

/* ------------------------------------------------------------------ */
/* T-PAYLOAD-08: DUMP_IMAGE handles camera_init failure                */
/* ------------------------------------------------------------------ */

static void test_dump_image_init_fail(void)
{
  reset_all();

  s_camera_init_ret = false;
  s_notify_wait_ret = pdTRUE;
  s_notify_writeme = PAYLOAD_NOTIFY_DUMP_IMAGE;

  vPayloadTask_Step();

  PASS("T-PAYLOAD-08 DUMP_IMAGE init failure handled without crash");
}

/* ------------------------------------------------------------------ */
/* T-PAYLOAD-09: DUMP_IMAGE handles camera_capture failure             */
/* ------------------------------------------------------------------ */

static void test_dump_image_capture_fail(void)
{
  reset_all();

  s_camera_init_ret = true;
  s_camera_capture_ret = false;
  s_notify_wait_ret = pdTRUE;
  s_notify_writeme = PAYLOAD_NOTIFY_DUMP_IMAGE;

  vPayloadTask_Step();

  PASS("T-PAYLOAD-09 DUMP_IMAGE capture failure handled without crash");
}

/* ------------------------------------------------------------------ */
/* T-PAYLOAD-10: DUMP_IMAGE handles FIFO read failure                  */
/* ------------------------------------------------------------------ */

static void test_dump_image_fifo_fail(void)
{
  reset_all();

  s_camera_init_ret = true;
  s_camera_capture_ret = true;
  s_camera_fifo_read_ret = false;
  s_notify_wait_ret = pdTRUE;
  s_notify_writeme = PAYLOAD_NOTIFY_DUMP_IMAGE;

  vPayloadTask_Step();

  PASS("T-PAYLOAD-10 DUMP_IMAGE FIFO failure handled without crash");
}

/* ------------------------------------------------------------------ */
/* T-PAYLOAD-11: unknown notify value is handled gracefully            */
/* ------------------------------------------------------------------ */

static void test_unknown_notify_value(void)
{
  reset_all();

  /* A bit pattern that matches neither CAPTURE_IMAGE (bit 0) nor
   * DUMP_IMAGE (bit 2) — use bit 1 only */
  s_notify_wait_ret = pdTRUE;
  s_notify_writeme = 0x2;

  vPayloadTask_Step();

  PASS("T-PAYLOAD-11 unknown notify value handled without crash");
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

int main(void)
{
  printf("=== Payload task unit tests ===\n");

  test_storage_uses_actual_jpeg_size();
  test_no_storage_on_init_fail();
  test_no_storage_on_fifo_read_fail();
  test_storage_failure_does_not_crash();
  test_capture_capture_fail();
  test_payload_task_init();
  test_dump_image_success();
  test_dump_image_init_fail();
  test_dump_image_capture_fail();
  test_dump_image_fifo_fail();
  test_unknown_notify_value();

  if (g_failures == 0)
  {
    printf("ALL PAYLOAD TASK TESTS PASSED\n");
    return 0;
  }
  else
  {
    printf("SOME TESTS FAILED (%d failure(s))\n", g_failures);
    return 1;
  }
}
