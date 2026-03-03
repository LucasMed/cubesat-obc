/**
 * @file event_logger.c
 * @brief Persistent Event Logger implementation.
 *
 * Implements the logger API declared in logger.h.  Log events are
 * accumulated in an in-memory ring buffer.  When the buffer reaches
 * capacity the entire buffer is flushed to persistent storage via
 * flash_backend_flush() and the buffer is cleared.
 *
 * On host builds flash_backend_flush() is provided by
 * flash_backend_stub.c and writes to /tmp/obc_log.bin.
 *
 * Design notes:
 *   - Buffer capacity: LOG_RING_CAPACITY (64) records × 40 bytes = 2560 B.
 *   - Flush is triggered when the buffer would otherwise overflow
 *     (capacity reached).  After the flush the buffer is reset and the
 *     triggering event is written as the first entry of the new epoch.
 *   - log_clear_info() compacts the buffer in-place by removing all
 *     LOG_CLASS_INFO records; Class A and B records are retained.
 *   - NOT ISR-safe (no locking).  All callers must be task context.
 *
 * Spec ref: SPEC-2-PLG v1.16 §5.1–5.3
 */

#include "flash_backend.h"
#include "logger.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/* Module-private state                                                */
/* ------------------------------------------------------------------ */

/** In-memory log ring buffer.  Valid entries occupy indices [0, s_count). */
static log_event_t s_ring[LOG_RING_CAPACITY];

/** Number of valid entries currently in the ring buffer. */
static uint32_t s_count;

/** Non-zero after a successful logger_init() call. */
static uint8_t s_initialized;

/** Running epoch counter used as a monotonic timestamp surrogate. */
static uint32_t s_tick;

/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */

/**
 * Flush s_ring[0..s_count-1] to flash storage and reset the buffer.
 * Always adds a LOG_EVT_LOG_OVERFLOW informational event afterwards so
 * downstream tools can detect that a flush epoch boundary occurred.
 */
static void flush_and_reset(void)
{
  if (s_count > 0u)
  {
    flash_backend_flush((const uint8_t *)s_ring, s_count * sizeof(log_event_t));
  }
  s_count = 0u;

  /* Record that the buffer wrapped */
  log_event_t overflow_ev;
  (void)memset(&overflow_ev, 0, sizeof(overflow_ev));
  overflow_ev.timestamp_ms = s_tick;
  overflow_ev.event_id = LOG_EVT_LOG_OVERFLOW;
  overflow_ev.severity = (uint8_t)LOG_CLASS_INFO;
  overflow_ev.subsystem = 0x00u;
  s_ring[s_count] = overflow_ev;
  s_count++;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

int logger_init(void)
{
  (void)memset(s_ring, 0, sizeof(s_ring));
  s_count = 0u;
  s_tick = 0u;
  s_initialized = 1u;

  /* Record boot event */
  log_event_t boot_ev;
  (void)memset(&boot_ev, 0, sizeof(boot_ev));
  boot_ev.timestamp_ms = 0u;
  boot_ev.event_id = LOG_EVT_SYSTEM_BOOT;
  boot_ev.severity = (uint8_t)LOG_CLASS_INFO;
  boot_ev.subsystem = 0x00u;

  s_ring[s_count] = boot_ev;
  s_count++;

  return 0;
}

void log_event(uint16_t event_id, log_class_t log_class, const void *data, uint8_t data_len)
{
  if (s_initialized == 0u)
  {
    return;
  }

  s_tick++;

  /* If the ring is full, flush everything to persistent storage first. */
  if (s_count >= LOG_RING_CAPACITY)
  {
    flush_and_reset();
  }

  /* Build the new record */
  log_event_t ev;
  (void)memset(&ev, 0, sizeof(ev));
  ev.timestamp_ms = s_tick;
  ev.event_id = event_id;
  ev.severity = (uint8_t)log_class;
  ev.subsystem = (uint8_t)(event_id >> 8u);

  if ((data != NULL) && (data_len > 0u))
  {
    uint8_t copy_len = (data_len > (uint8_t)sizeof(ev.data)) ? (uint8_t)sizeof(ev.data) : data_len;
    (void)memcpy(ev.data, data, (size_t)copy_len);
  }

  s_ring[s_count] = ev;
  s_count++;
}

size_t log_read_recent(log_event_t *out, size_t count)
{
  if ((out == NULL) || (s_initialized == 0u))
  {
    return 0u;
  }

  size_t available = (size_t)s_count;
  size_t n = (count < available) ? count : available;

  if (n == 0u)
  {
    return 0u;
  }

  /* Copy the n most-recent records (they are at the end of s_ring). */
  size_t start_idx = available - n;
  (void)memcpy(out, &s_ring[start_idx], n * sizeof(log_event_t));
  return n;
}

void log_clear_info(void)
{
  if (s_initialized == 0u)
  {
    return;
  }

  /* Compact in-place: keep all non-INFO entries. */
  uint32_t write_pos = 0u;
  for (uint32_t i = 0u; i < s_count; i++)
  {
    if ((log_class_t)s_ring[i].severity != LOG_CLASS_INFO)
    {
      if (write_pos != i)
      {
        s_ring[write_pos] = s_ring[i];
      }
      write_pos++;
    }
  }
  s_count = write_pos;
}
