/**
 * @file spi_payload.h
 * @brief Shared SPI0 payload bus initialisation helper.
 *
 * Provides a single-initialisation guard for the SPI0 bus used by the
 * RM3100 magnetometer, OV2640 camera, and microSD card.  Any payload
 * driver may call spi_payload_init() safely; subsequent calls are no-ops.
 *
 * Pin assignments (from config/pico_pins.h):
 *   MISO — GPIO16   SCK — GPIO18   MOSI — GPIO19
 *
 * Spec ref: ICD-PAYLOAD-001 §4.1, §11.1
 */

#ifndef SPI_PAYLOAD_H
#define SPI_PAYLOAD_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Initialise the SPI0 shared payload bus (idempotent).
   *
   * Configures spi0 at SPI0_BAUD_RATE_INIT (1 MHz), sets SPI0_MISO_PIN,
   * SPI0_SCK_PIN, and SPI0_MOSI_PIN to SPI function, and pulls all chip
   * select GPIOs (SPI_CS_MAG_PIN, SPI_CS_FLASH_PIN, SPI_CS_CAM_PIN)
   * high (inactive).
   *
   * Safe to call from multiple drivers: only the first call performs
   * hardware initialisation; subsequent calls return immediately.
   *
   * @return true on first (real) init, false if already initialised.
   */
  bool spi_payload_init(void);

  /**
   * @brief Assert (pull low) a payload chip-select GPIO.
   * @param cs_pin  GPIO number of the target chip select.
   */
  void spi_payload_cs_select(uint32_t cs_pin);

  /**
   * @brief Deassert (pull high) a payload chip-select GPIO.
   * @param cs_pin  GPIO number of the target chip select.
   */
  void spi_payload_cs_deselect(uint32_t cs_pin);

#ifdef __cplusplus
}
#endif

#endif /* SPI_PAYLOAD_H */
