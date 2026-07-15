/**
 * @file watchdog_hal_pico.c
 * @brief Pico SDK hardware watchdog implementation of watchdog_hal.h.
 *
 * Uses the RP2350 hardware watchdog peripheral via the Pico SDK
 * hardware/watchdog.h API.  Selected by CMake when PICO_BUILD=ON.
 *
 * The watchdog timeout is configured by the caller (watchdog_hal_init(ms))
 * — currently 15 000 ms from obc_main.c.  The health monitor task feeds
 * it every 5 s, so any task stall > 15 s triggers a hardware reset.
 *
 * Spec ref: SYS-REQ-4 (fault-tolerant safe-mode re-entry via watchdog).
 */

#ifdef PICO_BUILD

  #include "hardware/watchdog.h"
  #include "pico/stdlib.h"
  #include "watchdog_hal.h"

  #include <stdbool.h>
  #include <stdint.h>

  /* Matches WATCHDOG_NON_REBOOT_MAGIC in pico-sdk's watchdog.c.
   * Defined locally because it is not exposed in the public header. */
  #define HAL_WDT_NON_REBOOT_MAGIC 0x6ab73121u

/* Scratch[4] value captured BEFORE watchdog_enable() overwrites it.
 *   watchdog_reboot(0,0,10) → scratch[4] = 0
 *   actual WDT timeout      → scratch[4] = WATCHDOG_NON_REBOOT_MAGIC
 * We read it in watchdog_hal_init() because watchdog_enable() writes the
 * magic into scratch[4], making the two cases indistinguishable after init. */
static uint32_t s_scratch4_at_boot = 0;
static bool s_scratch4_saved = false;

/* One-shot latch so watchdog_hal_triggered() returns true at most once
 * per boot.  Without it the health monitor would re-raise FAULT_WDT_KICK_MISSED
 * every tick (every 5 s). */
static bool s_triggered_read = false;
static bool s_triggered_result = false;

void watchdog_hal_init(uint32_t timeout_ms)
{
  /* Capture the original scratch[4] before watchdog_enable() overwrites it. */
  s_scratch4_at_boot = watchdog_hw->scratch[4];
  s_scratch4_saved = true;

  /* Enable the hardware watchdog with pause_on_debug = false.
   *
   * pause_on_debug=true is unsafe on RP2350 single-core (OI-4):
   * the PAUSE_DBG1 bit pauses the watchdog when core 1 is halted,
   * and core 1 is never released in single-core mode.  This would
   * effectively disable the watchdog on every boot.
   *
   * With pause_on_debug=false the watchdog always counts regardless
   * of debug state, which is correct for flight hardware. */
  watchdog_enable((int)timeout_ms, /* pause_on_debug */ false);
}

void watchdog_hal_feed(void)
{
  watchdog_update();
}

bool watchdog_hal_triggered(void)
{
  if (!s_scratch4_saved)
  {
    return false;
  }
  if (!s_triggered_read)
  {
    s_triggered_read = true;
    /* A watchdog-caused reboot is only a real WDT fault (not a deliberate
     * REBOOT command) when scratch[4] contains WATCHDOG_NON_REBOOT_MAGIC,
     * meaning the watchdog was enabled via watchdog_enable(), not via a
     * watchdog_reboot() call which sets scratch[4]=0. */
    s_triggered_result = watchdog_hw->reason && s_scratch4_at_boot == HAL_WDT_NON_REBOOT_MAGIC;
  }
  return s_triggered_result;
}

void watchdog_hal_clear_triggered(void)
{
  s_triggered_result = false;
}

#endif /* PICO_BUILD */

/* Suppress ISO C "empty translation unit" warning when not built for Pico. */
typedef int watchdog_hal_pico_unused_t;
