/**
 * @file i2c_interface.h
 * @brief Abstract I2C interface for cross-platform support.
 */

#ifndef I2C_INTERFACE_H
#define I2C_INTERFACE_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Initialize the I2C master bus.
 * 
 * @param sda_pin GPIO pin for SDA
 * @param scl_pin GPIO pin for SCL
 * @param baudrate Bus speed in Hz (e.g., 400000)
 * @return 0 on success, negative error code otherwise
 */
int i2c_bus_init(uint32_t sda_pin, uint32_t scl_pin, uint32_t baudrate);

/**
 * @brief Write data to an I2C slave.
 * 
 * @param addr Slave address (7-bit)
 * @param data Buffer containing data to write
 * @param len Number of bytes to write
 * @return 0 on success, negative error code otherwise
 */
int i2c_bus_write(uint8_t addr, const uint8_t *data, size_t len);

/**
 * @brief Read data from an I2C slave.
 * 
 * @param addr Slave address (7-bit)
 * @param data Buffer to store read data
 * @param len Number of bytes to read
 * @return 0 on success, negative error code otherwise
 */
int i2c_bus_read(uint8_t addr, uint8_t *data, size_t len);

/**
 * @brief Write then read from an I2C slave (repeated start).
 * 
 * @param addr Slave address
 * @param tx Buffer containing data to write
 * @param tx_len Number of bytes to write
 * @param rx Buffer to store read data
 * @param rx_len Number of bytes to read
 * @return 0 on success, negative error code otherwise
 */
int i2c_bus_write_read(uint8_t addr, const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_len);

#endif // I2C_INTERFACE_H
