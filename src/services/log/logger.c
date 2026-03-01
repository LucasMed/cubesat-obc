/**
 * @file logger.c
 * @brief Persistent Event Logger implementation.
 *
 * Storage model (host / test build):
 *   A single RAM ring buffer of LOG_RING_CAP entries holds all three
 *   log classes.  On overflow the oldest entry is inspected:
 *     - Class A (CRITICAL) : protected — the new event is silently
 *       dropped and LOG_EVT_LOG_OVERFLOW is recorded in its place.
 *     - Class B / C        : oldest entry is overwritten (ring).
 *
 * log_clear_info() nullifies all Class-C (INFO) entries in the ring
 * without touching Class-A or Class-B records.
 *
 * log_read_recent() returns records newest-first, skipping nullified
 * entries.
 *
 * Pico / flash note: on a real flight build the weak symbol
 * log_hal_write() / log_hal_read() would be overridden by a driver
 * that pages records to NOR flash.  The RAM ring is the default.
 *
 * Spec ref: SPEC-2-PLG v1.16, SPEC-2 v2.0 §4.10
 */

#include "logger.h"

#include "log_event_ids.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef PICO_BUILD
  #include "FreeRTOS.h"
  #include "task.h"
  #define log_lock() taskENTER_CRITICAL()
  #define log_unlock() taskEXIT_CRITICAL()
#else
  #define log_lock() ((void)0)
  #define log_unlock() ((void)0)
#endif

/* ------------------------------------------------------------------ */
/* Tick helper                                                         */
/* ------------------------------------------------------------------ */

#ifdef PICO_BUILD
  #include "FreeRTOS.h"
  #include "task.h"
static uint32_t get_tick_ms(void)
{
  return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}
#else
static uint32_t get_tick_ms(void)
{
  static uint32_t g_tick_ms = 0u;
  return g_tick_ms++;
}
#endif

/* ------------------------------------------------------------------ */
/* Ring buffer                                                         */
/* ------------------------------------------------------------------ */

/** Total ring capacity (all classes share the same buffer). */
#define LOG_RING_CAP 320u

/**
 * @brief Internal ring-buffer slot.
 *
 * A zeroed slot (event_id == 0) means the entry is vacant or has been
 * erased by log_clear_info().
 */
typedef struct
{
  log_event_t event;
  bool valid; /**< false → slot was cleared or never written */
} log_slot_t;

static log_slot_t g_ring[LOG_RING_CAP];
static uint32_t g_head = 0u;  /**< Next write position (mod LOG_RING_CAP)  */
static uint32_t g_count = 0u; /**< Total valid + cleared entries in use     */
static bool g_initialised = false;

/* ------------------------------------------------------------------ */
/* Internal helper — write one event into the ring (caller holds lock)*/
/* ------------------------------------------------------------------ */

/**
 * If the ring is full and the slot about to be overwritten is a
 * Class-A (CRITICAL) record, the caller must not call this function.
 */
static void ring_push(const log_event_t *ev)
{
  g_ring[g_head].event = *ev;
  g_ring[g_head].valid = true;
  g_head = (g_head + 1u) % LOG_RING_CAP;
  if (g_count < LOG_RING_CAP)
  {
    g_count++;
  }
}

/**
 * @brief Return the index of the oldest entry in the ring.
 *
 * Valid only when g_count == LOG_RING_CAP (ring is full).
 */
static uint32_t oldest_idx(void)
{
  /* When the ring is full, g_head points to the oldest entry (it is
   * about to be overwritten on the next push). */
  return g_head;
}

/* ------------------------------------------------------------------ */
/* Internal helper — build an event record                            */
/* ------------------------------------------------------------------ */

static log_event_t make_event(uint16_t event_id, log_class_t log_class, const void *data,
                              uint8_t data_len)
{
  log_event_t ev = {0};
  ev.timestamp_ms = get_tick_ms();
  ev.event_id = event_id;
  ev.severity = (uint8_t)log_class;
  ev.subsystem = (uint8_t)(event_id >> 8u);

  if (data && data_len)
  {
    uint8_t n = (data_len > (uint8_t)sizeof(ev.data)) ? (uint8_t)sizeof(ev.data) : data_len;
    (void)memcpy(ev.data, data, n);
  }
  return ev;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

int logger_init(void)
{
  log_lock();
  (void)memset(g_ring, 0, sizeof(g_ring));
  g_head = 0u;
  g_count = 0u;
  g_initialised = true;
  log_unlock();

  /* Announce boot — this is a Class-C (INFO) event per spec */
  log_event(LOG_EVT_SYSTEM_BOOT, LOG_CLASS_INFO, NULL, 0u);
  return 0;
}

void log_event(uint16_t event_id, log_class_t log_class, const void *data, uint8_t data_len)
{
  if (!g_initialised || event_id == 0u)
  {
    return;
  }

  log_event_t ev = make_event(event_id, log_class, data, data_len);

  log_lock();

  if (g_count == LOG_RING_CAP)
  {
    /* Ring is full — check what we are about to overwrite */
    uint32_t oi = oldest_idx();
    if (g_ring[oi].valid && g_ring[oi].event.severity == (uint8_t)LOG_CLASS_CRITICAL)
    {
      /*
       * Cannot overwrite a Class-A record.  Record an overflow marker
       * over the current write position if it is not itself Class-A,
       * otherwise silently drop.
       */
      uint32_t write_pos = g_head; /* same as oldest when ring is full */
      if (g_ring[write_pos].event.severity != (uint8_t)LOG_CLASS_CRITICAL)
      {
        log_event_t overflow_ev = make_event(LOG_EVT_LOG_OVERFLOW, LOG_CLASS_INFO, NULL, 0u);
        g_ring[write_pos].event = overflow_ev;
        g_ring[write_pos].valid = true;
        g_head = (g_head + 1u) % LOG_RING_CAP;
      }
      log_unlock();
      return;
    }
  }

  ring_push(&ev);
  log_unlock();
}

size_t log_read_recent(log_event_t *out, size_t count)
{
  if (!out || count == 0u || !g_initialised)
  {
    return 0u;
  }

  log_lock();

  /*
   * Walk backwards from (g_head - 1) for up to g_count slots, copying
   * valid entries newest-first into out[].
   */
  size_t written = 0u;
  uint32_t n = (g_count < LOG_RING_CAP) ? g_count : LOG_RING_CAP;
  uint32_t idx = (g_head + LOG_RING_CAP - 1u) % LOG_RING_CAP;

  for (uint32_t i = 0u; i < n && written < count; i++)
  {
    if (g_ring[idx].valid)
    {
      out[written++] = g_ring[idx].event;
    }
    idx = (idx + LOG_RING_CAP - 1u) % LOG_RING_CAP;
  }

  log_unlock();
  return written;
}

void log_clear_info(void)
{
  log_lock();
  for (uint32_t i = 0u; i < LOG_RING_CAP; i++)
  {
    if (g_ring[i].valid && g_ring[i].event.severity == (uint8_t)LOG_CLASS_INFO)
    {
      (void)memset(&g_ring[i], 0, sizeof(g_ring[i]));
    }
  }
  log_unlock();
}
