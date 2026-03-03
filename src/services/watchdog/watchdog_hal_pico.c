/**
 * @file watchdog_hal_pico.c
 * @brief Pico SDK hardware watchdog implementation of watchdog_hal.h.
 *
 * Uses the RP2350 hardware watchdog peripheral via the Pico SDK
 * hardware/watchdog.h API.  Selected by CMake when PICO_BUILD=ON.
 *
 * The watchdog is configured with a timeout of 3 000 ms.  The health
 * monitor task feeds it every 500 ms, so any task stall > 3 s triggers
 * a hardware reset.
 *
 * Spec ref: SYS-REQ-4 (fault-tolerant safe-mode re-entry via watchdog).
 */

#ifdef PICO_BUILD

  #include "hardware/watchdog.h"
  #include "pico/stdlib.h"
  #include "watchdog_hal.h"

  #include <stdbool.h>
  #include <stdint.h>

void watchdog_hal_init(uint32_t timeout_ms)
{
  /* Enable the hardware watchdog.  The second argument (pause_on_debug)
   * stops the countdown while a debugger is attached. */
  watchdog_enable((int)timeout_ms, /* pause_on_debug */ true);
}

void watchdog_hal_feed(void)
{
  watchdog_update();
}

bool watchdog_hal_triggered(void)
{
  return watchdog_caused_reboot();
}

#endif /* PICO_BUILD */

/* Suppress ISO C "empty translation unit" warning when not built for Pico. */
typedef int watchdog_hal_pico_unused_t;
