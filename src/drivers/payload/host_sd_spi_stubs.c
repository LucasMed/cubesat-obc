// host_sd_spi_stubs.c
// Stubs para pruebas/análisis en host (no hardware real)
// Genera respuestas "no disponible" para FatFs/diskio

#include "sd_spi.h"

#include <stdbool.h>
#include <stdint.h>

// Simula éxito en inicialización (true)
bool sd_spi_init(void)
{
  return true;
}
// Simula tipo desconocido sd_card
sd_type_t sd_spi_get_type(void)
{
  return SD_TYPE_UNKNOWN;
}
// Simula operaciones de lectura/escritura exitosas (true)
bool sd_spi_read_sector(uint32_t sector,
                        uint8_t *buffer)  // NOLINT(readability-non-const-parameter)
{
  (void)sector;
  (void)buffer;
  return true;
}
bool sd_spi_write_sector(uint32_t sector, const uint8_t *buffer)
{
  (void)sector;
  (void)buffer;
  return true;
}
