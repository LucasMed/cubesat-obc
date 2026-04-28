/**
 * @file pico_power.c
 * @brief Power source detection for Raspberry Pi Pico 2W.
 *
 * Detects whether the board is powered via USB (VBUS) or an external
 * power source (battery).  This is used by the EPS monitor to decide
 * whether to trust the INA219 battery readings or assume nominal
 * voltage during USB development.
 *
 * The Pico 2W exposes VBUS detection through the USB peripheral:
 *   USB_SIE_STATUS bits contain VBUS_DETECTED (bit 0)
 *
 * SDK ref: pico-sdk/src/rp2_common/hardware_usb/include/usb.h
 */
#include "pico_power.h"

#ifdef PICO_BUILD
  #include "hardware/structs/usb.h"
#endif

bool pico_power_is_usb(void)
{
#ifdef PICO_BUILD
  /* Read VBUS_DETECTED bit from USB SIE status register.
   * USB_SIE_STATUS_VBUS_DETECTED_BITS = 0x00000001
   * Bit 0 = 1 when VBUS > ~1V (USB connected) */
  return (usb_hw->sie_status & USB_SIE_STATUS_VBUS_DETECTED_BITS) != 0;
#else
  /* Host build: assume USB for development convenience */
  return true;
#endif
}
