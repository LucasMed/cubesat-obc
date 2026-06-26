/**
 * @file crc32.h
 * @brief CRC-32/ISO-HDLC (PKZIP / Ethernet) computation.
 *
 * Polynomial: 0xEDB88320 (reflected)
 * Initial:    0xFFFFFFFF
 * Final XOR:  0xFFFFFFFF
 *
 * Shared implementation — single source for all CRC-32 needs across the OBC.
 * Replaces duplicated inline CRC-32 in post.c, w25q64.c, and flash_backend.c.
 */

#ifndef CRC32_H
#define CRC32_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Compute CRC-32 over a memory region (one-shot).
   *
   * @param data  Pointer to the data (may be NULL when len == 0).
   * @param len   Number of bytes.
   * @return CRC-32 checksum.
   */
  uint32_t crc32_compute(const void *data, size_t len);

  /**
   * @brief Initialise CRC-32 state.
   * @return Initial CRC state (0xFFFFFFFF).
   */
  uint32_t crc32_init(void);

  /**
   * @brief Update CRC-32 with a chunk of data.
   *
   * @param crc   Current CRC state.
   * @param data  Data chunk (must not be NULL when len > 0).
   * @param len   Chunk length.
   * @return Updated CRC state.
   */
  uint32_t crc32_update(uint32_t crc, const void *data, size_t len);

  /**
   * @brief Finalise CRC-32 computation.
   * @param crc  Final CRC state.
   * @return Completed CRC-32 value.
   */
  uint32_t crc32_finalize(uint32_t crc);

#ifdef __cplusplus
}
#endif

#endif /* CRC32_H */
