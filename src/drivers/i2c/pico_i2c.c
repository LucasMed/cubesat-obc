/**
 * @file pico_i2c.c
 * @brief Raspberry Pi Pico implementation of the I2C interface.
 */

#include "drivers/i2c_interface.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"

// Define which I2C instance to use (default I2C0)
#define I2C_INST i2c0

int i2c_bus_init(uint32_t sda_pin, uint32_t scl_pin, uint32_t baudrate) {
    // Initialize I2C instance
    i2c_init(I2C_INST, baudrate);
    
    // Configure GPIO pins
    gpio_set_function(sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(scl_pin, GPIO_FUNC_I2C);
    
    // Enable pull-ups (hardware usually has them, but safety first)
    gpio_pull_up(sda_pin);
    gpio_pull_up(scl_pin);
    
    return 0;
}

int i2c_bus_write(uint8_t addr, const uint8_t *data, size_t len) {
    int ret = i2c_write_blocking(I2C_INST, addr, data, len, false);
    if (ret < 0) return ret;
    return 0;
}

int i2c_bus_read(uint8_t addr, uint8_t *data, size_t len) {
    int ret = i2c_read_blocking(I2C_INST, addr, data, len, false);
    if (ret < 0) return ret;
    return 0;
}

int i2c_bus_write_read(uint8_t addr, const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_len) {
    // Write then read with repeated start
    int ret = i2c_write_blocking(I2C_INST, addr, tx, tx_len, true);
    if (ret < 0) return ret;
    
    ret = i2c_read_blocking(I2C_INST, addr, rx, rx_len, false);
    if (ret < 0) return ret;
    
    return 0;
}
