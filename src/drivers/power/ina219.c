/**
 * @file ina219.c
 * @brief INA219 Power Monitor Driver Implementation
 *
 * Uses I2C interface for communication.
 * High-side current sensing with power calculation.
 *
 * Supports multiple device instances.  Two static instances are
 * maintained for the legacy singleton API (bus monitor, 0x40) and
 * the solar panel convenience API (0x41).
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

/* Safety thresholds - adjusted for USB (5V) + 2S LiPo (7.4V nominal)
 * USB: 4.0-5.5V | Battery: 6.0-8.4V */
#define INA219_VOLTAGE_MIN_MV 3500   /**< 3.5V - minimum safe voltage (USB/battery) */
#define INA219_VOLTAGE_MAX_MV 8500   /**< 8.5V - overvoltage (2S LiPo full charge ~8.4V) */
#define INA219_CURRENT_MAX_UA 500000 /**< 500mA - overcurrent/short */
#define INA219_POWER_MAX_UW 2000000  /**< 2W - excessive power draw */

/* Solar panel thresholds — wider range since panels can be unloaded (Voc)
 * or heavily loaded near max-power point. */
#define INA219_SOLAR_VOLTAGE_MIN_MV 0     /**< 0V — panel in shadow */
#define INA219_SOLAR_VOLTAGE_MAX_MV 10000 /**< 10V — max open-circuit for typical CubeSat panel */
#define INA219_SOLAR_CURRENT_MAX_UA 600000 /**< 600mA — short-circuit current for typical panel */
#define INA219_SOLAR_POWER_MAX_UW 3000000  /**< 3W — max panel output */

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

/* ------------------------------------------------------------------ */
/*  Static device instances                                            */
/* ------------------------------------------------------------------ */

/** Default bus-power monitor at 0x40 */
static ina219_t s_bus_dev = {
    .addr = INA219_ADDR,
    .calibration_value = INA219_CALIBRATION_VALUE,
    .last_voltage_mv = 0,
    .last_reading_ms = 0,
    .initialized = false,
};

/** Solar-panel monitor at 0x41 */
static ina219_t s_solar_dev = {
    .addr = INA219_ADDR_SOLAR,
    .calibration_value = INA219_CALIBRATION_VALUE,
    .last_voltage_mv = 0,
    .last_reading_ms = 0,
    .initialized = false,
};

/* ------------------------------------------------------------------ */
/*  Low-level I2C helpers                                              */
/* ------------------------------------------------------------------ */

static bool write_register(uint8_t addr, uint8_t reg, uint16_t value)
{
  uint8_t data[3];
  data[0] = reg;
  data[1] = (uint8_t)(value >> 8);    // MSB
  data[2] = (uint8_t)(value & 0xFF);  // LSB

  if (i2c_bus_write(addr, data, 3) < 0)
  {
    return false;
  }
  return true;
}

static bool read_register(uint8_t addr, uint8_t reg, uint16_t *value)
{
  uint8_t data[2];

  if (i2c_bus_write_read(addr, &reg, 1, data, 2) < 0)
  {
    return false;
  }

  *value = ((uint16_t)data[0] << 8) | data[1];
  return true;
}

/* ------------------------------------------------------------------ */
/*  Common instance logic                                              */
/* ------------------------------------------------------------------ */

