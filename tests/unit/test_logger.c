/**
 * @file test_logger.c
 * @brief PR-6 gate: Persistent Event Logger correctness checks.
 *
 * Tests:
 *   1.  logger_init() writes LOG_EVT_SYSTEM_BOOT as the first entry
 *   2.  log_event() stores an event; log_read_recent() retrieves it
 *   3.  log_read_recent(count > stored) returns only what is stored
 *   4.  Data payload is preserved when data_len <= 32
 *   5.  Data payload is silently clipped to 32 bytes
 *   6.  log_clear_info() only removes Class-C; Class-A and Class-B survive
 *   7.  log_read_recent() returns events newest-first
 *   8.  Ring wrap: oldest Class-C entry is overwritten when ring is full
 *   9.  Class-A entries are NOT overwritten when the ring is full
 *   10. event_id == 0 is rejected (not stored)
 *   11. subsystem field equals the high byte of event_id
 *   12. log_read_recent() yields 0 for a NULL output pointer
 */

#include "../../include/log_event_ids.h"
#include "../../include/logger.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

/** Ring capacity — must match the value in logger.c */
#define RING_CAP 320u

/* ------------------------------------------------------------------ */
/* Test 1: init writes system-boot event                               */
/* ------------------------------------------------------------------ */

static void test_init_writes_boot(void)
{
  logger_init();

  log_event_t ev = {0};
  size_t n = log_read_recent(&ev, 1u);
  CHECK(n == 1u, "log_read_recent must return 1 after init");
  CHECK(ev.event_id == LOG_EVT_SYSTEM_BOOT, "first event must be LOG_EVT_SYSTEM_BOOT");
  CHECK(ev.severity == (uint8_t)LOG_CLASS_INFO, "boot event must be LOG_CLASS_INFO");
  printf("test_init_writes_boot: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 2: log_event stores; log_read_recent retrieves                 */
/* ------------------------------------------------------------------ */

static void test_store_and_retrieve(void)
{
  logger_init();
  log_event(LOG_EVT_MODE_CHANGE, LOG_CLASS_OPERATIONAL, NULL, 0u);

  log_event_t buf[2] = {{0}};
  size_t n = log_read_recent(buf, 2u);
  CHECK(n == 2u, "two events expected (boot + mode_change)");
  /* Newest first → buf[0] is LOG_EVT_MODE_CHANGE */
  CHECK(buf[0].event_id == LOG_EVT_MODE_CHANGE, "newest event must be MODE_CHANGE");
  CHECK(buf[1].event_id == LOG_EVT_SYSTEM_BOOT, "second event must be SYSTEM_BOOT");
  printf("test_store_and_retrieve: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 3: count > stored returns actual count                         */
/* ------------------------------------------------------------------ */

static void test_read_capped_to_stored(void)
{
  logger_init(); /* 1 event */
  log_event_t buf[10] = {{0}};
  size_t n = log_read_recent(buf, 10u);
  CHECK(n == 1u, "read_recent must not return more events than stored");
  printf("test_read_capped_to_stored: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 4: payload preserved when data_len <= 32                       */
/* ------------------------------------------------------------------ */

static void test_payload_stored(void)
{
  logger_init();
  uint8_t payload[4] = {0xDE, 0xAD, 0xBE, 0xEF};
  log_event(LOG_EVT_CONFIG_APPLIED, LOG_CLASS_INFO, payload, sizeof(payload));

  log_event_t ev = {0};
  log_read_recent(&ev, 1u);
  CHECK(ev.event_id == LOG_EVT_CONFIG_APPLIED, "event ID must match");
  CHECK(ev.data[0] == 0xDE && ev.data[1] == 0xAD && ev.data[2] == 0xBE && ev.data[3] == 0xEF,
        "payload bytes must be preserved");
  printf("test_payload_stored: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 5: payload clipped at 32 bytes                                 */
/* ------------------------------------------------------------------ */

static void test_payload_clipped(void)
{
  logger_init();
  uint8_t big[40];
  memset(big, 0xAB, sizeof(big));
  big[32] = 0xFF; /* byte that must NOT appear in the log entry */
  log_event(LOG_EVT_CONFIG_APPLIED, LOG_CLASS_INFO, big, (uint8_t)sizeof(big));

  log_event_t ev = {0};
  log_read_recent(&ev, 1u);
  /* First 32 bytes must be 0xAB */
  bool all_ab = true;
  for (int i = 0; i < 32; i++)
  {
    if (ev.data[i] != 0xAB)
    {
      all_ab = false;
      break;
    }
  }
  CHECK(all_ab, "first 32 payload bytes must be 0xAB after clip");
  printf("test_payload_clipped: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 6: log_clear_info() spares Class-A and Class-B                 */
/* ------------------------------------------------------------------ */

static void test_clear_info_spares_ab(void)
{
  logger_init();                                                   /* Class-C boot event */
  log_event(LOG_EVT_SAFE_ENTRY, LOG_CLASS_CRITICAL, NULL, 0u);     /* Class A */
  log_event(LOG_EVT_MODE_CHANGE, LOG_CLASS_OPERATIONAL, NULL, 0u); /* Class B */
  log_event(LOG_EVT_CMD_CLASS_B, LOG_CLASS_INFO, NULL, 0u);        /* Class C */

  log_clear_info(); /* removes Class-C only */

  /* Read up to 4 events back */
  log_event_t buf[4] = {{0}};
  size_t n = log_read_recent(buf, 4u);

  /* Class-C entries (boot + CMD_CLASS_B) are erased; Class-A and Class-B survive */
  bool found_a = false, found_b = false, found_c = false;
  for (size_t i = 0; i < n; i++)
  {
    if (buf[i].event_id == LOG_EVT_SAFE_ENTRY)
      found_a = true;
    if (buf[i].event_id == LOG_EVT_MODE_CHANGE)
      found_b = true;
    if (buf[i].severity == (uint8_t)LOG_CLASS_INFO)
      found_c = true;
  }
  CHECK(found_a, "Class-A event must survive log_clear_info()");
  CHECK(found_b, "Class-B event must survive log_clear_info()");
  CHECK(!found_c, "Class-C events must be removed by log_clear_info()");
  printf("test_clear_info_spares_ab: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 7: log_read_recent returns events newest-first                 */
/* ------------------------------------------------------------------ */

static void test_newest_first_ordering(void)
{
  logger_init();
  log_event(LOG_EVT_MODE_CHANGE, LOG_CLASS_OPERATIONAL, NULL, 0u);   /* 2nd */
  log_event(LOG_EVT_ENERGY_CHANGE, LOG_CLASS_OPERATIONAL, NULL, 0u); /* newest */

  log_event_t buf[3] = {{0}};
  size_t n = log_read_recent(buf, 3u);
  CHECK(n == 3u, "expected 3 events (boot + mode + energy)");
  CHECK(buf[0].event_id == LOG_EVT_ENERGY_CHANGE, "buf[0] must be newest: ENERGY_CHANGE");
  CHECK(buf[1].event_id == LOG_EVT_MODE_CHANGE, "buf[1] must be MODE_CHANGE");
  CHECK(buf[2].event_id == LOG_EVT_SYSTEM_BOOT, "buf[2] must be oldest: SYSTEM_BOOT");
  printf("test_newest_first_ordering: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 8: ring wrap overwrites oldest Class-C                         */
/* ------------------------------------------------------------------ */

static void test_ring_wrap_overwrites_classc(void)
{
  logger_init(); /* 1 Class-C boot event */

  /* Fill remaining RING_CAP-1 slots with Class-C events — ring is now full */
  for (uint32_t i = 0u; i < RING_CAP - 1u; i++)
  {
    log_event(LOG_EVT_CONFIG_APPLIED, LOG_CLASS_INFO, NULL, 0u);
  }

  /* Write one distinctive event — must overwrite the oldest Class-C */
  log_event(LOG_EVT_CMD_CLASS_B, LOG_CLASS_INFO, NULL, 0u);

  log_event_t newest = {0};
  log_read_recent(&newest, 1u);
  CHECK(newest.event_id == LOG_EVT_CMD_CLASS_B, "newest event after ring wrap must be CMD_CLASS_B");
  printf("test_ring_wrap_overwrites_classc: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 9: Class-A entries protected from ring overflow                */
/* ------------------------------------------------------------------ */

static void test_class_a_protected(void)
{
  logger_init();

  /* Clear the boot event first so we start with a clean slate */
  log_clear_info();

  /* Fill the entire ring with Class-A (CRITICAL) events */
  for (uint32_t i = 0u; i < RING_CAP; i++)
  {
    log_event(LOG_EVT_SAFE_ENTRY, LOG_CLASS_CRITICAL, NULL, 0u);
  }

  /* Attempt to add a Class-B event — must be silently dropped */
  log_event(LOG_EVT_MODE_CHANGE, LOG_CLASS_OPERATIONAL, NULL, 0u);

  /* Verify no Class-B event appears in the ring */
  log_event_t buf[RING_CAP];
  size_t n = log_read_recent(buf, RING_CAP);
  bool found_b = false;
  for (size_t i = 0; i < n; i++)
  {
    if (buf[i].event_id == LOG_EVT_MODE_CHANGE)
    {
      found_b = true;
      break;
    }
  }
  CHECK(!found_b, "Class-B event must be dropped when ring holds only Class-A");
  printf("test_class_a_protected: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 10: event_id == 0 rejected                                     */
/* ------------------------------------------------------------------ */

static void test_zero_event_id_rejected(void)
{
  logger_init();                           /* 1 boot event */
  log_event(0u, LOG_CLASS_INFO, NULL, 0u); /* must be a no-op */

  log_event_t buf[2] = {{0}};
  size_t n = log_read_recent(buf, 2u);
  CHECK(n == 1u, "event_id 0 must be rejected (ring must still have only 1 entry)");
  printf("test_zero_event_id_rejected: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 11: subsystem field derived from high byte of event_id         */
/* ------------------------------------------------------------------ */

static void test_subsystem_field(void)
{
  logger_init();
  /* LOG_EVT_MODE_CHANGE = 0x0101 → high byte = 0x01 */
  log_event(LOG_EVT_MODE_CHANGE, LOG_CLASS_OPERATIONAL, NULL, 0u);

  log_event_t ev = {0};
  log_read_recent(&ev, 1u);
  CHECK(ev.subsystem == 0x01u,
        "subsystem must equal high byte of event_id (0x01 for LOG_EVT_MODE_CHANGE)");
  printf("test_subsystem_field: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 12: NULL output returns 0                                      */
/* ------------------------------------------------------------------ */

static void test_null_output_returns_zero(void)
{
  logger_init();
  size_t n = log_read_recent(NULL, 10u);
  CHECK(n == 0u, "log_read_recent(NULL, n) must return 0");
  printf("test_null_output_returns_zero: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 13: Overflow when ring full — oldest non-CRITICAL overwritten  */
/* ------------------------------------------------------------------ */

static void test_overflow_oldest_not_critical(void)
{
  logger_init(); /* 1 Class-C boot event */

  /* Clear all so ring starts empty */
  log_clear_info();

  /* Fill ring with Class-B (OPERATIONAL) events so we control the
   * class of every occupied slot.  RING_CAP entries exactly fill it. */
  for (uint32_t i = 0u; i < RING_CAP; i++)
  {
    log_event(LOG_EVT_MODE_CHANGE, LOG_CLASS_OPERATIONAL, NULL, 0u);
  }

  /* Ring is now full with all Class-B.  The oldest entry is Class-B
   * (not CRITICAL), so the overflow branch falls through to ring_push
   * which overwrites it. */
  log_event(LOG_EVT_CMD_CLASS_B, LOG_CLASS_INFO, NULL, 0u);

  /* Read back — should still return RING_CAP events (oldest overwritten) */
  log_event_t buf[RING_CAP];
  size_t n = log_read_recent(buf, RING_CAP);
  CHECK(n == RING_CAP, "overflow when oldest not CRITICAL must preserve ring count");

  /* Newest event must be the one we just added */
  CHECK(buf[0].event_id == LOG_EVT_CMD_CLASS_B,
        "newest event after overflow must be CMD_CLASS_B");

  printf("test_overflow_oldest_not_critical: OK\n");
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(void)
{
  test_init_writes_boot();
  test_store_and_retrieve();
  test_read_capped_to_stored();
  test_payload_stored();
  test_payload_clipped();
  test_clear_info_spares_ab();
  test_newest_first_ordering();
  test_ring_wrap_overwrites_classc();
  test_class_a_protected();
  test_zero_event_id_rejected();
  test_subsystem_field();
  test_null_output_returns_zero();
  test_overflow_oldest_not_critical();

  if (g_failures == 0)
  {
    printf("All Logger checks PASSED.\n");
    return 0;
  }
  printf("%d Logger check(s) FAILED.\n", g_failures);
  return 1;
}
