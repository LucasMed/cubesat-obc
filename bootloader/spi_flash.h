/**
 * @file spi_flash.h
 * @brief Minimal SPI flash driver for bootloader W25Q64 access.
 *
 * Provides read-only access to the external W25Q64 flash for
 * golden image restore. Uses Pico SDK hardware_spi.
 */

#ifndef BOOTLOADER_SPI_FLASH_H
#define BOOTLOADER_SPI_FLASH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize SPI flash for reading.
 *
 * Sets up SPI0 with appropriate pins and clock divider.
 *
 * @return true if JEDEC ID matches W25Q64, false on failure.
 */
bool spi_flash_init(void);

/**
 * @brief Read data from W25Q64 at a byte offset.
 *
 * @param offset  Byte offset from flash base (0x00000000).
 * @param data    Destination buffer.
 * @param len     Number of bytes to read.
 * @return true on success, false on SPI error.
 */
bool spi_flash_read(uint32_t offset, uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* BOOTLOADER_SPI_FLASH_H */
