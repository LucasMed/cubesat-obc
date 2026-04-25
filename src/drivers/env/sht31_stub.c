/**
 * @file sht31_stub.c
 * @brief Host stub for SHT31 sensor driver
 */

#include "sht31.h"

#include <stdbool.h>
#include <stddef.h>

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

bool sht31_fetch(float *temperature, float *humidity)
{
  (void)temperature;
  (void)humidity;
  return false;
}
