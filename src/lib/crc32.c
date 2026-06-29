/**
 * @file crc32.c
 * @brief CRC-32/ISO-HDLC shared implementation.
 *
 * Polynomial: 0xEDB88320 (reflected)
 * Initial:    0xFFFFFFFF
 * Final XOR:  0xFFFFFFFF
 *
 * Uses a 256-entry lookup table for byte-at-a-time computation.
 * Suitable for both embedded (PICO_BUILD) and host (test) builds.
 */

#include "crc32.h"

/* ------------------------------------------------------------------ */
/* Lookup table                                                        */
/* ------------------------------------------------------------------ */

/** CRC-32 polynomial (reflected representation). */
#define CRC32_POLY 0xEDB88320u

static uint32_t s_crc32_table[256];
static int s_crc32_initialised = 0;

static void crc32_build_table(void)
{
  for (uint32_t i = 0u; i < 256u; i++)
  {
    uint32_t crc = i;
    for (int j = 0; j < 8; j++)
    {
      if (crc & 1u)
      {
        crc = (crc >> 1u) ^ CRC32_POLY;
      }
      else
      {
        crc >>= 1u;
      }
    }
    s_crc32_table[i] = crc;
  }
  s_crc32_initialised = 1;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

uint32_t crc32_init(void)
{
  return 0xFFFFFFFFu;
}

uint32_t crc32_update(uint32_t crc, const void *data, size_t len)
{
  if (!s_crc32_initialised)
  {
    crc32_build_table();
  }

  const uint8_t *bytes = (const uint8_t *)data;
  for (size_t i = 0u; i < len; i++)
  {
    const uint8_t idx = (uint8_t)((crc ^ bytes[i]) & 0xFFu);
    crc = (crc >> 8u) ^ s_crc32_table[idx];
  }
  return crc;
}

uint32_t crc32_finalize(uint32_t crc)
{
  return crc ^ 0xFFFFFFFFu;
}

uint32_t crc32_compute(const void *data, size_t len)
{
  uint32_t crc = crc32_init();
  crc = crc32_update(crc, data, len);
  return crc32_finalize(crc);
}
