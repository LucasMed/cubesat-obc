#ifndef HOST_I2C_INTERFACE_H
#define HOST_I2C_INTERFACE_H
#include <stddef.h>
#include <stdint.h>
static inline int i2c_bus_init(uint32_t sda_pin, uint32_t scl_pin, uint32_t baudrate)
{
  (void)sda_pin;
  (void)scl_pin;
  (void)baudrate;
  return 0;
}
static inline int i2c_bus_write(uint8_t addr, const uint8_t *data, size_t len)
{
  (void)addr;
  (void)data;
  (void)len;
  return 0;
}
static inline int i2c_bus_read(uint8_t addr, uint8_t *data, size_t len)
{
  (void)addr;
  for (size_t i = 0; i < len; ++i)
    data[i] = 0;
  return 0;
}
static inline int i2c_bus_write_read(uint8_t addr, const uint8_t *tx, size_t tx_len, uint8_t *rx,
                                     size_t rx_len)
{
  (void)addr;
  (void)tx;
  (void)tx_len;
  for (size_t i = 0; i < rx_len; ++i)
    rx[i] = 0x68;
  return 0;
}
#endif
