/**
 * @file sht31.c
 * @brief SHT31-D Temperature and Humidity Sensor Driver Implementation
 */

#include "sht31.h"

#include "drivers/i2c_interface.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef PICO_BUILD
  #include "pico/stdlib.h"
#endif

/* SHT31 Command codes */
#define SHT31_CMD_MEASURE_HIGH 0x2400 /* Single shot, high repeatability */
#define SHT31_CMD_HEATER_ON 0x306D    /* Enable heater */
#define SHT31_CMD_HEATER_OFF 0x3066   /* Disable heater */
#define SHT31_CMD_SOFT_RESET 0x30A2   /* Soft reset */
#define SHT31_CMD_READ_STATUS 0xF32D  /* Read status register */
#define SHT31_CMD_CLEAR_STATUS 0x3041 /* Clear status */

/* Periodic mode commands - start measurement */
#define SHT31_CMD_PERIODIC_05HZ 0x202F /* 0.5 measurements/sec, high repeatability */
#define SHT31_CMD_PERIODIC_1HZ 0x2124 /* 1 measurement/sec, high repeatability */
#define SHT31_CMD_PERIODIC_2HZ 0x2229 /* 2 measurements/sec, high repeatability */
#define SHT31_CMD_PERIODIC_4HZ 0x2324 /* 4 measurements/sec, high repeatability */
#define SHT31_CMD_PERIODIC_10HZ 0x2721 /* 10 measurements/sec, high repeatability */

/* Fetch command - read from periodic mode */
#define SHT31_CMD_FETCH 0xE000

/* Art command - accelerated response time */
#define SHT31_CMD_ART 0x2B06

static uint8_t s_sht31_addr = SHT31_ADDR_DEFAULT;

/**
 * @brief Calculate CRC-8 for SHT31
 *
 * Polynomial: 0x31 (x^8 + x^5 + x^4 + 1)
 * Initial value: 0xFF
 */
static uint8_t sht31_crc8(const uint8_t data[2])
{
  uint8_t crc = 0xFF;

  for (int i = 0; i < 2; i++)
  {
    crc ^= data[i];
    for (int j = 8; j > 0; --j)
    {
      if (crc & 0x80)
      {
        crc = (crc << 1) ^ 0x31;
      }
      else
      {
        crc = crc << 1;
      }
    }
  }

  return crc;
}

bool sht31_init(uint8_t addr)
{
  s_sht31_addr = addr;

  /* Soft reset */
  const uint8_t cmd[2] = {(uint8_t)(SHT31_CMD_SOFT_RESET >> 8),
                          (uint8_t)(SHT31_CMD_SOFT_RESET & 0xFF)};

  if (i2c_bus_write(s_sht31_addr, cmd, 2) < 0)
  {
#ifdef PICO_BUILD
    printf("sht31: Failed to initialize (reset)\n");
#endif
    return false;
  }

  /* Wait for reset to complete */
#ifdef PICO_BUILD
  sleep_ms(2);
#else
  /* Host: no delay needed */
#endif

  /* Verify sensor is present */
  if (!sht31_is_present(s_sht31_addr))
  {
#ifdef PICO_BUILD
    printf("sht31: Sensor not detected at 0x%02X\n", s_sht31_addr);
#endif
    return false;
  }

#ifdef PICO_BUILD
  printf("sht31: Initialized at 0x%02X\n", s_sht31_addr);
#endif

  return true;
}

bool sht31_is_present(uint8_t addr)
{
  /* Try to read status register */
  const uint8_t cmd[2] = {(uint8_t)(SHT31_CMD_READ_STATUS >> 8),
                          (uint8_t)(SHT31_CMD_READ_STATUS & 0xFF)};

  uint8_t status[3];
  if (i2c_bus_write_read(addr, cmd, 2, status, 3) < 0)
  {
    return false;
  }

  /* Verify CRC */
  uint8_t expected_crc = sht31_crc8(status);
  if (expected_crc != status[2])
  {
#ifdef PICO_BUILD
    printf("sht31: Status CRC mismatch (expected 0x%02X, got 0x%02X)\n", expected_crc, status[2]);
#endif
    return false;
  }

  return true;
}

