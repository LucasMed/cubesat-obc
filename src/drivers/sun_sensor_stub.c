/**
 * @file sun_sensor_stub.c
 * @brief Sun Sensor Stub for host testing
 */

#include "sun_sensor.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
  /* Mock data - return low values */
  data->adc_x = 100;
  data->adc_y = 100;
  data->intensity_x = 0.024f;
  data->intensity_y = 0.024f;
  data->sun_detected_x = false;
  data->sun_detected_y = false;
  return true;
}

bool sun_sensor_is_sun_visible(uint16_t threshold)
{
  return false;
}