/**
 * @file ds3231_stub.c
 * @brief DS3231 Real Time Clock Driver Stub (Host/Testing)
 *
 * Stub implementation that returns mock data for testing on host.
 */

#include "ds3231.h"

#include <stdbool.h>
#include <stdint.h>

bool ds3231_init(void)
{
  return true;
}

bool ds3231_is_present(void)
{
  return true;
}

bool ds3231_read_time(uint16_t *year, uint8_t *month, uint8_t *day, uint8_t *hour, uint8_t *minute,
                      uint8_t *second)
{
  /* Validate all pointers are non-null */
  if (year == 0 || month == 0 || day == 0 || hour == 0 || minute == 0 || second == 0)
  {
    return false;
  }

  /* Return mock time: 2024-01-01 12:00:00 */
  *year = 2024;
  *month = 1;
  *day = 1;
  *hour = 12;
  *minute = 0;
  *second = 0;

  return true;
}

bool ds3231_set_time(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute,
                     uint8_t second)
{
  /* Validate ranges (same as real driver) */
  if (year < 2000 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 ||
      minute > 59 || second > 59)
  {
    return false;
  }

  /* No-op on host - store would be needed for persistence */
  return true;
}

uint32_t ds3231_to_epoch(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute,
                         uint8_t second)
{
  /* Return a fixed timestamp: 2024-01-01 12:00:00 UTC = 1704100800 */
  (void)year;
  (void)month;
  (void)day;
  (void)hour;
  (void)minute;
  (void)second;

  return 1704100800UL;
}