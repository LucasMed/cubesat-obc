/**
 * @file watchdog_hal_host.c
 * @brief Host (no-hardware) stub implementation of watchdog_hal.h.
 *
 * All three functions are no-ops so that code depending on the watchdog HAL
 * compiles and runs correctly on the host for unit testing without requiring
 * the RP2040's watchdog peripheral.
 *
 * Selected by CMake when PICO_BUILD=OFF.
 */

#include "watchdog_hal.h"

void watchdog_hal_init(uint32_t timeout_ms)
{
  (void)timeout_ms;
  /* no-op on host */
}

void watchdog_hal_feed(void)
{
  /* no-op on host */
}

bool watchdog_hal_triggered(void)
{
  /* On hardware this queries the RP2040 chip-reset-cause register.
   * On the host there is no watchdog, so we always report false. */
  return false;
}

void watchdog_hal_clear_triggered(void)
{
  /* no-op on host */
}
