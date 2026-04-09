/**
 * @file bh1750_stub.c
 * @brief Host stub for BH1750 sensor driver
 *
 * Used for building on Linux/macOS without hardware.
 * Returns success on init/presence, returns false on read (tests can override).
 */

#include "bh1750.h"

#include <stdbool.h>
#include <stddef.h>

bool bh1750_init(uint8_t addr)
{
  (void)addr;
  return true;
}

bool bh1750_is_present(uint8_t addr)
{
  (void)addr;
  return true;
}

bool bh1750_read(float *lux)
{
  /* Return false by default — tests can override if needed */
  (void)lux;
  return false;
}
