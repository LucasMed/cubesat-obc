/**
 * @file sun_sensor.h
 * @brief Sun Sensor Driver for photodiode-based Sun detection
 *
 * Uses analog photodiodes connected to ADC pins to detect sunlight.
 * Typical application: ADCS sun sensors for attitude determination.
 *
 * Hardware:
 *   Photodiode (e.g., BPW21, blue-enhanced silicon)
 *   Connected to ADC with 10kΩ pull-down resistor
 *
 * Pin configuration:
 *   GPIO27 (ADC1) -> Sun sensor X
 *   GPIO28 (ADC2) -> Sun sensor Y
 */

#ifndef SUN_SENSOR_H
#define SUN_SENSOR_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Sun sensor data
   */
  typedef struct
  {
    uint16_t adc_x;      /**< Raw ADC value X (0-4095) */
    uint16_t adc_y;      /**< Raw ADC value Y (0-4095) */
    bool sun_detected_x; /**< Sun detected in X direction */
    bool sun_detected_y; /**< Sun detected in Y direction */
    float intensity_x;   /**< Intensity X (0.0-1.0 normalized) */
    float intensity_y;   /**< Intensity Y (0.0-1.0 normalized) */
  } sun_sensor_data_t;

  /**
   * @brief Initialize sun sensors (uses default pins GPIO27 and GPIO28)
   *
   * @return true on success
   */
  bool sun_sensor_init(void);

  /**
   * @brief Read sun sensor data
   *
   * @param data Pointer to store sensor data
   * @return true on success
   */
  bool sun_sensor_read(sun_sensor_data_t *data);

  /**
   * @brief Check if sun is visible
   *
   * @param threshold Detection threshold (0-4095)
   * @return true if any sensor detects sun
   */
  bool sun_sensor_is_sun_visible(uint16_t threshold);

#ifdef __cplusplus
}
#endif

#endif /* SUN_SENSOR_H */