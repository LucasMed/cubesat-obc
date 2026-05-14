/**
 * @file ina219_stub.c
 * @brief INA219 Power Monitor Driver Stub (Host/Testing)
 *
 * Stub implementation that returns mock data for testing on host.
 * Supports both the default bus monitor and solar panel instances.
 */

#include "ina219.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Mock power data: ~150mA @ 5V = ~750mW */
static const ina219_data_t s_mock_power_data = {
    .bus_voltage_mv = 5000,    /* 5.0V */
    .shunt_voltage_uv = 15000, /* 15mV across 0.1 ohm = 150mA */
    .current_ua = 150000,      /* 150 mA = 150,000 µA */
    .power_uw = 750000         /* 750 mW = 750,000 µW */
};

/* Mock solar data: ~100mA @ 6.5V = ~650mW (typical illuminated panel) */
static const ina219_data_t s_mock_solar_data = {
    .bus_voltage_mv = 6500,    /* 6.5V — typical CubeSat panel Vmp */
    .shunt_voltage_uv = 10000, /* 10mV across 0.1 ohm = 100mA */
    .current_ua = 100000,      /* 100 mA = 100,000 µA */
    .power_uw = 650000         /* 650 mW = 650,000 µW */
};

static uint32_t s_call_count = 0;

/* ------------------------------------------------------------------ */
/*  Instance-based API                                                 */
/* ------------------------------------------------------------------ */

bool ina219_init_device(ina219_t *dev, uint8_t addr)
{
  if (dev == NULL)
  {
    return false;
  }

  dev->addr              = addr;
  dev->calibration_value = 4096;
  dev->last_voltage_mv   = 0;
  dev->last_reading_ms   = 0;
  dev->initialized       = true;

#ifdef PICO_BUILD
  printf("ina219(0x%02X): Initialized (stub)\r\n", addr);
#else
  (void)printf;
#endif
  return true;
}

bool ina219_device_is_present(ina219_t *dev)
{
  (void)dev;
  return true;
}

bool ina219_device_read_power(ina219_t *dev, ina219_data_t *data)
{
  if (dev == NULL || data == NULL)
  {
    return false;
  }

  /* Return mock data with slight variation */
  if (dev->addr == INA219_ADDR_SOLAR)
  {
    *data = s_mock_solar_data;
  }
  else
  {
    *data = s_mock_power_data;
  }

  /* Add small random variation (deterministic based on call count) */
  s_call_count++;
  data->bus_voltage_mv += (int16_t)((s_call_count % 7) - 3) * 10; /* ±30 mV */
  data->current_ua += (int32_t)((s_call_count % 11) - 5) * 1000;  /* ±10 mA */
  data->power_uw = ((int32_t)data->bus_voltage_mv * (int32_t)data->current_ua) / 1000;

  /* Update instance state */
  dev->last_reading_ms = s_call_count * 1000;
  dev->last_voltage_mv = data->bus_voltage_mv;

  return true;
}

bool ina219_device_reset(ina219_t *dev)
{
  if (dev == NULL)
  {
    return false;
  }
  dev->last_voltage_mv = 0;
  dev->last_reading_ms = 0;
  return true;
}

uint32_t ina219_device_get_last_reading_ms(ina219_t *dev)
{
  if (dev == NULL)
  {
    return 0;
  }
  return dev->last_reading_ms;
}

int16_t ina219_device_get_voltage_mv(ina219_t *dev)
{
  if (dev == NULL)
  {
    return 0;
  }
  return dev->last_voltage_mv;
}

/* ------------------------------------------------------------------ */
/*  Legacy singleton API                                               */
/* ------------------------------------------------------------------ */

static ina219_t s_bus_stub_dev;
static ina219_t s_solar_stub_dev;
static bool s_bus_initialised  = false;
static bool s_solar_initialised = false;

bool ina219_init(void)
{
  if (!s_bus_initialised)
  {
    ina219_init_device(&s_bus_stub_dev, INA219_ADDR);
    s_bus_initialised = true;
  }
  return true;
}

bool ina219_is_present(void)
{
  return true;
}

bool ina219_read_power(ina219_data_t *data)
{
  if (!s_bus_initialised)
  {
    ina219_init();
  }
  return ina219_device_read_power(&s_bus_stub_dev, data);
}

bool ina219_reset(void)
{
  return ina219_device_reset(&s_bus_stub_dev);
}

uint32_t ina219_get_last_reading_ms(void)
{
  return ina219_device_get_last_reading_ms(&s_bus_stub_dev);
}

int16_t ina219_get_voltage_mv(void)
{
  return ina219_device_get_voltage_mv(&s_bus_stub_dev);
}

/* ------------------------------------------------------------------ */
/*  Solar panel convenience API                                        */
/* ------------------------------------------------------------------ */

bool ina219_solar_init(void)
{
  if (!s_solar_initialised)
  {
    ina219_init_device(&s_solar_stub_dev, INA219_ADDR_SOLAR);
    s_solar_initialised = true;
  }
  return true;
}

bool ina219_solar_is_present(void)
{
  return true;
}

bool ina219_solar_read_power(ina219_data_t *data)
{
  if (!s_solar_initialised)
  {
    ina219_solar_init();
  }
  return ina219_device_read_power(&s_solar_stub_dev, data);
}

bool ina219_solar_reset(void)
{
  return ina219_device_reset(&s_solar_stub_dev);
}

uint32_t ina219_solar_get_last_reading_ms(void)
{
  return ina219_device_get_last_reading_ms(&s_solar_stub_dev);
}

int16_t ina219_solar_get_voltage_mv(void)
{
  return ina219_device_get_voltage_mv(&s_solar_stub_dev);
}
