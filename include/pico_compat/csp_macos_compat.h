#ifndef CSP_MACOS_COMPAT_H
#define CSP_MACOS_COMPAT_H

#include <stdint.h>

#ifndef PICO_COMPAT
  #define PICO_COMPAT 1
#endif

#ifndef PICO_RP2040
  #define PICO_RP2040 0
#endif

#ifndef PICO_RP2350
  #define PICO_RP2350 0
#endif

#ifndef PICO_ON_DEVICE
  #define PICO_ON_DEVICE 0
#endif

#ifndef PICO_NO_HARDWARE
  #define PICO_NO_HARDWARE 0
#endif

#ifndef PICO_BUILD
  #define PICO_BUILD 0
#endif

#ifndef __weak
  #define __weak __attribute__((weak))
#endif

#ifndef PICO_STATIC_ASSERT
  #define PICO_STATIC_ASSERT(x, msg) _Static_assert(x, msg)
#endif

#ifndef __noinit
  #define __noinit
#endif

#endif  // CSP_MACOS_COMPAT_H
