// neo7m.c -- GPS NEO-7M/6M NMEA driver implementation
// All comments in English, see WP-7.10

#include "gps_driver.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Static variables for last fix, satellite count, status, etc.
static GpsFix_t g_last_fix = {0};
static uint8_t g_satellites_in_view = 0;

// --- API stubs (to implement) ---

bool gps_init(void)
{
  // TODO: Initialize UART0 for GPS, clear buffers, etc.
  return true;
}

void gps_deinit(void)
{
  // TODO: Release UART resources, buffers, etc.
}

GpsFix_t *gps_read_fix(void)
{
  // TODO: Read and parse next NMEA frame, update g_last_fix
  return &g_last_fix;
}

GpsFix_t *gps_get_last_fix(void)
{
  // Return pointer to last parsed valid fix
  return &g_last_fix;
}

bool gps_is_fix_valid(void)
{
  // TODO: Add stale detection, timestamp check
  return g_last_fix.valid;
}

uint8_t gps_get_satellites_in_view(void)
{
  // TODO: parse from GPGGA sentence
  return g_satellites_in_view;
}
