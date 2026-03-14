#ifndef PICO_BUILD
  #include "FreeRTOS.h"

  #include <stddef.h>
  #include <stdint.h>

// Stub for spi_write_blocking: always succeed
int __attribute__((weak)) spi_write_blocking(void *spi, const uint8_t *src, size_t len)
{
  (void)spi;
  (void)src;
  return (int)len;
}

// Stub for spi_read_blocking: fill with zeros
int __attribute__((weak)) spi_read_blocking(void *spi, uint8_t filler, uint8_t *dst, size_t len)
{
  (void)spi;
  (void)filler;
  for (size_t i = 0; i < len; ++i)
  {
    dst[i] = 0;
  }
  return (int)len;
}

void __attribute__((weak)) gpio_put(unsigned int pin, int value)
{
  (void)pin;
  (void)value;
}

uint16_t __attribute__((weak)) adc_read(void)
{
  return 0;
}

// Stub for gpio_get: always return 1 (high)
int __attribute__((weak)) gpio_get(unsigned int pin)
{
  (void)pin;
  return 1;
}

BaseType_t __attribute__((weak))
xTaskNotifyWait_Mock(uint32_t ulBitsToClearOnEntry, uint32_t ulBitsToClearOnExit,
                     uint32_t *pulNotificationValue, TickType_t xTicksToWait)
{
  (void)ulBitsToClearOnEntry;
  (void)ulBitsToClearOnExit;
  (void)xTicksToWait;
  if (pulNotificationValue != NULL)
  {
    *pulNotificationValue = 0;
  }
  return pdPASS;
}
#endif
