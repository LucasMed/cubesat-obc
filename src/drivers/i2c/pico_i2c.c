/**
 * @file pico_i2c.c
 * @brief Raspberry Pi Pico implementation of the I2C interface.
 *
 * Uses timeout-based I2C calls (50 ms) to prevent indefinite hangs
 * when sensors are not connected or the bus is stuck.
 */

#include "drivers/i2c_interface.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"

#include <stdio.h>

#if defined(PICO_BUILD)
  #include "../uart/pico_usart.h"
  #include "FreeRTOS.h"
  #include "semphr.h"
#endif

// Define which I2C instance to use (default I2C0)
#define I2C_INST i2c0

/** I2C operation timeout in microseconds (50 ms — generous for 400 kHz bus). */
#define I2C_TIMEOUT_US 50000

#if defined(PICO_BUILD)
static SemaphoreHandle_t s_i2c_mutex = NULL;
#endif

int i2c_bus_init(uint32_t sda_pin, uint32_t scl_pin, uint32_t baudrate)
{
  // Initialize I2C instance
  i2c_init(I2C_INST, baudrate);

  // Configure GPIO pins
  gpio_set_function(sda_pin, GPIO_FUNC_I2C);
  gpio_set_function(scl_pin, GPIO_FUNC_I2C);

  // Enable pull-ups (hardware usually has them, but safety first)
  gpio_pull_up(sda_pin);
  gpio_pull_up(scl_pin);

  /* Allow I2C bus to stabilize and sensors to respond to initial bus activity.
   * Without this, fast probes after initialization can fail on devices like
   * MPU6050 that are in sleep mode on POR and need I2C bus settle time.
   * Spec: MPU6050 POR recovery time ~100ms, we use conservative 10ms here. */
  sleep_ms(10);

#if defined(PICO_BUILD)
  s_i2c_mutex = xSemaphoreCreateMutex();
  configASSERT(s_i2c_mutex != NULL);
#endif

  return 0;
}

int i2c_bus_write(uint8_t addr, const uint8_t *data, size_t len)
{
#if defined(PICO_BUILD)
  xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
#endif
  int ret = i2c_write_timeout_us(I2C_INST, addr, data, len, false, I2C_TIMEOUT_US);
#if defined(PICO_BUILD)
  xSemaphoreGive(s_i2c_mutex);
#endif
  return (ret < 0) ? ret : 0;
}

int i2c_bus_read(uint8_t addr, uint8_t *data, size_t len)
{
#if defined(PICO_BUILD)
  xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
#endif
  int ret = i2c_read_timeout_us(I2C_INST, addr, data, len, false, I2C_TIMEOUT_US);
#if defined(PICO_BUILD)
  xSemaphoreGive(s_i2c_mutex);
#endif
  return (ret < 0) ? ret : 0;
}

int i2c_bus_write_read(uint8_t addr, const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_len)
{
#if defined(PICO_BUILD)
  xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
#endif

  // Write then read with repeated start
  int ret = i2c_write_timeout_us(I2C_INST, addr, tx, tx_len, true, I2C_TIMEOUT_US);
  if (ret < 0)
  {
#if defined(PICO_BUILD)
    xSemaphoreGive(s_i2c_mutex);
#endif
    return ret;
  }

  ret = i2c_read_timeout_us(I2C_INST, addr, rx, rx_len, false, I2C_TIMEOUT_US);
#if defined(PICO_BUILD)
  xSemaphoreGive(s_i2c_mutex);
#endif
  return (ret < 0) ? ret : 0;
}

/**
 * @brief Scan I2C bus for devices
 * @param start_addr First address to scan (default 0x03)
 * @param end_addr Last address to scan (default 0x77)
 * @return Number of devices found
 */
int i2c_bus_scan(uint8_t start_addr, uint8_t end_addr)
{
#if defined(PICO_BUILD)
  xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
#endif

  int found = 0;
  uint8_t dummy;

  printf("    Scanning I2C0 bus...\r\n");
  for (uint8_t addr = start_addr; addr <= end_addr; addr++)
  {
    // Try to read 1 byte without sending any data first (probe)
    int ret = i2c_read_timeout_us(I2C_INST, addr, &dummy, 1, false, 1000);
    if (ret >= 0)
    {
      printf("    Found device at 0x%02X\r\n", addr);

      /* Also report to UART1 (HC-12 radio) so remote user can see scan results
       * on telemetry link without needing access to USB console. */
#ifdef PICO_BUILD
      char buf[32];
      snprintf(buf, sizeof(buf), "  0x%02X\r\n", addr);
      uart1_puts_safe(buf);
#endif

      found++;
    }
  }

#if defined(PICO_BUILD)
  xSemaphoreGive(s_i2c_mutex);
#endif

  return found;
}

#if defined(PICO_BUILD)
void i2c_bus_lock(void)
{
  xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
}

void i2c_bus_unlock(void)
{
  xSemaphoreGive(s_i2c_mutex);
}
#endif
