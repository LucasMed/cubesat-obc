/**
 * @file ds3231.h
 * @brief DS3231 Real Time Clock Driver
 *
 * I2C RTC with temperature-compensated crystal oscillator (TCXO).
 * Datasheet: https://www.analog.com/media/en/technical-documentation/data-sheets/ds3231.pdf
 *
 * Specifications:
 * - I2C Address: 0x68 (7-bit)
 * - Time format: BCD (binary-coded decimal)
 * - Date range: 2000-2099 (with leap year compensation)
 * - Accuracy: ±2 ppm (±1 minute/month)
 * - Battery backup: CR2032 or similar
 */

#ifndef DS3231_H
#define DS3231_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief I2C address (7-bit, ADR pin grounded).
   */
#define DS3231_ADDR 0x68

  /**
   * @brief Register addresses (timekeeping).
   */
#define DS3231_REG_SECONDS 0x00
#define DS3231_REG_MINUTES 0x01
#define DS3231_REG_HOURS 0x02
#define DS3231_REG_DAY 0x03
#define DS3231_REG_DATE 0x04
#define DS3231_REG_MONTH 0x05
#define DS3231_REG_YEAR 0x06

  /**
   * @brief Control and status registers.
   */
#define DS3231_REG_CONTROL 0x0E
#define DS3231_REG_STATUS 0x0F

  /**
   * @brief Initialize the DS3231 RTC.
   *
   * Verifies the RTC is present and responding on the I2C bus.
   *
   * @return true on success, false on failure
   */
  bool ds3231_init(void);

  /**
   * @brief Check if DS3231 is present on the I2C bus.
   *
   * Attempts to read the seconds register and validates BCD range.
   *
   * @return true if RTC responds with valid data
   */
  bool ds3231_is_present(void);

  /**
   * @brief Read current time from RTC.
   *
   * Reads all 7 timekeeping registers (seconds through year).
   *
   * @param year Pointer to store year (2000-2099). Must not be NULL.
   * @param month Pointer to store month (1-12). Must not be NULL.
   * @param day Pointer to store day (1-31). Must not be NULL.
   * @param hour Pointer to store hour (0-23). Must not be NULL.
   * @param minute Pointer to store minute (0-59). Must not be NULL.
   * @param second Pointer to store second (0-59). Must not be NULL.
   * @return true on success, false on failure (I2C error or invalid data)
   */
  bool ds3231_read_time(uint16_t *year, uint8_t *month, uint8_t *day, uint8_t *hour,
                        uint8_t *minute, uint8_t *second);

  /**
   * @brief Set time in RTC.
   *
   * Writes all 7 timekeeping registers. The DS3231 handles leap year
   * compensation internally.
   *
   * @param year Year (2000-2099)
   * @param month Month (1-12)
   * @param day Day (1-31)
   * @param hour Hour (0-23)
   * @param minute Minute (0-59)
   * @param second Second (0-59)
   * @return true on success, false on failure
   */
  bool ds3231_set_time(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute,
                       uint8_t second);

  /**
   * @brief Convert RTC time to Unix timestamp.
   *
   * Simplified conversion assuming UTC (no timezone). Does not handle
   * dates before 1970-01-01.
   *
   * @param year Year (2000-2099)
   * @param month Month (1-12)
   * @param day Day (1-31)
   * @param hour Hour (0-23)
   * @param minute Minute (0-59)
   * @param second Second (0-59)
   * @return Unix timestamp (seconds since 1970-01-01 00:00:00 UTC), or 0 on error
   */
  uint32_t ds3231_to_epoch(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute,
                           uint8_t second);

#ifdef __cplusplus
}
#endif

#endif /* DS3231_H */