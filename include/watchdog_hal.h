/**
 * @file watchdog_hal.h
 * @brief Hardware Abstraction Layer for the system watchdog timer.
 *
 * Provides a thin, platform-independent interface for watchdog management:
 *
 *   - watchdog_hal_init()      Configure and start the watchdog.
 *   - watchdog_hal_feed()      Reset the watchdog countdown ("kick").
 *   - watchdog_hal_triggered() Query whether the last reset was watchdog-
 *                              induced (useful for health diagnostics).
 *
 * Platform implementations:
 *   Host / unit-test  : src/services/watchdog/watchdog_hal_host.c  (no-ops)
 *   RP2040 / Pico     : src/services/watchdog/watchdog_hal_pico.c  (hardware)
 *
 * The host stub is selected by CMake when PICO_BUILD is OFF, so all unit
 * tests that include this header compile and run without hardware.
 */

#ifndef WATCHDOG_HAL_H
#define WATCHDOG_HAL_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initialise the watchdog hardware and start the countdown.
 *
 * Must be called once during system start-up, before the FreeRTOS scheduler
 * is launched.  On Pico, configures the RP2040 watchdog peripheral with the
 * given timeout.  On the host stub this is a no-op.
 *
 * @param timeout_ms  Watchdog timeout in milliseconds.  If watchdog_hal_feed()
 *                    is not called within this window the system resets.
 *                    Ignored on host stub.
 */
void watchdog_hal_init(uint32_t timeout_ms);

/**
 * @brief Feed (kick) the watchdog, resetting its countdown.
 *
 * Must be called at least once per timeout window to prevent a reset.
 * Called by vHealthMonitorTask_Step() on every health-monitor tick.
 * On the host stub this is a no-op.
 */
void watchdog_hal_feed(void);

/**
 * @brief Query whether the previous system reset was caused by the watchdog.
 *
 * Useful for health diagnostics: if this returns true the software can log
 * or raise a fault indicating that a task starved the watchdog.
 *
 * @return true   Last reset was a watchdog reset (RP2040).
 * @return false  Last reset was a clean power-on, or always false on host.
 */
bool watchdog_hal_triggered(void);

/**
 * @brief Clear the watchdog-triggered flag.
 *
 * Call after watchdog_hal_triggered() to prevent re-triggering on
 * subsequent health-monitor ticks.  On Pico this clears the hardware
 * reason register; on host it clears an internal flag.
 */
void watchdog_hal_clear_triggered(void);

#endif /* WATCHDOG_HAL_H */
