/**
 * @file sht31.h
 * @brief SHT31-D Temperature and Humidity Sensor Driver
 *
 * High-accuracy I2C temperature and humidity sensor.
 * Datasheet: https://sensirion.com/media/documents/33FD6951/66A6D720/SHT3x_Datasheet_digital.pdf
 *
 * Specifications:
 * - Temperature range: -40°C to +125°C
 * - Humidity range: 0% to 100% RH
 * - Temperature accuracy: ±0.3°C (typ)
 * - Humidity accuracy: ±2% RH (typ)
 * - I2C addresses: 0x44 (default), 0x45 (ADR = VDD)
 */

#ifndef SHT31_H
#define SHT31_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Initialize the SHT31 sensor.
   *
   * @param addr I2C address (0x44 or 0x45)
   * @return true on success
   */
  bool sht31_init(uint8_t addr);

  /**
   * @brief Read temperature and humidity.
   *
   * @param temperature Output pointer for temperature in Celsius
   * @param humidity Output pointer for relative humidity in percent
   * @return true on success
   */
  bool sht31_read(float *temperature, float *humidity);

  /**
   * @brief Check if sensor is present and responding.
   *
   * @param addr I2C address to check
   * @return true if sensor responds
   */
  bool sht31_is_present(uint8_t addr);

  /**
   * @brief Enable or disable the heater.
   *
   * @param enable true to enable heater, false to disable
   * @return true on success
   */
  bool sht31_set_heater(bool enable);

/**
 * @brief Get the default I2C address.
 */
#define SHT31_ADDR_DEFAULT 0x44

/**
 * @brief Alternate I2C address (when ADR pin is HIGH).
 */
#define SHT31_ADDR_ALTERNATE 0x45

#ifdef __cplusplus
}
#endif

#endif /* SHT31_H */
