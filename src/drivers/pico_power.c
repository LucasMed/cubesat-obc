/**
 * @file pico_power.c
 * @brief Power source detection for Raspberry Pi Pico 2W.
 *
 * Note: This module was originally used by the EPS monitor to force
 * nominal voltage on USB.  VBUS detection proved unreliable on Pico 2W
 * (CYW43 keeps the VBUS detect line high when powered from VSYS).
 *
 * The EPS monitor now uses a voltage-plausibility check instead:
 * if the ADC reading is inside 1S LiPo range (2.5-4.5 V) the real
 * voltage is trusted; otherwise it forces nominal (USB-only bench).
 *
 * This file is kept for potential future use (e.g. a manual override).
 */
#include "pico_power.h"

bool pico_power_is_usb(void)
{
  /* Unreliable on Pico 2W when running from VSYS (battery → XL4005).
   * Use eps_monitor.c's voltage-plausibility approach instead. */
  return false;
}
