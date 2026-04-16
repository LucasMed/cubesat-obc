/**
 * @file ina219_stub.c
 * @brief INA219 Power Monitor Driver Stub (Host/Testing)
 *
 * Stub implementation that returns mock data for testing on host.
 */

#include "ina219.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/* Mock power data: ~150mA @ 5V = ~750mW */
static const ina219_data_t s_mock_power_data = {
    .bus_voltage_mv = 5000,    /* 5.0V */
    .shunt_voltage_uv = 15000, /* 15mV across 0.1 ohm = 150mA */
    .current_ua = 150000,      /* 150 mA = 150,000 µA */
    .power_uw = 750000         /* 750 mW = 750,000 µW */
};

static uint32_t s_last_reading_ms = 0;
static uint32_t s_call_count = 0;

bool ina219_init(void)
{
#ifdef PICO_BUILD
  printf("ina219: Initialized (stub) at 0x%02X\r\n", INA219_ADDR);
#else
  (void)printf;
#endif
  return true;
}

bool ina219_is_present(void)
{
  return true;
}

bool ina219_read_power(ina219_data_t *data)
{
  if (data == NULL)
  {
    return false;
  }

  /* Return mock data with slight variation to simulate real readings */
  *data = s_mock_power_data;

  /* Add small random variation (deterministic based on call count) */
  s_call_count++;
  data->bus_voltage_mv += (int16_t)((s_call_count % 7) - 3) * 10; /* ±30 mV */
  data->current_ua += (int32_t)((s_call_count % 11) - 5) * 1000;  /* ±10 mA */
  data->power_uw = ((int32_t)data->bus_voltage_mv * (int32_t)data->current_ua) / 1000;

  /* Store timestamp */
  s_last_reading_ms = s_call_count * 1000;

  return true;
}

bool ina219_reset(void)
{
  s_call_count = 0;
  return true;
}

uint32_t ina219_get_last_reading_ms(void)
{
  return s_last_reading_ms;
}