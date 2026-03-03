/**
 * @file host_temp.c
 * @brief Host-mode mock implementation of the temperature sensor for testing.
 */

#include "drivers/temperature.h"

/* cppcheck-suppress misra-c2012-21.6 -- MISRA deviation: host-only temperature
 * mock uses printf for test diagnostics; not compiled for flight targets.
 * See MISRA_DEVIATIONS.md §21.6-D5. */
#include <stdio.h>

int temperature_init(void)
{
  (void)printf("[Host Temp] Init onboard sensor mock\n");
  return 0;
}

float temperature_read(void)
{
  // Return a constant "room temperature" for host builds
  return 25.0f;
}
