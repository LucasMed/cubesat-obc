/**
 * @file ina219.h
 * @brief INA219 Power Monitor Driver
 *
 * I2C high-side current and power sensor.
 * Datasheet: https://www.ti.com/lit/ds/symlink/ina219.pdf
 *
 * This driver supports multiple INA219 instances via an instance handle.
 * A legacy singleton API is provided for the default bus monitor (0x40),
 * and convenience macros for solar panel monitoring (0x41).
 *
 * Specifications:
 * - I2C Address: 0x40 (A0=A1=GND, default), 0x41 (A0=GND, A1=VS, solar)
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
   * @brief I2C address for solar panel INA219 (A0=GND, A1=VS).
   */
#define INA219_ADDR_SOLAR 0x41

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
   * @brief INA219 device instance handle.
   *
   * Manages per-instance state including I2C address, calibration,
   * and cached readings.  Callers declare instances and pass them
   * to the device-level API.
   */
  typedef struct
  {
    uint8_t addr;               /**< 7-bit I2C address               */
    uint16_t calibration_value; /**< Calibration register value       */
    int16_t last_voltage_mv;    /**< Last measured bus voltage [mV]  */
    uint32_t last_reading_ms;   /**< Timestamp of last reading [ms]  */
    bool initialized;           /**< true after successful init       */
  } ina219_t;

  /* ------------------------------------------------------------------ */
  /*  Instance-based API                                                 */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Initialise an INA219 device instance.
   *
   * Detects the sensor at the given address, resets it, and configures
   * for continuous 12-bit shunt+bus mode with default calibration.
   *
   * @param dev  Pointer to an ina219_t instance (must not be NULL).
   * @param addr 7-bit I2C address (e.g. INA219_ADDR or INA219_ADDR_SOLAR).
   * @return true on success, false on failure.
   */
  bool ina219_init_device(ina219_t *dev, uint8_t addr);

  /**
   * @brief Check if an INA219 device is present on the I2C bus.
   *
   * @param dev  Pointer to an initialised ina219_t instance.
   * @return true if the sensor responds.
   */
  bool ina219_device_is_present(ina219_t *dev);

  /**
   * @brief Read power data from an INA219 device.
   *
   * Reads bus voltage, shunt voltage, and calculates current/power.
   * Updates cached voltage and timestamp in the instance handle.
   *
   * @param dev   Pointer to an initialised ina219_t instance.
   * @param data  Pointer to store measurement results (must not be NULL).
   * @return true on success, false on failure (I2C error).
   */
  bool ina219_device_read_power(ina219_t *dev, ina219_data_t *data);

  /**
   * @brief Reset an INA219 device to default configuration.
   *
   * @param dev  Pointer to an initialised ina219_t instance.
   * @return true on success, false on failure.
   */
  bool ina219_device_reset(ina219_t *dev);

  /**
   * @brief Get last reading timestamp for a device instance.
   *
   * @param dev  Pointer to an initialised ina219_t instance.
   * @return Timestamp in milliseconds since boot, or 0 if no reading yet.
   */
  uint32_t ina219_device_get_last_reading_ms(ina219_t *dev);

  /**
   * @brief Get last measured bus voltage for a device instance.
   *
   * @param dev  Pointer to an initialised ina219_t instance.
   * @return Bus voltage in mV, or 0 if no reading yet.
   */
  int16_t ina219_device_get_voltage_mv(ina219_t *dev);

  /* ------------------------------------------------------------------ */
  /*  Legacy singleton API (bus monitor at INA219_ADDR = 0x40)           */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Initialise the default bus-power INA219.
   *
   * Convenience wrapper around ina219_init_device() using INA219_ADDR.
   *
   * @return true on success, false on failure
   */
  bool ina219_init(void);

  /**
   * @brief Check if default INA219 is present on the I2C bus.
   *
   * @return true if sensor responds
   */
  bool ina219_is_present(void);

  /**
   * @brief Read power data from default INA219.
   *
   * @param data Pointer to store measurement results. Must not be NULL.
   * @return true on success, false on failure (I2C error)
   */
  bool ina219_read_power(ina219_data_t *data);

  /**
   * @brief Reset default INA219 to default configuration.
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

  /**
   * @brief Get last measured bus voltage in millivolts.
   *
   * @return Bus voltage in mV, or 0 if no reading yet
   */
  int16_t ina219_get_voltage_mv(void);

  /* ------------------------------------------------------------------ */
  /*  Solar panel convenience API (INA219 at INA219_ADDR_SOLAR = 0x41)   */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Initialise the solar-panel INA219.
   *
   * Convenience wrapper around ina219_init_device() using INA219_ADDR_SOLAR.
   *
   * @return true on success, false on failure
   */
  bool ina219_solar_init(void);

  /**
   * @brief Check if solar-panel INA219 is present on the I2C bus.
   *
   * @return true if sensor responds
   */
  bool ina219_solar_is_present(void);

  /**
   * @brief Read power data from solar-panel INA219.
   *
   * @param data Pointer to store measurement results. Must not be NULL.
   * @return true on success, false on failure (I2C error)
   */
  bool ina219_solar_read_power(ina219_data_t *data);

  /**
   * @brief Reset solar-panel INA219 to default configuration.
   *
   * @return true on success, false on failure
   */
  bool ina219_solar_reset(void);

  /**
   * @brief Get last reading timestamp for solar-panel INA219.
   *
   * @return Timestamp in milliseconds
   */
  uint32_t ina219_solar_get_last_reading_ms(void);

  /**
   * @brief Get last measured solar panel voltage in millivolts.
   *
   * @return Voltage in mV, or 0 if no reading yet
   */
  int16_t ina219_solar_get_voltage_mv(void);

#ifdef __cplusplus
}
#endif

#endif /* INA219_H */
