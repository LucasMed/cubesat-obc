#ifndef PICO_BUILD
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
// Stub for gpio_get: always return 1 (high)
int __attribute__((weak)) gpio_get(unsigned int pin)
{
  (void)pin;
  return 1;
}
#endif
