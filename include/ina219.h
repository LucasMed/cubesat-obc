/**
 * @file ina219.h
 * @brief INA219 Power Monitor Driver
 *
 * I2C high-side current and power sensor.
 * Datasheet: https://www.ti.com/lit/ds/symlink/ina219.pdf
 *
 * Specifications:
 * - I2C Address: 0x40 (A0=A1=GND, default)
 * - Bus voltage range: 0-26V (internal 26V shunt voltage max)
 * - Shunt resistance: External (typically 0.1 ohm)
 * - Current resolution: 100 µA (LSB for 400mA range)
 * - Power resolution: 2 mW
 * - ADC: 12-bit, configurable conversion time
 */

#ifndef INA219_H
#define INA219_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief I2C address (7-bit, A0=A1=GND).
   */
#define INA219_ADDR 0x40

  /**
   * @brief Register addresses.
   */
#define INA219_REG_CONFIG 0x00
#define INA219_REG_SHUNT_VOLTAGE 0x01
#define INA219_REG_BUS_VOLTAGE 0x02
#define INA219_REG_POWER 0x03
#define INA219_REG_CURRENT 0x04
#define INA219_REG_CALIBRATION 0x05

  /**
   * @brief Configuration register bits.
   */
#define INA219_CONFIG_RESET (1 << 15)
#define INA219_CONFIG_BRNG (1 << 13)      // Bus voltage range: 0=16V, 1=32V
#define INA219_CONFIG_PG(x) ((x) << 11)   // PGA gain: 0=±40mV, 1=±80mV, 2=±160mV, 3=±320mV
#define INA219_CONFIG_BADC(x) ((x) << 7)  // Bus ADC: 0-3 (9-12 bit)
#define INA219_CONFIG_SADC(x) ((x) << 3)  // Shunt ADC: 0-3 (9-12 bit)
#define INA219_CONFIG_MODE(x) ((x)&0x07)  // Operating mode

#define INA219_MODE_POWER_DOWN 0x00
#define INA219_MODE_SHUNT_TRIG 0x01
#define INA219_MODE_BUS_TRIG 0x02
#define INA219_MODE_SHUNT_BUS_TRIG 0x03
#define INA219_MODE_POWER_DOWN_ALT 0x04
#define INA219_MODE_SHUNT_CONT 0x05
#define INA219_MODE_BUS_CONT 0x06
#define INA219_MODE_SHUNT_BUS_CONT 0x07  // Continuous mode (default)

  /**
   * @brief Power measurement result.
   */
  typedef struct
  {
    int16_t bus_voltage_mv;    // Bus voltage in millivolts
    int16_t shunt_voltage_uv;  // Shunt voltage in microvolts
    int32_t current_ua;        // Current in microamps
    int32_t power_uw;          // Power in microwatts
  } ina219_data_t;

  /**
   * @brief Initialize the INA219 power monitor.
   *
   * Configures the sensor with default calibration for ~400mA range
   * with 0.1 ohm shunt resistor.
   *
   * @return true on success, false on failure
   */
  bool ina219_init(void);

  /**
   * @brief Check if INA219 is present on the I2C bus.
   *
   * Reads the configuration register to verify communication.
   *
   * @return true if sensor responds
   */
  bool ina219_is_present(void);

  /**
   * @brief Read power data from INA219.
   *
   * Reads bus voltage, shunt voltage, and calculates current/power.
   * Uses internal calibration for conversion.
   *
   * @param data Pointer to store measurement results. Must not be NULL.
   * @return true on success, false on failure (I2C error)
   */
  bool ina219_read_power(ina219_data_t *data);

  /**
   * @brief Reset INA219 to default configuration.
   *
   * @return true on success, false on failure
   */
  bool ina219_reset(void);

  /**
   * @brief Get last reading timestamp (milliseconds since boot).
   *
   * @return Timestamp in milliseconds
   */
  uint32_t ina219_get_last_reading_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* INA219_H */