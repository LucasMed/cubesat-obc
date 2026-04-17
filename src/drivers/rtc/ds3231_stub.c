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

/**
 * Convert a calendar date/time to Unix epoch (seconds since 1970-01-01 00:00:00 UTC).
 * Valid range: 1970-01-01 00:00:00 to 2106-02-07 06:28:15 (uint32_t overflow).
 * Algorithm: days from epoch + seconds within day.
 */
static uint32_t days_from_epoch(uint16_t year, uint8_t month, uint8_t day)
{
  /* Days per month (non-leap year) */
  static const uint16_t cum_days[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

  /* Count complete years from 1970 to year-1 */
  uint32_t days = 0;
  for (uint16_t y = 1970; y < year; y++)
  {
    days += (((y % 4 == 0) && (y % 100 != 0)) || (y % 400 == 0)) ? 366 : 365;
  }

  /* Add days for complete months in current year */
  days += cum_days[month - 1];

  /* Add day-of-month (1-indexed → 0-indexed) */
  days += (uint32_t)day - 1;

  /* Leap day correction: if current year is leap and month > Feb, add 1 */
  bool is_leap = (((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0));
  if (is_leap && month > 2)
  {
    days += 1;
  }

  return days;
}

uint32_t ds3231_to_epoch(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute,
                         uint8_t second)
{
  uint32_t days = days_from_epoch(year, month, day);
  return (days * 86400UL) + ((uint32_t)hour * 3600UL) + ((uint32_t)minute * 60UL) +
         (uint32_t)second;
}