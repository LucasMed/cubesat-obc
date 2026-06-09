/**
 * @file sht31_stub.c
 * @brief Host stub for SHT31 sensor driver
 *
 * Configurable globals (declared extern in tests) let host tests inject
 * controlled return values for sht31_fetch() without redefining the stub.
 */

#include "sht31.h"

#include <stdbool.h>
#include <stddef.h>

/* ------------------------------------------------------------------ */
/* Configurable state — tests set these via extern declarations        */
/* ------------------------------------------------------------------ */

/** @brief Return value from sht31_fetch() (default: false). */
bool s_sht31_fetch_ret = false;

/** @brief Temperature value written through sht31_fetch() output param. */
float s_sht31_fetch_temp = 0.0f;

/** @brief Humidity value written through sht31_fetch() output param. */
float s_sht31_fetch_humid = 0.0f;

bool sht31_init(uint8_t addr)
{
  (void)addr;
  return true;
}

// NOLINTNEXTLINE(readability-non-const-parameter)
bool sht31_read(float *temperature, float *humidity)
{
  /* Return false by default - tests can override this stub if needed */
  (void)temperature;
  (void)humidity;
  return false;
}

bool sht31_is_present(uint8_t addr)
{
  (void)addr;
  return true;
}

bool sht31_set_heater(bool enable)
{
  (void)enable;
  return true;
}

bool sht31_start_periodic(uint8_t hz)
{
  (void)hz;
  return true;
}

// NOLINTNEXTLINE(readability-non-const-parameter)
bool sht31_fetch(float *temperature, float *humidity)
{
  if (temperature)
  {
    *temperature = s_sht31_fetch_temp;
  }
  if (humidity)
  {
    *humidity = s_sht31_fetch_humid;
  }
  return s_sht31_fetch_ret;
}