bool sht31_read(float *temperature, float *humidity)
{
  if (temperature == NULL && humidity == NULL)
  {
    return false;
  }

  /* Send single shot measurement command */
  const uint8_t cmd[2] = {(uint8_t)(SHT31_CMD_MEASURE_HIGH >> 8),
                          (uint8_t)(SHT31_CMD_MEASURE_HIGH & 0xFF)};

  /* Wait for measurement (typ 15ms, max 50ms) */
#ifdef PICO_BUILD
  sleep_ms(20);
#endif

uint8_t data[6];
  int i2c_result = i2c_bus_write_read(s_sht31_addr, cmd, 2, data, 6);
  if (i2c_result < 0)
  {
    return false;
  }
  
  /* Check for obviously corrupt data (all 0xFF) */
  if (data[0] == 0xFF && data[1] == 0xFF)
  {
    return false;
  }
  
  /* Verify temperature CRC */
  uint8_t temp_crc = sht31_crc8(&data[0]);
  if (temp_crc != data[2])
  {
    return false;
  }

  printf("[sht31] I2C OK, data=%02X%02X%02X%02X%02X%02X\n", data[0], data[1], data[2], data[3],
         data[4], data[5]);

  /* Check for obviously corrupt data (all 0xFF) */
  if (data[0] == 0xFF && data[1] == 0xFF)
  {
    printf("[sht31] corrupt: all 0xFF\n");
    return false;
  }

  /* Verify temperature CRC */
  uint8_t temp_crc = sht31_crc8(&data[0]);
  if (temp_crc != data[2])
  {
    printf("[sht31] temp CRC fail: calc=0x%02X got=0x%02X\n", temp_crc, data[2]);
    return false;
  }

  /* Verify humidity CRC */
  uint8_t hum_crc = sht31_crc8(&data[3]);
  if (hum_crc != data[5])
  {
    return false;
  }

  /* Parse temperature: T = -45 + 175 * (raw / 65535) */
  if (temperature != NULL)
  {
    uint16_t raw_temp = ((uint16_t)data[0] << 8) | data[1];
    *temperature = -45.0f + 175.0f * ((float)raw_temp / 65535.0f);
  }

  /* Parse humidity: RH = 100 * (raw / 65535) */
  /* Note: Some cheap modules may not have functional humidity sensor */
  if (humidity != NULL)
  {
    uint16_t raw_hum = ((uint16_t)data[3] << 8) | data[4];

    /* Check for invalid humidity data (all 0xFF means sensor not functional) */
    if (raw_hum == 0xFFFF)
    {
      *humidity = -1.0f; /* Indicate humidity not available */
    }
    else
    {
      *humidity = 100.0f * ((float)raw_hum / 65535.0f);

      /* Clamp to valid range */
      if (*humidity < 0.0f)
        *humidity = 0.0f;
      if (*humidity > 100.0f)
        *humidity = 100.0f;
    }
  }

  return true;
}

bool sht31_set_heater(bool enable)
{
  uint8_t cmd[2];
  if (enable)
  {
    cmd[0] = (uint8_t)(SHT31_CMD_HEATER_ON >> 8);
    cmd[1] = (uint8_t)(SHT31_CMD_HEATER_ON & 0xFF);
  }
  else
  {
    cmd[0] = (uint8_t)(SHT31_CMD_HEATER_OFF >> 8);
    cmd[1] = (uint8_t)(SHT31_CMD_HEATER_OFF & 0xFF);
  }

  if (i2c_bus_write(s_sht31_addr, cmd, 2) < 0)
  {
    return false;
  }

  return true;
}

/**
 * @brief Start periodic measurement mode (recommended for continuous monitoring)
 * 
 * @param hz Measurement frequency (1, 2, 4, or 10 Hz)
 * @return true on success
 */
