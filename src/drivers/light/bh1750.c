/**
 * @file bh1750.c
 * @brief BH1750 Digital Light Sensor Driver Implementation
 *
 * Uses One-time H-Res-2 mode (0x20): 0.5 lux resolution, 16 ms measurement.
 * Conforms to the project's C coding standards.
 */

#include "bh1750.h"

#include "drivers/i2c_interface.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef PICO_BUILD
  #include "pico/time.h"
#endif

/* Measurement time for H-Res-2 mode [ms] */
#define BH1750_MEASUREMENT_TIME_MS 16

/* Conversion factor: lux = raw / 1.2 */
#define BH1750_CONV_FACTOR 1.2f

static uint8_t s_bh1750_addr = BH1750_ADDR_DEFAULT;

bool bh1750_init(uint8_t addr)
{
  s_bh1750_addr = addr;

  /* Power on */
  uint8_t cmd = BH1750_CMD_POWER_ON;
  if (i2c_bus_write(s_bh1750_addr, &cmd, 1) < 0)
  {
#ifdef PICO_BUILD
    printf("bh1750: failed to power on\n");
#endif
    return false;
  }

  /* Reset data register */
  cmd = BH1750_CMD_RESET;
  if (i2c_bus_write(s_bh1750_addr, &cmd, 1) < 0)
  {
#ifdef PICO_BUILD
    printf("bh1750: failed to reset\n");
#endif
    return false;
  }

  /* Wait for sensor to be ready after power-on/reset (first measurement is slower) */
#ifdef PICO_BUILD
  sleep_ms(120);
#else
  /* Host: no delay needed */
#endif

  /* Verify sensor is present */
  if (!bh1750_is_present(s_bh1750_addr))
  {
#ifdef PICO_BUILD
    printf("bh1750: not detected at 0x%02X\n", s_bh1750_addr);
#endif
    return false;
  }

#ifdef PICO_BUILD
  printf("bh1750: Initialized at 0x%02X\n", s_bh1750_addr);
#endif

  return true;
}

bool bh1750_is_present(uint8_t addr)
{
  /* Trigger a measurement using write, then read */
  uint8_t cmd = BH1750_CMD_OT_H_RES2;
  uint8_t data[2];

  if (i2c_bus_write(addr, &cmd, 1) < 0)
  {
    return false;
  }

#ifdef PICO_BUILD
  sleep_ms(BH1750_MEASUREMENT_TIME_MS);
#endif

  if (i2c_bus_read(addr, data, 2) < 0)
  {
    return false;
  }

  /* Plausibility check */
  uint16_t raw = ((uint16_t)data[0] << 8) | data[1];
  if (raw == 0 || raw == 0xFFFF)
  {
    return false;
  }

  return true;
}

bool bh1750_read(float *lux)
{
  if (lux == NULL)
  {
    return false;
  }

  /* Send one-time H-Res-2 measurement command */
  uint8_t cmd = BH1750_CMD_OT_H_RES2;
  if (i2c_bus_write(s_bh1750_addr, &cmd, 1) < 0)
  {
    return false;
  }

#ifdef PICO_BUILD
  sleep_ms(BH1750_MEASUREMENT_TIME_MS);
#endif

  /* Read 2 bytes (MSB first) */
  uint8_t data[2];
  if (i2c_bus_read(s_bh1750_addr, data, 2) < 0)
  {
    return false;
  }

  /* Parse raw value */
  uint16_t raw = ((uint16_t)data[0] << 8) | data[1];

  /* Plausibility check */
  if (raw == 0 || raw == 0xFFFF)
  {
    return false;
  }

  /* Convert to Lux: lux = raw / 1.2 */
  *lux = (float)raw / BH1750_CONV_FACTOR;

  return true;
}
