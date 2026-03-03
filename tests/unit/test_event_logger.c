/**
 * @file test_event_logger.c
 * @brief Unit tests for event_logger.c (PR-26 — T-LOG-01).
 *
 * Provides a strong-symbol override of flash_backend_flush() so that
 * flash writes can be intercepted and inspected without actual I/O.
 * flash_backend_stub.c is NOT linked into this test target.
 *
 * Tests
 * -----
 *   T-LOG-01a — logger_init() inserts a LOG_EVT_SYSTEM_BOOT record
 *   T-LOG-01b — ring-buffer flush triggered when capacity is reached
 *               flushed payload size == LOG_RING_CAPACITY * sizeof(log_event_t)
 *               first flushed record is the SYSTEM_BOOT entry
 *   T-LOG-01c — log_read_recent() returns the N most-recent records
 *   T-LOG-01d — log_clear_info() removes all LOG_CLASS_INFO records
 */

#include "flash_backend.h"
#include "logger.h"

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
#define CHECK_EQ(label, actual, expected, msg)                                                     \
  do                                                                                               \
  {                                                                                                \
    if ((actual) != (expected))                                                                    \
    {                                                                                              \
      FAIL(label, msg);                                                                            \
    }                                                                                              \
    else                                                                                           \
    {                                                                                              \
      PASS(label);                                                                                 \
    }                                                                                              \
  } while (0)

static int g_failures = 0;

/* ------------------------------------------------------------------ */
/* flash_backend_flush override (strong symbol, no actual I/O)         */
/* ------------------------------------------------------------------ */

/* Storage for up to 2 flush calls (capacity = LOG_RING_CAPACITY recs) */
#define CAPTURE_BUF_BYTES ((size_t)(LOG_RING_CAPACITY * sizeof(log_event_t) * 2u))

static unsigned int s_flush_calls;
static size_t s_last_flush_len;
static uint8_t s_flushed_data[CAPTURE_BUF_BYTES];

void flash_backend_flush(const uint8_t *buf, size_t len)
{
  s_flush_calls++;
  s_last_flush_len = len;
  if (len <= sizeof(s_flushed_data))
  {
    (void)memcpy(s_flushed_data, buf, len);
  }
}

static void reset_flush_counters(void)
{
  s_flush_calls = 0u;
  s_last_flush_len = 0u;
  (void)memset(s_flushed_data, 0, sizeof(s_flushed_data));
}

/* ------------------------------------------------------------------ */
/* T-LOG-01a: logger_init() inserts SYSTEM_BOOT                        */
/* ------------------------------------------------------------------ */
static void test_init_inserts_boot_event(void)
{
  reset_flush_counters();
  (void)logger_init();

  log_event_t recent[2];
  size_t n = log_read_recent(recent, 2u);

  if (n < 1u)
  {
    FAIL("T-LOG-01a", "no events after init");
    return;
  }

  if (recent[0].event_id == LOG_EVT_SYSTEM_BOOT)
  {
    PASS("T-LOG-01a logger_init inserts SYSTEM_BOOT");
  }
  else
  {
    FAIL("T-LOG-01a", "first event is not LOG_EVT_SYSTEM_BOOT");
  }

  /* No flush should have been triggered by init alone */
  CHECK_EQ("T-LOG-01a no spurious flush on init", s_flush_calls, 0u, "flush_calls != 0 after init");
}

/* ------------------------------------------------------------------ */
/* T-LOG-01b: ring-buffer flush triggered at capacity                  */
/* ------------------------------------------------------------------ */
static void test_flush_on_capacity(void)
{
  reset_flush_counters();
  (void)logger_init();

  /*
   * After init, s_count == 1 (SYSTEM_BOOT).
   * Fill the remaining (LOG_RING_CAPACITY - 1) slots, which brings
   * s_count to exactly LOG_RING_CAPACITY.
   * Then one more write must trigger flush_and_reset().
   */
  const size_t fill_count = (size_t)(LOG_RING_CAPACITY - 1u);
  for (size_t i = 0u; i < fill_count; i++)
  {
    log_event(LOG_EVT_CMD_CLASS_B, LOG_CLASS_INFO, NULL, 0u);
  }

  /* Ring is full — no flush yet */
  if (s_flush_calls != 0u)
  {
    FAIL("T-LOG-01b", "flush triggered prematurely before ring full");
    return;
  }

  /* This write pushes over capacity and must trigger a flush */
  log_event(LOG_EVT_MODE_CHANGE, LOG_CLASS_OPERATIONAL, NULL, 0u);

  size_t expected_bytes = (size_t)(LOG_RING_CAPACITY * sizeof(log_event_t));

  CHECK_EQ("T-LOG-01b flush_calls == 1", s_flush_calls, 1u, "flush not called exactly once");

  if (s_last_flush_len == expected_bytes)
  {
    PASS("T-LOG-01b flushed payload size == LOG_RING_CAPACITY * sizeof(log_event_t)");
  }
  else
  {
    FAIL("T-LOG-01b", "flushed payload size mismatch");
    printf("       expected %zu bytes, got %zu\n", expected_bytes, s_last_flush_len);
  }

  /* First record in flushed data must be the SYSTEM_BOOT event */
  const log_event_t *first = (const log_event_t *)(const void *)s_flushed_data;
  if (first->event_id == LOG_EVT_SYSTEM_BOOT)
  {
    PASS("T-LOG-01b first flushed record is SYSTEM_BOOT");
  }
  else
  {
    FAIL("T-LOG-01b", "first flushed record is not SYSTEM_BOOT");
  }
}

