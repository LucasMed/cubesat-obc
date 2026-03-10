/**
 * @file flash_backend.h
 * @brief Flash-backed log storage interface.
 *
 * Provides a single flush function that the event logger calls when its
 * in-memory ring buffer becomes full.  On host builds the stub implementation
 * appends raw records to /tmp/obc_log.bin.  On Pico builds the real
 * implementation calls flash_range_erase() + flash_range_program() with
 * interrupt protection, writing to the top 16 KB of the 2 MB flash device.
 *
 * Tests may supply a strong-symbol override to intercept flush calls.
 *
 * Spec ref: SYS-F-304 (SyRS-OBC-001); SRR-OBC-001 ACT-16
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
   * On Pico builds: disables interrupts, erases a 4 KB flash sector, and
   * programs the header + payload.  Sectors are used round-robin.
   * On host builds: appends raw bytes to /tmp/obc_log.bin.
   *
   * @param buf  Pointer to the data to persist (read-only).
   * @param len  Number of bytes to write.
   */
  void flash_backend_flush(const uint8_t *buf, size_t len);

#ifdef PICO_BUILD
  /**
   * @brief Scan flash log sectors on boot and replay valid records.
   *
   * For each sector with a valid magic + CRC32 header, invokes @p cb
   * with a pointer to the raw payload and its length.  The callback
   * may call log_event() to replay Class-A events into the live ring.
   *
   * Only available on Pico builds (reads XIP window directly).
   *
   * @param cb  Recovery callback — must not be NULL.
   */
  void flash_backend_recover(void (*cb)(const uint8_t *payload, uint32_t len));
#endif

#ifdef __cplusplus
}
#endif

#endif /* FLASH_BACKEND_H */
