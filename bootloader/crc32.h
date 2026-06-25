/**
 * @file crc32.h
 * @brief CRC32 computation for image integrity validation.
 *
 * Standard CRC-32 (PKZIP, Ethernet): polynomial 0xEDB88320.
 * Used to validate firmware slot images and golden image copies.
 */

#ifndef BOOTLOADER_CRC32_H
#define BOOTLOADER_CRC32_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Compute CRC32 over a memory region.
 *
 * Uses the standard CRC-32 algorithm (polynomial 0xEDB88320).
 *
 * @param data  Pointer to the data buffer.
 * @param len   Length of the data in bytes.
 * @return CRC32 checksum.
 */
uint32_t crc32_compute(const uint8_t *data, size_t len);

/**
 * @brief Compute CRC32 incrementally (for streaming / chunked data).
 *
 * Call crc32_init() once, then crc32_update() for each chunk,
 * then crc32_finalize() to get the final value.
 *
 * @return Initial CRC state.
 */
uint32_t crc32_init(void);

/**
 * @brief Update CRC32 with a chunk of data.
 *
 * @param crc  Current CRC state.
 * @param data Data chunk.
 * @param len  Chunk length.
 * @return Updated CRC state.
 */
uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len);

/**
 * @brief Finalize CRC32 computation.
 *
 * @param crc  Final CRC state.
 * @return Completed CRC32 value.
 */
uint32_t crc32_finalize(uint32_t crc);

#ifdef __cplusplus
}
#endif

#endif /* BOOTLOADER_CRC32_H */
