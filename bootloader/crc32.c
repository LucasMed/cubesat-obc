/**
 * @file crc32.c
 * @brief CRC32 implementation (standard CRC-32, PKZIP / Ethernet).
 *
 * Polynomial: 0xEDB88320 (reflected)
 * Initial:    0xFFFFFFFF
 * Final XOR:  0xFFFFFFFF
 *
 * Spec ref: Golden Image + Dual Boot SDD, bootloader/spec.md
 */

#include "crc32.h"

/* Lookup table for byte-at-a-time CRC32. */
static uint32_t crc32_table[256];
static int      crc32_table_initialized = 0;

/** CRC-32 polynomial (reflected representation). */
#define CRC32_POLY 0xEDB88320u

/**
 * @brief Build the CRC32 lookup table (called once).
 */
static void crc32_build_table(void)
{
    for (uint32_t i = 0; i < 256; i++)
    {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ CRC32_POLY;
            else
                crc >>= 1;
        }
        crc32_table[i] = crc;
    }
    crc32_table_initialized = 1;
}

uint32_t crc32_init(void)
{
    return 0xFFFFFFFFu;
}

uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len)
{
    if (!crc32_table_initialized)
        crc32_build_table();

    for (size_t i = 0; i < len; i++)
    {
        uint8_t index = (uint8_t)(crc ^ data[i]);
        crc = (crc >> 8) ^ crc32_table[index];
    }
    return crc;
}

uint32_t crc32_finalize(uint32_t crc)
{
    return crc ^ 0xFFFFFFFFu;
}

uint32_t crc32_compute(const uint8_t *data, size_t len)
{
    uint32_t crc = crc32_init();
    crc = crc32_update(crc, data, len);
    return crc32_finalize(crc);
}
