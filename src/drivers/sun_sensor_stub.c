/**
 * @file sun_sensor_stub.c
 * @brief Sun Sensor Stub for host testing
 *
 * Configurable globals (declared extern in tests) let host tests inject
 * controlled return values for sun_sensor_read() and sun_sensor_is_sun_visible()
 * without redefining the stub.
 */

#include "sun_sensor.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Configurable state — tests set these via extern declarations        */
/* ------------------------------------------------------------------ */

/** @brief Return value from sun_sensor_read() (default: true). */
bool s_sun_sensor_read_ret = true;

/** @brief Raw ADC value for X axis (default: 100). */
uint16_t s_sun_sensor_adc_x = 100;

/** @brief Raw ADC value for Y axis (default: 100). */
uint16_t s_sun_sensor_adc_y = 100;

/** @brief Normalised intensity X (default: 0.024f). */
float s_sun_sensor_intensity_x = 0.024f;

/** @brief Normalised intensity Y (default: 0.024f). */
float s_sun_sensor_intensity_y = 0.024f;

/** @brief Sun detected in X direction (default: false). */
bool s_sun_sensor_sun_detected_x = false;

/** @brief Sun detected in Y direction (default: false). */
bool s_sun_sensor_sun_detected_y = false;

/** @brief Return value from sun_sensor_is_sun_visible() (default: false). */
bool s_sun_sensor_visible = false;

bool sun_sensor_init(void)
{
  return true;
}

bool sun_sensor_read(sun_sensor_data_t *data)
{
  if (data == 0)
  {
    return false;
  }
  data->adc_x = s_sun_sensor_adc_x;
  data->adc_y = s_sun_sensor_adc_y;
  data->intensity_x = s_sun_sensor_intensity_x;
  data->intensity_y = s_sun_sensor_intensity_y;
  data->sun_detected_x = s_sun_sensor_sun_detected_x;
  data->sun_detected_y = s_sun_sensor_sun_detected_y;
  return s_sun_sensor_read_ret;
}

bool sun_sensor_is_sun_visible(uint16_t threshold)
{
  (void)threshold;
  return s_sun_sensor_visible;
}