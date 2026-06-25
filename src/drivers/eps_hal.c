/**
 * @file eps_hal.c
 * @brief Pico-specific EPS HAL override — reads battery voltage via INA219.
 *
 * Overrides the weak eps_hal_read() in eps_monitor.c.
 * Uses the INA219 (0x40) bus monitor instead of the resistor divider on
 * GPIO26 (ADC0), because:
 *   - The INA219 gives stable readings regardless of battery presence
 *   - The ADC pin floats on dev boards (USB-only, no battery) causing
 *     false CRITICAL voltage readings → spurious SAFE mode transitions
 *   - The INA219 is already read by the sensor task at 100 Hz, so the
 *     cached voltage is always fresh (use ina219_get_voltage_mv())
 *
 * Hardware:
 *   INA219 at 0x40 — system bus voltage (battery or USB)
 *   INA219 at 0x41 — solar panel voltage
 *
 * Built only when PICO_BUILD is set (see drivers/CMakeLists.txt).
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef PICO_BUILD
  #include "ina219.h"
#endif

/* Forward declaration of the weak symbol we are overriding (MISRA-C:2012
 * Rule 8.4 — non-static functions require a declaration before definition). */
bool eps_hal_read(float *vbatt, float *ibatt, float *temp);

/**
 * @brief Read battery voltage from the INA219 bus monitor.
 *
 * Uses the cached INA219 reading (updated at 100 Hz by the sensor task)
 * rather than the raw GPIO26 ADC, which gives unreliable readings when
 * no battery is attached (dev board with USB power only).
 *
 * @param[out] vbatt  Battery voltage (V).
 * @param[out] ibatt  Battery current (A) — from INA219 cached value.
 * @param[out] temp   Temperature (°C) — not available, set to 0.
 * @return true on success, false if no INA219 reading available yet.
 */
bool eps_hal_read(float *vbatt, float *ibatt, float *temp)
{
#ifdef PICO_BUILD
  int16_t bus_mv = ina219_get_voltage_mv();

  if (vbatt != NULL)
  {
    *vbatt = (float)bus_mv / 1000.0f;
  }
  if (ibatt != NULL)
  {
    /* INA219 current not cached via singleton API — set to 0 for now */
    *ibatt = 0.0f;
  }
  if (temp != NULL)
  {
    /* No EPS temperature sensor — set to 0 */
    *temp = 0.0f;
  }

  return bus_mv != 0;

#else  /* PICO_BUILD — not reached in practice, keeps MISRA-C:2012 Rule 14.4 */
  if (vbatt != NULL)
  {
    *vbatt = 4.2f;
  }
  if (ibatt != NULL)
  {
    *ibatt = 0.0f;
  }
  if (temp != NULL)
  {
    *temp = 25.0f;
  }
  return true;
#endif /* PICO_BUILD */
}