bool ina219_init_device(ina219_t *dev, uint8_t addr)
{
  if (dev == NULL)
  {
    return false;
  }

  dev->addr = addr;
  dev->calibration_value = INA219_CALIBRATION_VALUE;
  dev->last_voltage_mv = 0;
  dev->last_reading_ms = 0;

  if (!ina219_device_is_present(dev))
  {
#ifdef PICO_BUILD
    printf("ina219(0x%02X): not found\r\n", addr);
#endif
    dev->initialized = false;
    return false;
  }

  /* Reset to known state */
  if (!ina219_device_reset(dev))
  {
    dev->initialized = false;
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

  if (!write_register(dev->addr, INA219_REG_CONFIG, config))
  {
    dev->initialized = false;
    return false;
  }

  /* Set calibration register */
  if (!write_register(dev->addr, INA219_REG_CALIBRATION, dev->calibration_value))
  {
    dev->initialized = false;
    return false;
  }

  dev->initialized = true;

#ifdef PICO_BUILD
  printf("ina219(0x%02X): Initialized\r\n", dev->addr);
#endif

  return true;
}

bool ina219_device_is_present(const ina219_t *dev)
{
  if (dev == NULL)
  {
    return false;
  }

  uint16_t config;

  /* Try to read the config register */
  if (!read_register(dev->addr, INA219_REG_CONFIG, &config))
  {
    return false;
  }

  /* Check that we can read a valid configuration */
  return (config != 0xFFFF);
}

bool ina219_device_read_power(ina219_t *dev, ina219_data_t *data)
{
  if (dev == NULL || data == NULL)
  {
    return false;
  }

  uint16_t bus_voltage_raw;
  uint16_t shunt_voltage_raw;

  /* Read bus voltage */
  if (!read_register(dev->addr, INA219_REG_BUS_VOLTAGE, &bus_voltage_raw))
  {
    return false;
  }

  /* Read shunt voltage */
  if (!read_register(dev->addr, INA219_REG_SHUNT_VOLTAGE, &shunt_voltage_raw))
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

  /* Current: I = V / R (Ohm's law)
   * shunt_voltage_uv in µV, R_shunt = 0.1 Ω
   * I_µA = V_µV / 0.1 = V_µV * 10
   */
  data->current_ua = (int32_t)data->shunt_voltage_uv * 10;

  /* Power: P = V * I
   * V in mV, I in µA → convert to µW:
   * P_µW = V_mV * I_µA / 1000
   */
  data->power_uw = ((int32_t)data->bus_voltage_mv * data->current_ua) / 1000;

  /* Safety checks - generate faults if anomalies detected */
#ifdef PICO_BUILD
  if (dev->addr == INA219_ADDR)
  {
    /* Bus monitor thresholds */
    if (data->bus_voltage_mv < INA219_VOLTAGE_MIN_MV)
    {
      fault_report(FAULT_EPS_VBATT_CRITICAL, FAULT_LEVEL_CRITICAL);
    }
    else if (data->bus_voltage_mv > INA219_VOLTAGE_MAX_MV)
    {
      fault_report(FAULT_EPS_VBATT_EMERGENCY, FAULT_LEVEL_WARNING);
    }

    if (data->current_ua > INA219_CURRENT_MAX_UA)
    {
      fault_report(FAULT_EPS_OVERCURRENT, FAULT_LEVEL_ERROR);
    }

    if (data->power_uw > INA219_POWER_MAX_UW)
    {
      fault_report(FAULT_EPS_OVERCURRENT, FAULT_LEVEL_WARNING);
    }
  }
  else if (dev->addr == INA219_ADDR_SOLAR)
  {
    /* Solar panel thresholds */
    if (data->bus_voltage_mv > INA219_SOLAR_VOLTAGE_MAX_MV)
    {
      fault_report(FAULT_EPS_VBATT_EMERGENCY, FAULT_LEVEL_WARNING);
    }

    if (data->current_ua > INA219_SOLAR_CURRENT_MAX_UA)
    {
      fault_report(FAULT_EPS_OVERCURRENT, FAULT_LEVEL_WARNING);
    }

    if (data->power_uw > INA219_SOLAR_POWER_MAX_UW)
    {
      fault_report(FAULT_EPS_OVERCURRENT, FAULT_LEVEL_WARNING);
    }
  }
#endif

  /* Store timestamp and voltage in instance */
#ifdef PICO_BUILD
  dev->last_reading_ms = to_ms_since_boot(get_absolute_time());
#endif
  dev->last_voltage_mv = data->bus_voltage_mv;

  return true;
}

bool ina219_device_reset(ina219_t *dev) // cppcheck-suppress constParameterPointer
{
  if (dev == NULL)
  {
    return false;
  }
  /* Set reset bit */
  return write_register(dev->addr, INA219_REG_CONFIG, INA219_CONFIG_RESET);
}

uint32_t ina219_device_get_last_reading_ms(const ina219_t *dev)
{
  if (dev == NULL)
  {
    return 0;
  }
  return dev->last_reading_ms;
}

int16_t ina219_device_get_voltage_mv(const ina219_t *dev)
{
  if (dev == NULL)
  {
    return 0;
  }
  return dev->last_voltage_mv;
}

/* ------------------------------------------------------------------ */
/*  Legacy singleton API — delegates to s_bus_dev                      */
/* ------------------------------------------------------------------ */

bool ina219_init(void)
{
  return ina219_init_device(&s_bus_dev, INA219_ADDR);
}

bool ina219_is_present(void)
{
  return ina219_device_is_present(&s_bus_dev);
}

bool ina219_read_power(ina219_data_t *data)
{
  return ina219_device_read_power(&s_bus_dev, data);
}

bool ina219_reset(void)
{
  return ina219_device_reset(&s_bus_dev);
}

uint32_t ina219_get_last_reading_ms(void)
{
  return ina219_device_get_last_reading_ms(&s_bus_dev);
}

int16_t ina219_get_voltage_mv(void)
{
  return ina219_device_get_voltage_mv(&s_bus_dev);
}

/* ------------------------------------------------------------------ */
/*  Solar panel convenience API — delegates to s_solar_dev             */
/* ------------------------------------------------------------------ */

bool ina219_solar_init(void)
{
  return ina219_init_device(&s_solar_dev, INA219_ADDR_SOLAR);
}

bool ina219_solar_is_present(void)
{
  return ina219_device_is_present(&s_solar_dev);
}

bool ina219_solar_read_power(ina219_data_t *data)
{
  return ina219_device_read_power(&s_solar_dev, data);
}

bool ina219_solar_reset(void)
{
  return ina219_device_reset(&s_solar_dev);
}

uint32_t ina219_solar_get_last_reading_ms(void)
{
  return ina219_device_get_last_reading_ms(&s_solar_dev);
}

int16_t ina219_solar_get_voltage_mv(void)
{
  return ina219_device_get_voltage_mv(&s_solar_dev);
}
