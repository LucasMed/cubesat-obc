/**
 * @file host_temp.c
 * @brief Host-mode mock implementation of the temperature sensor for testing.
 */

#include "drivers/temperature.h"
#include <stdio.h>

int temperature_init(void) {
    printf("[Host Temp] Init onboard sensor mock\n");
    return 0;
}

float temperature_read(void) {
    // Return a constant "room temperature" for host builds
    return 25.0f;
}
