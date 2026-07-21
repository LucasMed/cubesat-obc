/**
 * @file test_host_sd_stubs.c
 * @brief Unit tests for host_sd_spi_stubs.c — verify all SD SPI stubs
 *        return expected values.
 */

#include "sd_spi.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void test_sd_spi_init_returns_true(void)
{
    bool ok = sd_spi_init();
    assert(ok == true);
    printf("  PASS T-SDS-01 sd_spi_init returns true\n");
}

static void test_sd_spi_get_type_returns_unknown(void)
{
    sd_type_t type = sd_spi_get_type();
    assert(type == SD_TYPE_UNKNOWN);
    printf("  PASS T-SDS-02 sd_spi_get_type returns SD_TYPE_UNKNOWN\n");
}

static void test_sd_spi_read_sector_returns_true(void)
{
    uint8_t buf[512];
    memset(buf, 0xFF, sizeof(buf));
    bool ok = sd_spi_read_sector(0, buf);
    assert(ok == true);
    printf("  PASS T-SDS-03 sd_spi_read_sector returns true\n");
}

static void test_sd_spi_write_sector_returns_true(void)
{
    uint8_t buf[512];
    memset(buf, 0xAA, sizeof(buf));
    bool ok = sd_spi_write_sector(0, buf);
    assert(ok == true);
    printf("  PASS T-SDS-04 sd_spi_write_sector returns true\n");
}

int main(void)
{
    printf("=== Host SD Stubs tests ===\n");

    test_sd_spi_init_returns_true();
    test_sd_spi_get_type_returns_unknown();
    test_sd_spi_read_sector_returns_true();
    test_sd_spi_write_sector_returns_true();

    printf("ALL HOST SD STUBS TESTS PASSED\n");
    return 0;
}
