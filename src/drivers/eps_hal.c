/**
 * @file eps_hal.c
 * @brief Pico-specific EPS HAL override — reads battery voltage via ADC.
 *
 * Overrides the weak eps_hal_read() in eps_monitor.c.
 * Measures VBATT via resistor divider on GPIO26 (ADC0).
 *
 * Resistor divider (field-calibrated):
 *   R1 = 235 kΩ  (two 470 kΩ in parallel)
 *   R2 =  98.8 kΩ (measured)
 *   Factor = 3.67  (V_batt / V_ADC from simultaneous DMM reading)
 *
 * Built only when PICO_BUILD is set (see drivers/CMakeLists.txt).
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef PICO_BUILD
  #include "hardware/adc.h"
  #include "pico_pins.h"
#endif

/* Forward declaration of the weak symbol we are overriding (MISRA-C:2012
 * Rule 8.4 — non-static functions require a declaration before definition). */
bool eps_hal_read(float *vbatt, float *ibatt, float *temp);

/**
 * @brief Read battery voltage from ADC0 (GPIO26) via resistor divider.
 *
 * Overrides the weak stub in eps_monitor.c. Uses the Pico SDK ADC driver
 * to read the raw 12-bit value and applies the calibrated divider factor.
 *
 * @param[out] vbatt  Battery voltage (V).
 * @param[out] ibatt  Battery current (A) — not implemented, set to 0.
 * @param[out] temp   Temperature (°C) — not implemented, set to 0.
 * @return true always (ADC read is synchronous and non-failing).
 */
bool eps_hal_read(float *vbatt, float *ibatt, float *temp)
{
#ifdef PICO_BUILD
  /* ── field-calibrated divider factor ──────────────────────────────
   * Measured 2026-05-19:
   *   V_batt = 3.74 V  (DMM across battery terminals)
   *   V_ADC  = 1.02 V  (DMM across R2, GPIO26 disconnected)
   *   Factor = 3.74 / 1.02 = 3.67
   *
   * Hardware:
   *   R1 = 235 kΩ (two 470 kΩ ±5 % in parallel)
   *   R2 =  98.8 kΩ (measured)
   *   Expected factor = (235 + 98.8) / 98.8 ≈ 3.38
   *   Difference due to resistor tolerance stack + DMM loading in kΩ range.
   *   Calibrated factor is authoritative.                              */
  static const float EPS_DIVIDER_FACTOR = 3.67f;

  /* Configure GPIO26 for ADC — this disables the digital pull-down
   * that was loading the divider when the pin was in GPIO input mode. */
  adc_gpio_init(ADC_VBATT_PIN);
  adc_select_input(0); /* ADC0 = GPIO26 */

  /* RP2350: 12-bit successive-approximation ADC, 0-3.3 V range */
  const uint16_t raw = adc_read();
  const float v_adc = (float)raw * 3.3f / 4095.0f;

  if (vbatt != NULL)
  {
    *vbatt = v_adc * EPS_DIVIDER_FACTOR;
  }
  if (ibatt != NULL)
  {
    /* No INA219 current sensor on this board revision — set to 0 */
    *ibatt = 0.0f;
  }
  if (temp != NULL)
  {
    /* No EPS temperature sensor — set to 0 */
    *temp = 0.0f;
  }

  return true;

#else  /* PICO_BUILD */
  /* Fallback stub (should not be reached — this file is only compiled
   * under PICO_BUILD, but keeps MISRA-C:2012 Rule 14.4 satisfied). */
  if (vbatt != NULL)
  {
    *vbatt = 7.6f;
  }
  if (ibatt != NULL)
  {
    *ibatt = 0.5f;
  }
  if (temp != NULL)
  {
    *temp = 25.0f;
  }
  return true;
#endif /* PICO_BUILD */
}
