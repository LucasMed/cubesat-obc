/**
 * @file logger.h
 * @brief Persistent Event Logger API.
 *
 * Provides the three-class event logging system described in
 * SPEC-2-PLG v1.16.  Log events are written to a ring buffer in flash
 * (Class C) or to a dedicated critical log region (Class A/B).
 *
 * Each @ref log_event_t record is exactly 40 bytes to allow dense
 * packing in 64-byte NOR flash pages.
 *
 * Spec ref: SPEC-2-PLG v1.16, SPEC-2 v2.0 §4.10
 */

#ifndef LOGGER_H
#define LOGGER_H

#include "log_event_ids.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /* ------------------------------------------------------------------ */
  /* Log classification                                                  */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Log event class (maps to storage priority).
   *
   * LOG_CLASS_CRITICAL    — stored in protected critical log segment.
   * LOG_CLASS_OPERATIONAL — stored in operational log segment.
   * LOG_CLASS_INFO        — stored in circular buffer; may be overwritten.
   */
  typedef enum
  {
    LOG_CLASS_CRITICAL = 0,    /**< Class A — never overwritten          */
    LOG_CLASS_OPERATIONAL = 1, /**< Class B — overwritten only when full */
    LOG_CLASS_INFO = 2         /**< Class C — circular, oldest overwritten*/
  } log_class_t;

  /* ------------------------------------------------------------------ */
  /* Log event record (40 bytes)                                         */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Single log event record.
   *
   * Fixed size: 4 + 2 + 1 + 1 + 32 = 40 bytes.
   * @note All multi-byte fields are little-endian (RP2350 native order).
   */
  typedef struct
  {
    uint32_t timestamp_ms; /**< System tick in ms at time of logging       */
    uint16_t event_id;     /**< Event identifier (see log_event_ids.h)     */
    uint8_t severity;      /**< @ref log_class_t cast to uint8             */
    uint8_t subsystem;     /**< Originating subsystem code (upper byte of  *
                            *   the event_id's subsystem, or 0 if global)  */
    uint8_t data[32];      /**< Subsystem-specific payload                 */
  } log_event_t;

  /* Compile-time size check — will produce a build error if the struct  */
  /* layout changes and is no longer exactly 40 bytes.                   */
  typedef char log_event_size_check_[(sizeof(log_event_t) == 40u) ? 1 : -1];

  /* ------------------------------------------------------------------ */
  /* Logger configuration constants                                     */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Maximum log records held in the in-memory ring buffer.
   *
   * When the buffer reaches this capacity log_event() automatically
   * calls flash_backend_flush() and resets the buffer before writing
   * the new record.
   */
#define LOG_RING_CAPACITY 64u

  /* ------------------------------------------------------------------ */
  /* Logger API                                                          */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Initialise the persistent event logger.
   *
   * Mounts the flash log storage and validates existing log headers.
   * Logs LOG_EVT_SYSTEM_BOOT on success.
   * Must be called once during system_init(), before any task is created.
   *
   * @return 0 on success, negative errno on failure.
   */
  int logger_init(void);

  /**
   * @brief Record an event in the persistent log.
   *
   * Thread-safe.  May be called from any task (but NOT from an ISR —
   * use a deferred handler for ISR-originated events).
   *
   * @param event_id   Event identifier from log_event_ids.h.
   * @param log_class  Log class (determines storage segment).
   * @param data       Pointer to optional payload (may be NULL).
   * @param data_len   Number of bytes in @p data (max 32; excess is clipped).
   */
  void log_event(uint16_t event_id, log_class_t log_class, const void *data, uint8_t data_len);

  /**
   * @brief Read the most recent @p count log events into a caller buffer.
   *
   * @param out    Destination array of @ref log_event_t records.
   * @param count  Maximum number of records to copy into @p out.
   * @return       Actual number of records copied (≤ count).
   */
  size_t log_read_recent(log_event_t *out, size_t count);

  /**
   * @brief Erase only the Class-C (informational) circular buffer.
   *
   * Does not erase Class-A or Class-B records.
   */
  void log_clear_info(void);

#ifdef __cplusplus
}
#endif

#endif /* LOGGER_H */
