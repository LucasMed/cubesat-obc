/**
 * @file test_post.c
 * @brief Unit tests for the Power-On Self-Test suite.
 *
 * Tests the pure-logic POST functions (boot reason names, critical-failure
 * detection) that are independent of hardware.  Hardware-dependent paths
 * (sensor reads, flash write) are stubbed at compile time via the
 * PICO_BUILD guard.
 *
 * Spec ref: FMM-SPEC-090–100, FMM-DES-001 §4
 */

#include "../../include/post.h"
#include "../../include/fault_manager.h"
#include "../../include/logger.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Mock stubs for external dependencies                                 */
/* ------------------------------------------------------------------ */

/* post.c calls log_event() and fault_report() — provide no-op stubs. */
void log_event(uint16_t event_id, log_class_t log_class, const void *data, uint8_t data_len)
{
  (void)event_id;
  (void)log_class;
  (void)data;
  (void)data_len;
}

void fault_report(uint16_t fault_id, fault_level_t level)
{
  (void)fault_id;
  (void)level;
}

/* ------------------------------------------------------------------ */
/* Test helpers                                                        */
/* ------------------------------------------------------------------ */

static int g_failures = 0;

#define CHECK(cond, msg)                                                                           \
  do                                                                                               \
  {                                                                                                \
    if (!(cond))                                                                                   \
    {                                                                                              \
      printf("FAIL [%s:%d]: %s\n", __FILE__, __LINE__, (msg));                                     \
      g_failures++;                                                                                \
    }                                                                                              \
  } while (0)

/* ------------------------------------------------------------------ */
/* Test: boot_reason_name mapping                                      */
/* ------------------------------------------------------------------ */

static void test_boot_reason_name(void)
{
  /* Valid codes */
  CHECK(strcmp(post_boot_reason_name(POST_BOOT_POWER_ON), "POWER_ON") == 0,
        "POWER_ON name mismatch");
  CHECK(strcmp(post_boot_reason_name(POST_BOOT_WATCHDOG), "WATCHDOG") == 0,
        "WATCHDOG name mismatch");
  CHECK(strcmp(post_boot_reason_name(POST_BOOT_STACK_OVERFLOW), "STACK_OVERFLOW") == 0,
        "STACK_OVERFLOW name mismatch");
  CHECK(strcmp(post_boot_reason_name(POST_BOOT_BROWNOUT), "BROWNOUT") == 0,
        "BROWNOUT name mismatch");

  /* Unknown code */
  CHECK(strcmp(post_boot_reason_name(99), "UNKNOWN") == 0,
        "unknown code must return 'UNKNOWN'");
  CHECK(strcmp(post_boot_reason_name(0xFF), "UNKNOWN") == 0,
        "0xFF must return 'UNKNOWN'");

  printf("test_boot_reason_name: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test: critical failure detection                                     */
/* ------------------------------------------------------------------ */

static void test_critical_fail(void)
{
  post_record_t rec;

  /* All tests pass → not critical */
  memset(&rec, 0, sizeof(rec));
  rec.test_bitmap = (1u << POST_TEST_IMU) | (1u << POST_TEST_RTC) | (1u << POST_TEST_FLASH);
  CHECK(!post_is_critical_fail(&rec), "all critical bits set → not critical");

  /* All ten tests pass → not critical */
  rec.test_bitmap = (1u << POST_TEST_COUNT) - 1u;
  CHECK(!post_is_critical_fail(&rec), "all 10 bits set → not critical");

  /* IMU alone fails → critical */
  rec.test_bitmap = (1u << POST_TEST_RTC) | (1u << POST_TEST_FLASH);
  CHECK(post_is_critical_fail(&rec), "IMU fail → critical");

  /* RTC alone fails → critical */
  rec.test_bitmap = (1u << POST_TEST_IMU) | (1u << POST_TEST_FLASH);
  CHECK(post_is_critical_fail(&rec), "RTC fail → critical");

  /* Flash alone fails → critical */
  rec.test_bitmap = (1u << POST_TEST_IMU) | (1u << POST_TEST_RTC);
  CHECK(post_is_critical_fail(&rec), "Flash fail → critical");

  /* All three critical fail → critical */
  rec.test_bitmap = 0;
  CHECK(post_is_critical_fail(&rec), "all critical fail → critical");

  /* Non-critical test fails but critical pass → not critical */
  rec.test_bitmap = (1u << POST_TEST_IMU) | (1u << POST_TEST_RTC)
                  | (1u << POST_TEST_FLASH) | (1u << POST_TEST_BH1750)
                  | (1u << POST_TEST_SHT31) | (1u << POST_TEST_GPS);
  /* BH1750, SHT31, GPS are non-critical */
  CHECK(!post_is_critical_fail(&rec), "non-critical fail only → not critical");

  /* NULL pointer → critical */
  CHECK(post_is_critical_fail(NULL), "NULL record → critical");

  printf("test_critical_fail: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test: post_run produces a valid record on host build                 */
/* ------------------------------------------------------------------ */

static void test_run_host(void)
{
  post_record_t rec;
  memset(&rec, 0, sizeof(rec));

  post_run(&rec);

  /* On host build (no PICO_BUILD), all sensor tests behave as pass */
  CHECK(rec.magic == POST_MAGIC, "magic must be POST_MAGIC");
  CHECK(rec.boot_count == 1u, "first boot count must be 1");
  CHECK(rec.boot_reason == POST_BOOT_POWER_ON,
        "host build defaults to POWER_ON boot reason");

  /* All tests should have executed and passed on host */
  uint32_t expected_detail = (1u << POST_TEST_COUNT) - 1u;
  CHECK(rec.test_detail == expected_detail,
        "all tests must be marked as executed on host");
  CHECK(rec.test_bitmap == expected_detail,
        "all tests must pass on host (stubs return true)");

  /* CRC32 must be non-zero (valid CRC) */
  CHECK(rec.crc32 != 0, "CRC32 must be computed");

  printf("test_run_host: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test: post_read_last on host returns empty                           */
/* ------------------------------------------------------------------ */

static void test_read_last_host(void)
{
  post_record_t rec;
  memset(&rec, 0xAA, sizeof(rec)); /* fill with garbage */

  post_read_last(&rec);

  /* On host, flash access is a no-op, so record should be zeroed */
  CHECK(rec.magic == 0, "post_read_last on host must return zeroed record");
  CHECK(rec.boot_count == 0, "boot_count must be 0");
  CHECK(rec.test_bitmap == 0, "test_bitmap must be 0");

  printf("test_read_last_host: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test: NULL-safe API calls                                           */
/* ------------------------------------------------------------------ */

static void test_null_safety(void)
{
  /* These must not crash */
  post_run(NULL);
  post_read_last(NULL);

  /* post_is_critical_fail(NULL) returns true */
  CHECK(post_is_critical_fail(NULL), "NULL safety: is_critical_fail(NULL)");

  printf("test_null_safety: OK\n");
}

/* ------------------------------------------------------------------ */
/* Entry point                                                         */
/* ------------------------------------------------------------------ */

int main(void)
{
  test_boot_reason_name();
  test_critical_fail();
  test_run_host();
  test_read_last_host();
  test_null_safety();

  if (g_failures == 0)
  {
    printf("All POST checks passed.\n");
    return 0;
  }
  printf("%d POST check(s) FAILED.\n", g_failures);
  return 1;
}
