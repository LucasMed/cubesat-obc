/**
 * @file sd_spi.h
 * @brief Low-level SD card driver over SPI.
 *
 * Implements the SD physical layer (SPI mode) for microSD cards.
 * Provides block read/write access (512-byte sectors).
 *
 * Spec ref: ICD-PAYLOAD-001 §11.1
 */

#ifndef SD_SPI_H
#define SD_SPI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** SD Card Type */
  typedef enum
  {
    SD_TYPE_UNKNOWN = 0,
    SD_TYPE_MMC = 1,
    SD_TYPE_SDV1 = 2,
    SD_TYPE_SDV2 = 4,
    SD_TYPE_SDHC = 12 /* SDv2 | High Capacity */
  } sd_type_t;

  /**
   * @brief Initialize the SD card in SPI mode.
   *
   * Performs the SPI mode entry sequence:
   * 1. Power-on pulses (74+ clocks)
   * 2. CMD0 (Reset)
   * 3. CMD8 (Check voltage)
   * 4. ACMD41 (Initialize)
   *
   * @return true if initialization succeeds.
   */
  bool sd_spi_init(void);

  /**
   * @brief Read a 512-byte sector from the SD card.
   *
   * @param sector  Address of the sector (LBA).
   * @param buffer  Destination buffer (must be at least 512 bytes).
   * @return true on success.
   */
  bool sd_spi_read_sector(uint32_t sector, uint8_t *buffer);

  /**
   * @brief Write a 512-byte sector to the SD card.
   *
   * @param sector  Address of the sector (LBA).
   * @param buffer  Source buffer (must be at least 512 bytes).
   * @return true on success.
   */
  bool sd_spi_write_sector(uint32_t sector, const uint8_t *buffer);

  /**
   * @brief Return the card type (SDHC, SDv2, etc.).
   */
  sd_type_t sd_spi_get_type(void);

#ifdef __cplusplus
}
#endif

#endif /* SD_SPI_H */
