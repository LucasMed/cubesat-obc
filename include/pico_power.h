/**
 * @file pico_power.h
 * @brief Power source detection API for Raspberry Pi Pico 2W.
 */
#ifndef PICO_POWER_H
#define PICO_POWER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Check if the Pico is powered via USB (VBUS).
   *
   * @return true if VBUS is detected (USB cable connected),
   *         false if running from battery/external power.
   */
  bool pico_power_is_usb(void);

#ifdef __cplusplus
}
#endif

#endif /* PICO_POWER_H */