/* ------------------------------------------------------------------ */
/* T-LOG-01c: log_read_recent() returns the N most-recent records      */
/* ------------------------------------------------------------------ */
static void test_read_recent(void)
{
  reset_flush_counters();
  (void)logger_init(); /* boot event → 1 entry */

  const uint16_t ids[] = {
      LOG_EVT_MODE_CHANGE,
      LOG_EVT_FAULT_ERROR,
      LOG_EVT_CMD_CLASS_C,
  };
  for (size_t i = 0u; i < 3u; i++)
  {
    log_event(ids[i], LOG_CLASS_OPERATIONAL, NULL, 0u);
  }
  /* Ring now: [BOOT, MODE_CHANGE, FAULT_ERROR, CMD_CLASS_C] */

  log_event_t out[2];
  size_t n = log_read_recent(out, 2u);

  CHECK_EQ("T-LOG-01c log_read_recent returns 2", n, 2u,
           "log_read_recent did not return 2 records");

  if (n >= 2u)
  {
    /* The two most-recent records should be FAULT_ERROR, then CMD_CLASS_C */
    if ((out[0].event_id == LOG_EVT_FAULT_ERROR) && (out[1].event_id == LOG_EVT_CMD_CLASS_C))
    {
      PASS("T-LOG-01c most-recent 2 records are in correct order");
    }
    else
    {
      FAIL("T-LOG-01c", "unexpected event IDs in log_read_recent output");
      printf("       got [0]=0x%04x [1]=0x%04x, expected 0x%04x 0x%04x\n", out[0].event_id,
             out[1].event_id, (unsigned)LOG_EVT_FAULT_ERROR, (unsigned)LOG_EVT_CMD_CLASS_C);
    }
  }
}

/* ------------------------------------------------------------------ */
/* T-LOG-01d: log_clear_info() removes all LOG_CLASS_INFO records      */
/* ------------------------------------------------------------------ */
static void test_clear_info_removes_class_c(void)
{
  reset_flush_counters();
  (void)logger_init(); /* SYSTEM_BOOT (Class C / INFO) */

  /* Add one Class-A (Critical) and two Class-C (Info) events */
  log_event(LOG_EVT_FAULT_CRITICAL, LOG_CLASS_CRITICAL, NULL, 0u);
  log_event(LOG_EVT_CMD_CLASS_B, LOG_CLASS_INFO, NULL, 0u);
  log_event(LOG_EVT_CONFIG_APPLIED, LOG_CLASS_INFO, NULL, 0u);
  /* Ring: [BOOT(C), FAULT_CRITICAL(A), CMD_B(C), CONFIG(C)] → 4 entries */

  log_clear_info();
  /* After clear: only FAULT_CRITICAL should remain */

  log_event_t out[4];
  size_t n = log_read_recent(out, 4u);

  CHECK_EQ("T-LOG-01d only Class-A records remain after clear_info", n, 1u,
           "expected exactly 1 record after log_clear_info");

  if (n >= 1u)
  {
    if (out[0].event_id == LOG_EVT_FAULT_CRITICAL)
    {
      PASS("T-LOG-01d remaining record is LOG_EVT_FAULT_CRITICAL");
    }
    else
    {
      FAIL("T-LOG-01d", "remaining record has unexpected event_id");
      printf("       got 0x%04x, expected 0x%04x\n", out[0].event_id,
             (unsigned)LOG_EVT_FAULT_CRITICAL);
    }
  }
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */
int main(void)
{
  printf("=== Event logger unit tests (PR-26, T-LOG-01) ===\n");

  test_init_inserts_boot_event();
  test_flush_on_capacity();
  test_read_recent();
  test_clear_info_removes_class_c();

  if (g_failures == 0)
  {
    printf("ALL T-LOG-01 TESTS PASSED\n");
    return 0;
  }
  else
  {
    printf("SOME TESTS FAILED (%d failure(s))\n", g_failures);
    return 1;
  }
}
