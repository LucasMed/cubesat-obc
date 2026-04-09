
#include "spi_payload.h"

#include "pico_pins.h"

#if defined(PICO_BUILD)
  #include "hardware/gpio.h"
  #include "hardware/spi.h"
#endif

/* Fallback definitions for pins if not in pico_pins.h */
#ifndef SPI_CS_FLASH_PIN
  #define SPI_CS_FLASH_PIN 7
#endif
#ifndef SPI_CS_SD_PIN
  #define SPI_CS_SD_PIN 7
#endif

#include <stddef.h>
static bool s_spi_initialized = false;

bool spi_payload_init(void)
{
  if (s_spi_initialized)
  {
    return false; /* already done */
  }
#if defined(PICO_BUILD)
  spi_init(SPI0_PORT, SPI0_BAUD_RATE_INIT);
  gpio_set_function(SPI0_SCK_PIN, GPIO_FUNC_SPI);
  gpio_set_function(SPI0_MOSI_PIN, GPIO_FUNC_SPI);
  gpio_set_function(SPI0_MISO_PIN, GPIO_FUNC_SPI);

  /* All CS lines start deasserted (high) */
  const uint8_t cs_pins[] = {SPI_CS_MAG_PIN, SPI_CS_FLASH_PIN, SPI_CS_CAM_PIN};
  for (size_t i = 0; i < sizeof(cs_pins) / sizeof(cs_pins[0]); i++)
  {
    gpio_init(cs_pins[i]);
    gpio_set_dir(cs_pins[i], GPIO_OUT);
    gpio_put(cs_pins[i], 1);
  }
#else
  // Host stub: always succeed
#endif
  s_spi_initialized = true;
  return true; /* first init */
}
void spi_payload_cs_select(uint32_t cs_pin)
{
#if defined(PICO_BUILD)
  gpio_put(cs_pin, 0);
#else
  (void)cs_pin;
#endif
}

void spi_payload_cs_deselect(uint32_t cs_pin)
{
#if defined(PICO_BUILD)
  gpio_put(cs_pin, 1);
#else
  (void)cs_pin;
#endif
}
