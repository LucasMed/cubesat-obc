/**
 * @file spi_flash.c
 * @brief Minimal SPI flash driver for bootloader W25Q64 access.
 *
 * Uses Pico SDK hardware_spi (spi0) to read from the external
 * W25Q64 flash for golden image restore.
 *
 * Pins:
 *   SCK  = GPIO 2
 *   TX   = GPIO 3
 *   RX   = GPIO 4
 *   CS   = GPIO 7
 *
 * Spec ref: W25Q64JV datasheet, SPI timing: 50 MHz max.
 */

#include "spi_flash.h"

#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "pico/error.h"

/* ------------------------------------------------------------------ */
/* W25Q64 Commands                                                      */
/* ------------------------------------------------------------------ */
#define CMD_READ_JEDEC_ID    0x9F
#define CMD_READ_DATA        0x03

/* ------------------------------------------------------------------ */
/* Pin assignments (shared SPI payload bus)                             */
/* ------------------------------------------------------------------ */
#define SPI_PORT             spi0
#define PIN_SCK              2
#define PIN_TX               3
#define PIN_RX               4
#define PIN_CS               7

#define SPI_CLOCK_DIVIDER    4   /* ~12.5 MHz at 50 MHz system clock */

/* ------------------------------------------------------------------ */
/* Expected JEDEC ID for W25Q64                                         */
/* ------------------------------------------------------------------ */
#define JEDEC_MANUFACTURER   0xEF  /* Winbond */
#define JEDEC_MEMORY_TYPE    0x40
#define JEDEC_CAPACITY       0x17  /* W25Q64 = 64 Mbit */

/* ------------------------------------------------------------------ */
/* Implementation                                                       */
/* ------------------------------------------------------------------ */

static uint8_t cs_pin = PIN_CS;

static void cs_select(void)
{
    gpio_put(cs_pin, 0);
}

static void cs_deselect(void)
{
    gpio_put(cs_pin, 1);
}

bool spi_flash_init(void)
{
    /* Configure SPI pins */
    spi_init(SPI_PORT, 125 * 1000 * 1000 / SPI_CLOCK_DIVIDER);

    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_TX,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_RX,  GPIO_FUNC_SPI);

    /* Chip select as GPIO, active low */
    gpio_init(cs_pin);
    gpio_set_dir(cs_pin, GPIO_OUT);
    gpio_put(cs_pin, 1);  /* deselect */

    /* Verify W25Q64 presence by reading JEDEC ID */
    uint8_t cmd = CMD_READ_JEDEC_ID;
    uint8_t jedec[3] = {0};

    cs_select();
    spi_write_blocking(SPI_PORT, &cmd, 1);
    spi_read_blocking(SPI_PORT, 0, jedec, 3);
    cs_deselect();

    return (jedec[0] == JEDEC_MANUFACTURER &&
            jedec[1] == JEDEC_MEMORY_TYPE  &&
            jedec[2] == JEDEC_CAPACITY);
}

bool spi_flash_read(uint32_t offset, uint8_t *data, size_t len)
{
    if (!data || len == 0)
        return false;

    /* READ_DATA command: 0x03 + 3-byte address */
    uint8_t cmd[4] = {
        CMD_READ_DATA,
        (uint8_t)(offset >> 16),
        (uint8_t)(offset >> 8),
        (uint8_t)(offset)
    };

    cs_select();
    int ret = spi_write_blocking(SPI_PORT, cmd, 4);
    if (ret < 0)
    {
        cs_deselect();
        return false;
    }
    int bytes_read = spi_read_blocking(SPI_PORT, 0, data, len);
    cs_deselect();

    return bytes_read >= 0 && (size_t)bytes_read == len;
}
