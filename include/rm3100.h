/**
 * @file rm3100.h
 * @brief PNI RM3100 Magnetometer Driver API (SPI interface).
 *
 * Communicates over the shared SPI0 payload bus (GPIO16/18/19)
 * with chip select on SPI_CS_MAG_PIN (GPIO6).
 *
 * Sensitivity: 13 nT / LSB at default cycle count of 200.
 * Spec ref: ICD-PAYLOAD-001 §11.1, §11.3
 */

#ifndef RM3100_H
#define RM3100_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** Three-axis magnetic field measurement, in nano-Tesla (nT). */
  typedef struct
  {
    float x_nT; /**< X-axis magnetic field (nT) */
    float y_nT; /**< Y-axis magnetic field (nT) */
    float z_nT; /**< Z-axis magnetic field (nT) */
  } rm3100_vector_t;

  /**
   * @brief Initialise the RM3100 and the shared SPI0 bus.
   *
   * Calls spi_payload_init() (idempotent), configures MAG_DRDY_PIN as
   * input, and verifies the sensor REVID register (expects 0x22).
   *
   * @return true if sensor responds with the expected REVID.
   */
  bool rm3100_init(void);

  /**
   * @brief Configure Continuous Measurement Mode (CMM) on all three axes.
   *
   * @param cycle_count  Coil cycle count (typical: 200 → ~37 Hz ODR).
   *                     Higher values → finer resolution, lower data rate.
   * @return true on success.
   */
  bool rm3100_config_cmm(uint16_t cycle_count);

  /**
   * @brief Read a magnetic field vector from the sensor.
   *
   * Polls MAG_DRDY_PIN for up to @p timeout_ms milliseconds, then reads
   * MX/MY/MZ registers and converts counts to nT (scale 13 nT/LSB).
   *
   * @param[out] vec         Result buffer (must not be NULL).
   * @param      timeout_ms  Max wait for data-ready; 0 = single poll only.
   * @return true if data was ready and the SPI read succeeded.
   */
  bool rm3100_read_vector(rm3100_vector_t *vec, uint32_t timeout_ms);

  /**
   * @brief Return the cached result from the last successful read.
   * @param[out] vec  Result buffer (must not be NULL).
   */
  void rm3100_get_last(rm3100_vector_t *vec);

#ifdef __cplusplus
}
#endif

#endif /* RM3100_H */
