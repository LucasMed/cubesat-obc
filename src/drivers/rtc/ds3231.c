/**
 * @file ds3231.c
 * @brief DS3231 Real Time Clock Driver Implementation
 *
 * Uses I2C interface for communication.
 * Conforms to the project's C coding standards.
 */

#include "ds3231.h"

#include "drivers/i2c_interface.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef PICO_BUILD
  #include "pico/time.h"
#endif

/* BCD to binary conversion */
#define BCD_TO_BIN(bcd) ((((bcd) >> 4) * 10) + ((bcd) & 0x0F))

/* Binary to BCD conversion */
#define BIN_TO_BCD(bin) ((((bin) / 10) << 4) | ((bin) % 10))

/* Days in each month (non-leap year) */
static const uint8_t s_days_in_month[12] = {31, 28, 31, 30, 31, 30,
                                            31, 31, 30, 31, 30, 31};

static bool is_leap_year(uint16_t year)
{
  return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

bool ds3231_init(void)
{
  if (!ds3231_is_present())
  {
#ifdef PICO_BUILD
    printf("ds3231: not found\r\n");
#endif
    return false;
  }

#ifdef PICO_BUILD
  printf("ds3231: Initialized at 0x%02X\r\n", DS3231_ADDR);
#endif

  return true;
}

bool ds3231_is_present(void)
{
  uint8_t seconds;

  /* Try to read the seconds register */
  if (i2c_bus_read(DS3231_ADDR, &seconds, 1) < 0)
  {
    return false;
  }

  /* Validate BCD range: seconds should be 0-59 */
  uint8_t bin_sec = BCD_TO_BIN(seconds);
  if (bin_sec > 59)
  {
    return false;
  }

  return true;
}

bool ds3231_read_time(uint16_t *year, uint8_t *month, uint8_t *day,
                      uint8_t *hour, uint8_t *minute, uint8_t *second)
{
  if (year == NULL || month == NULL || day == NULL ||
      hour == NULL || minute == NULL || second == NULL)
  {
    return false;
  }

  /* Read 7 timekeeping registers (0x00-0x06) */
  uint8_t data[7];
  if (i2c_bus_read(DS3231_ADDR, data, 7) < 0)
  {
    return false;
  }

  /* Parse time data (BCD format) */
  *second = BCD_TO_BIN(data[0]);
  *minute = BCD_TO_BIN(data[1]);
  *hour   = BCD_TO_BIN(data[2] & 0x3F);  /* Mask 24-hour bit */
  /* data[3] = day of week (1-7), not used */
  *day    = BCD_TO_BIN(data[4]);

  /* Month register: bit 7 = century, bits 0-4 = month */
  *month = BCD_TO_BIN(data[5] & 0x1F);

  *year = 2000 + BCD_TO_BIN(data[6]);

  /* Plausibility checks */
  if (*second > 59 || *minute > 59 || *hour > 23 ||
      *month > 12 || *month < 1 || *day < 1)
  {
    return false;
  }

  /* Day validation */
  uint8_t max_day = s_days_in_month[*month - 1];
  if (*month == 2 && is_leap_year(*year))
  {
    max_day = 29;
  }
  if (*day > max_day)
  {
    return false;
  }

  return true;
}

bool ds3231_set_time(uint16_t year, uint8_t month, uint8_t day,
                     uint8_t hour, uint8_t minute, uint8_t second)
{
  /* Validate ranges */
  if (year < 2000 || year > 2099 || month < 1 || month > 12 ||
      day < 1 || day > 31 || hour > 23 || minute > 59 || second > 59)
  {
    return false;
  }

  /* Day validation for specific month */
  uint8_t max_day = s_days_in_month[month - 1];
  if (month == 2 && is_leap_year(year))
  {
    max_day = 29;
  }
  if (day > max_day)
  {
    return false;
  }

  /* Prepare data in BCD format */
  uint8_t data[7];
  data[0] = BIN_TO_BCD(second);
  data[1] = BIN_TO_BCD(minute);
  data[2] = BIN_TO_BCD(hour);  /* 24-hour mode */
  data[3] = 1;  /* Day of week: Sunday = 1 */
  data[4] = BIN_TO_BCD(day);
  data[5] = BIN_TO_BCD(month);  /* Century bit = 0 */
  data[6] = BIN_TO_BCD(year - 2000);

  /* Write to timekeeping registers */
  if (i2c_bus_write(DS3231_ADDR, data, 7) < 0)
  {
    return false;
  }

  return true;
}

/* Days since epoch for each month (non-leap year) */
static const uint16_t s_days_since_epoch[12] = {
  0,    /* January */
  31,   /* February */
  59,   /* March */
  90,   /* April */
  120,  /* May */
  151,  /* June */
  181,  /* July */
  212,  /* August */
  243,  /* September */
  273,  /* October */
  304,  /* November */
  334   /* December */
};

uint32_t ds3231_to_epoch(uint16_t year, uint8_t month, uint8_t day,
                          uint8_t hour, uint8_t minute, uint8_t second)
{
  if (year < 1970)
  {
    return 0;
  }

  /* Calculate days since 1970-01-01 */
  uint32_t days = 0;

  /* Full years since 1970 */
  for (uint16_t y = 1970; y < year; y++)
  {
    days += is_leap_year(y) ? 366 : 365;
  }

  /* Days in current year before this month */
  days += s_days_since_epoch[month - 1];

  /* Add leap day if needed */
  if (month > 2 && is_leap_year(year))
  {
    days += 1;
  }

  /* Days in current month (1-indexed) */
  days += day - 1;

  /* Convert to seconds and add time of day */
  uint32_t epoch = (days * 86400UL) +
                   ((uint32_t)hour * 3600UL) +
                   ((uint32_t)minute * 60UL) +
                   (uint32_t)second;

  return epoch;
}