bool sht31_start_periodic(uint8_t hz)
{
  uint8_t cmd[2];
  uint16_t cmd_code;
  
  switch (hz)
  {
    case 1:
      cmd_code = SHT31_CMD_PERIODIC_1HZ;
      break;
    case 2:
      cmd_code = SHT31_CMD_PERIODIC_2HZ;
      break;
    case 4:
      cmd_code = SHT31_CMD_PERIODIC_4HZ;
      break;
    case 10:
      cmd_code = SHT31_CMD_PERIODIC_10HZ;
      break;
    default:
      cmd_code = SHT31_CMD_PERIODIC_1HZ;
  }
  
  cmd[0] = (uint8_t)(cmd_code >> 8);
  cmd[1] = (uint8_t)(cmd_code & 0xFF);
  
  if (i2c_bus_write(s_sht31_addr, cmd, 2) < 0)
  {
    return false;
  }
  
  return true;
}

/**
 * @brief Fetch data from periodic measurement mode (non-blocking)
 * 
 * Should be called periodically after sht31_start_periodic()
 * 
 * @param temperature Output pointer for temperature in Celsius
 * @param humidity Output pointer for relative humidity in percent
 * @return true on success
 */
bool sht31_fetch(float *temperature, float *humidity)
{
  if (temperature == NULL && humidity == NULL)
  {
    return false;
  }
  
  uint8_t cmd[2] = {(uint8_t)(SHT31_CMD_FETCH >> 8),
                   (uint8_t)(SHT31_CMD_FETCH & 0xFF)};
  
  uint8_t data[6];
  if (i2c_bus_write_read(s_sht31_addr, cmd, 2, data, 6) < 0)
  {
    return false;
  }
  
  /* Check for obviously corrupt data (all 0xFF) */
  if (data[0] == 0xFF && data[1] == 0xFF)
  {
    return false;
  }
  
  /* Verify temperature CRC */
  uint8_t temp_crc = sht31_crc8(&data[0]);
  if (temp_crc != data[2])
  {
    return false;
  }
  
  /* Verify humidity CRC */
  uint8_t hum_crc = sht31_crc8(&data[3]);
  if (hum_crc != data[5])
  {
    return false;
  }
  
  /* Parse temperature: T = -45 + 175 * (raw / 65535) */
  if (temperature != NULL)
  {
    uint16_t raw_temp = ((uint16_t)data[0] << 8) | data[1];
    *temperature = -45.0f + 175.0f * ((float)raw_temp / 65535.0f);
  }
  
  /* Parse humidity: RH = 100 * (raw / 65535) */
  if (humidity != NULL)
  {
    uint16_t raw_hum = ((uint16_t)data[3] << 8) | data[4];
    if (raw_hum == 0xFFFF)
    {
      *humidity = -1.0f;
    }
    else
    {
      *humidity = 100.0f * ((float)raw_hum / 65535.0f);
    }
  }
  
  return true;
}
  
  uint8_t cmd[2] = {(uint8_t)(SHT31_CMD_FETCH >> 8),
                   (uint8_t)(SHT31_CMD_FETCH & 0xFF)};
  
  uint8_t data[6];
  if (i2c_bus_write_read(s_sht31_addr, cmd, 2, data, 6) < 0)
  {
    return false;
  }
  
  /* Check for obviously corrupt data (all 0xFF) */
  if (data[0] == 0xFF && data[1] == 0xFF)
  {
    return false;
  }
  
  /* Verify temperature CRC */
  uint8_t temp_crc = sht31_crc8(&data[0]);
  if (temp_crc != data[2])
  {
    return false;
  }
  
  /* Verify humidity CRC */
  uint8_t hum_crc = sht31_crc8(&data[3]);
  if (hum_crc != data[5])
  {
    return false;
  }
  
  /* Parse temperature: T = -45 + 175 * (raw / 65535) */
  if (temperature != NULL)
  {
    uint16_t raw_temp = ((uint16_t)data[0] << 8) | data[1];
    *temperature = -45.0f + 175.0f * ((float)raw_temp / 65535.0f);
  }
  
  /* Parse humidity: RH = 100 * (raw / 65535) */
  if (humidity != NULL)
  {
    uint16_t raw_hum = ((uint16_t)data[3] << 8) | data[4];
    if (raw_hum == 0xFFFF)
    {
      *humidity = -1.0f;
    }
    else
    {
      *humidity = 100.0f * ((float)raw_hum / 65535.0f);
    }
  }
  
  return true;
}
