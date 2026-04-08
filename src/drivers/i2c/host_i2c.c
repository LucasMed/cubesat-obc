/**
 * @file host_i2c.c
 * @brief Host-mode mock implementation of the I2C interface for testing.
 */

#include "drivers/i2c_interface.h"

/* cppcheck-suppress misra-c2012-21.6 -- MISRA deviation: host-only I2C mock
 * uses printf to log bus transactions for test diagnostics; not compiled for
 * flight targets. See MISRA_DEVIATIONS.md §21.6-D3. */
#include <stdio.h>

/* cppcheck-suppress unusedFunction -- called from Pico sensor drivers */
int i2c_bus_init(uint32_t sda_pin, uint32_t scl_pin, uint32_t baudrate)
{
  (void)printf("[Host I2C] Init: SDA=%u, SCL=%u, Baud=%u\n", sda_pin, scl_pin, baudrate);
  return 0;
}

int i2c_bus_write(uint8_t addr, const uint8_t *data, size_t len)
{
  (void)data;
  (void)printf("[Host I2C] Write to 0x%02X: %zu bytes\n", addr, len);
  return 0;
}

/* cppcheck-suppress unusedFunction -- called from Pico sensor drivers */
int i2c_bus_read(uint8_t addr, uint8_t *data, size_t len)
{
  (void)printf("[Host I2C] Read from 0x%02X: %zu bytes\n", addr, len);
  // Return dummy MPU6050 ID if it's a WHO_AM_I read
  if (len == 1u)
  {
    data[0] = 0x68u;
  }
  return 0;
}

int i2c_bus_write_read(uint8_t addr, const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_len)
{
  (void)printf("[Host I2C] Write-Read to 0x%02X: Write %zu, Read %zu\n", addr, tx_len, rx_len);
  // Mock WHO_AM_I response
  if ((tx_len == 1u) && (tx[0] == 0x75u))
  {
    rx[0] = 0x68u;
  }
  else
  {
    // Return dummy zero data for everything else
    for (size_t i = 0; i < rx_len; i++)
    {
      rx[i] = 0;
    }
  }
  return 0;
}

/* cppcheck-suppress unusedFunction -- debug scanner for host testing */
int i2c_bus_scan(uint8_t start_addr, uint8_t end_addr)
{
  (void)start_addr;
  (void)end_addr;
  (void)printf("[Host I2C] Scan not available in host mode\n");
  return 0;
}
