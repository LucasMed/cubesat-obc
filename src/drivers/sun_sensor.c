/**
 * @file sun_sensor.c
 * @brief Sun Sensor Driver Implementation
 *
 * Reads analog photodiodes on ADC pins for sun detection.
 */

#include "sun_sensor.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef PICO_BUILD
  #include "hardware/adc.h"

  #include <stdio.h>
#else
  #include <stdio.h>
#endif

/* Default ADC pins for sun sensors - GPIO27 (ADC1) and GPIO28 (ADC2) */
#define SUN_SENSOR_PIN_X 27
#define SUN_SENSOR_PIN_Y 28

/* Detection threshold (raw ADC value, 0-4095) */
/* Sunlight gives ~2000-4000, ambient ~100-500 */
#define SUN_THRESHOLD 1000

static bool s_initialized = false;

bool sun_sensor_init(void)
{
  if (s_initialized)
  {
    return true;
  }

#ifdef PICO_BUILD
  adc_init();
  adc_gpio_init(SUN_SENSOR_PIN_X);
  adc_gpio_init(SUN_SENSOR_PIN_Y);
#endif

  s_initialized = true;

#ifdef PICO_BUILD
  printf("sun_sensor: Initialized on GPIO%d/X, GPIO%d/Y\r\n", SUN_SENSOR_PIN_X, SUN_SENSOR_PIN_Y);
#endif

  return true;
}

bool sun_sensor_read(sun_sensor_data_t *data)
{
  if (!s_initialized || data == NULL)
  {
    return false;
  }

#ifdef PICO_BUILD
  /* Read X sensor (ADC1) - GPIO27 maps to ADC1 */
  adc_select_input(1);
  data->adc_x = (uint16_t)adc_read();

  /* Read Y sensor (ADC2) - GPIO28 maps to ADC2 */
  adc_select_input(2);
  data->adc_y = (uint16_t)adc_read();
#else
  /* Host stub - return mock data */
  data->adc_x = 100;
  data->adc_y = 100;
#endif

  /* Normalize to 0.0-1.0 range */
  data->intensity_x = (float)data->adc_x / 4095.0f;
  data->intensity_y = (float)data->adc_y / 4095.0f;

  /* Check if sun detected (above threshold) */
  data->sun_detected_x = (data->adc_x > SUN_THRESHOLD);
  data->sun_detected_y = (data->adc_y > SUN_THRESHOLD);

#ifdef PICO_BUILD
  static uint8_t print_counter = 0;
  if (++print_counter >= 10) /* Print every 10 reads (1 second at 10 Hz) */
  {
    print_counter = 0;
    printf("[sun_sensor] X=%u (%.2f) Y=%u (%.2f)\r\n", (unsigned)data->adc_x,
           (double)data->intensity_x, (unsigned)data->adc_y, (double)data->intensity_y);
  }
#endif

  return true;
}

bool sun_sensor_is_sun_visible(uint16_t threshold)
{
#ifdef PICO_BUILD
  /* Select X sensor */
  adc_select_input(1);
  uint16_t x = (uint16_t)adc_read();

  /* Select Y sensor */
  adc_select_input(2);
  uint16_t y = (uint16_t)adc_read();

  return (x > threshold) || (y > threshold);
#else
  return false;
#endif
}
