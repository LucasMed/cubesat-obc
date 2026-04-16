/**
 * @file ina219.c
 * @brief INA219 Power Monitor Driver Implementation
 *
 * Uses I2C interface for communication.
 * High-side current sensing with power calculation.
 */

#include "ina219.h"

#include "drivers/i2c_interface.h"
#include "fault_manager.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef PICO_BUILD
  #include "pico/time.h"
#endif

/* Safety thresholds */
#define INA219_VOLTAGE_MIN_MV 3000   /**< 3.0V - battery very low */
#define INA219_VOLTAGE_MAX_MV 5500   /**< 5.5V - overvoltage */
#define INA219_CURRENT_MAX_UA 500000 /**< 500mA - overcurrent/short */
#define INA219_POWER_MAX_UW 2000000  /**< 2W - excessive power draw */

/* Calibration for ~400mA max with 0.1 ohm shunt
 * Cal = 4096 / (current_LSB * R_shunt)
 * With R_shunt = 0.1 ohm, current_LSB = 0.01 mA (10 µA)
 * Cal = 4096 / (0.01 * 100) = 4096 / 1 = 4096
 */
#define INA219_CALIBRATION_VALUE 4096

/* Current LSB = 10 µA per bit */
#define INA219_CURRENT_LSB_UA 10

/* Power LSB = 20 * Current_LSB = 200 µW per bit */
#define INA219_POWER_LSB_UW 200

/* Conversion time for continuous mode */
#define INA219_BUS_CONV_TIME_4120US 12
#define INA219_SHUNT_CONV_TIME_4120US 12

/* Conversion ready bit in bus voltage register */
#define INA219_CONV_READY_MASK 0x02

static uint32_t s_last_reading_ms = 0;

static bool write_register(uint8_t reg, uint16_t value)
{
  uint8_t data[3];
  data[0] = reg;
  data[1] = (uint8_t)(value >> 8);    // MSB
  data[2] = (uint8_t)(value & 0xFF);  // LSB

  if (i2c_bus_write(INA219_ADDR, data, 3) < 0)
  {
    return false;
  }
  return true;
}

static bool read_register(uint8_t reg, uint16_t *value)
{
  uint8_t data[2];

  if (i2c_bus_write_read(INA219_ADDR, &reg, 1, data, 2) < 0)
  {
    return false;
  }

  *value = ((uint16_t)data[0] << 8) | data[1];
  return true;
}

bool ina219_init(void)
{
  if (!ina219_is_present())
  {
#ifdef PICO_BUILD
    printf("ina219: not found\r\n");
#endif
    return false;
  }

  /* Reset to known state */
  if (!ina219_reset())
  {
    return false;
  }

  /* Configure for continuous mode
   * - Bus voltage range: 32V (BRNG=1)
   * - PGA: ±320mV (PG=3)
   * - Bus ADC: 12-bit, 532 µs conversion
   * - Shunt ADC: 12-bit, 532 µs conversion
   * - Mode: Shunt + Bus continuous
   */
  uint16_t config = INA219_CONFIG_BRNG |         // 32V range
                    INA219_CONFIG_PG(3) |        // ±320mV
                    INA219_CONFIG_BADC(3) |      // 12-bit bus
                    INA219_CONFIG_SADC(3) |      // 12-bit shunt
                    INA219_MODE_SHUNT_BUS_CONT;  // continuous

  if (!write_register(INA219_REG_CONFIG, config))
  {
    return false;
  }

  /* Set calibration register */
  if (!write_register(INA219_REG_CALIBRATION, INA219_CALIBRATION_VALUE))
  {
    return false;
  }

#ifdef PICO_BUILD
  printf("ina219: Initialized at 0x%02X\r\n", INA219_ADDR);
#endif

  return true;
}

bool ina219_is_present(void)
{
  uint16_t config;

  /* Try to read the config register */
  if (!read_register(INA219_REG_CONFIG, &config))
  {
    return false;
  }

  /* Check that we can read a valid configuration */
  return (config != 0xFFFF);
}

bool ina219_read_power(ina219_data_t *data)
{
  if (data == NULL)
  {
    return false;
  }

  uint16_t bus_voltage_raw;
  uint16_t shunt_voltage_raw;

  /* Read bus voltage */
  if (!read_register(INA219_REG_BUS_VOLTAGE, &bus_voltage_raw))
  {
    return false;
  }

  /* Read shunt voltage */
  if (!read_register(INA219_REG_SHUNT_VOLTAGE, &shunt_voltage_raw))
  {
    return false;
  }

  /* Extract values (INA219 returns signed for shunt) */
  /* Bus voltage: bits [0-13], bit 1 = CNVR (conversion ready) */
  data->bus_voltage_mv = (int16_t)((bus_voltage_raw >> 3) & 0x1FFF);
  data->bus_voltage_mv *= 4;  // LSB = 4mV

  /* Shunt voltage: signed 16-bit, LSB = 10 µV */
  data->shunt_voltage_uv = (int16_t)shunt_voltage_raw;
  data->shunt_voltage_uv *= 10;  // Convert to microvolts

  /* Current: from calibration register calculation
   * Current = shunt_voltage / R_shunt * calibration_factor
   * With our calibration: current_LSB = 10 µA
   */
  data->current_ua = ((int32_t)data->shunt_voltage_uv / 100) * 100;  // 0.1 ohm -> 10 µA/LSB

  /* Power: P = V * I
   * V in mV, I in µA → result in mW → convert to µW (multiply by 1000)
   * Formula: P_uw = V_mV * I_uA */
  data->power_uw = (int32_t)data->bus_voltage_mv * data->current_ua;

  /* Safety checks - generate faults if anomalies detected */
#ifdef PICO_BUILD
  if (data->bus_voltage_mv < INA219_VOLTAGE_MIN_MV)
  {
    /* Battery critically low - use existing EPS fault */
    fault_report(FAULT_EPS_VBATT_CRITICAL, FAULT_LEVEL_CRITICAL);
  }
  else if (data->bus_voltage_mv > INA219_VOLTAGE_MAX_MV)
  {
    /* Overvoltage detected */
    fault_report(FAULT_EPS_VBATT_EMERGENCY, FAULT_LEVEL_WARNING);
  }

  if (data->current_ua > INA219_CURRENT_MAX_UA)
  {
    /* Overcurrent - possible short circuit */
    fault_report(FAULT_EPS_OVERCURRENT, FAULT_LEVEL_ERROR);
  }

  if (data->power_uw > INA219_POWER_MAX_UW)
  {
    /* Excessive power draw */
    fault_report(FAULT_EPS_OVERCURRENT, FAULT_LEVEL_WARNING);
  }
#endif

  /* Store timestamp */
#ifdef PICO_BUILD
  s_last_reading_ms = to_ms_since_boot(get_absolute_time());
#endif

  return true;
}

bool ina219_reset(void)
{
  /* Set reset bit */
  return write_register(INA219_REG_CONFIG, INA219_CONFIG_RESET);
}

uint32_t ina219_get_last_reading_ms(void)
{
  return s_last_reading_ms;
}