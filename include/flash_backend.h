/**
 * @file flash_backend.h
 * @brief Flash-backed log storage interface.
 *
 * Provides a single flush function that the event logger calls when its
 * in-memory ring buffer becomes full.  On host builds the stub implementation
 * appends raw records to /tmp/obc_log.bin.  On Pico builds a real
 * implementation would call flash_range_program().
 *
 * Tests may supply a strong-symbol override to intercept flush calls.
 *
 * Spec ref: SPEC-2-PLG v1.16 §5.3
 */

#ifndef FLASH_BACKEND_H
#define FLASH_BACKEND_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Write @p len bytes from @p buf to persistent log storage.
   *
   * The caller (event_logger) guarantees that @p buf points to an array
   * of packed @ref log_event_t records and that @p len is a non-zero
   * multiple of sizeof(log_event_t).
   *
   * This function must complete synchronously with respect to its caller.
   * It is NOT ISR-safe and must NOT be called from interrupt context.
   *
   * @param buf  Pointer to the data to persist (read-only).
   * @param len  Number of bytes to write.
   */
  void flash_backend_flush(const uint8_t *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* FLASH_BACKEND_H */
