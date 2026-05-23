#ifndef _HOST_PICO_STDLIB_H
#define _HOST_PICO_STDLIB_H

#include <stdio.h>

/* Stub: USB CDC not available on host */
static inline bool stdio_usb_connected(void)
{
  return false;
}

#endif
