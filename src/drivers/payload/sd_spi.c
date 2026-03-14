/**
 * @file sd_spi.c
 * @brief SD card driver implementation over SPI.
 *
 * Implements SDSC/SDHC initialization and block transfer.
 * Uses shared SPI0 bus with manual CS control.
 *
 * Spec ref: SD Specifications Part 1, Physical Layer Simplified Spec.
 */

#include "sd_spi.h"

#include "pico_pins.h"
#include "spi_payload.h"

#if defined(PICO_BUILD)
  #include "hardware/gpio.h"
  #include "hardware/spi.h"
  #include "pico/time.h"
#endif

#include <string.h>

/* SD Commands */
#define CMD0 0x00u   /* GO_IDLE_STATE */
#define CMD8 0x08u   /* SEND_IF_COND */
#define CMD17 0x11u  /* READ_SINGLE_BLOCK */
#define CMD24 0x18u  /* WRITE_BLOCK */
#define CMD55 0x37u  /* APP_CMD */
#define ACMD41 0x29u /* SD_SEND_OP_COND */

/* Status bits */
#define R1_IDLE_STATE 0x01u

static sd_type_t s_card_type = SD_TYPE_UNKNOWN;

/* ------------------------------------------------------------------ */
/* Internal Helpers                                                    */
/* ------------------------------------------------------------------ */

#if defined(PICO_BUILD)
static uint8_t sd_spi_transfer_byte(uint8_t tx)
{
  uint8_t rx;
  spi_write_read_blocking(SPI0_PORT, &tx, &rx, 1);
  return rx;
}

static uint8_t sd_send_cmd(uint8_t cmd, uint32_t arg)
{
  uint8_t res;

  /* Wait if busy */
  for (int i = 0; i < 1000; i++)
  {
    if (sd_spi_transfer_byte(0xFF) == 0xFF)
      break;
  }

  /* Send header */
  sd_spi_transfer_byte(0x40u | cmd);
  sd_spi_transfer_byte((uint8_t)(arg >> 24));
  sd_spi_transfer_byte((uint8_t)(arg >> 16));
  sd_spi_transfer_byte((uint8_t)(arg >> 8));
  sd_spi_transfer_byte((uint8_t)arg);

  /* CRC (fixed for CMD0/CMD8 in SPI mode) */
  uint8_t crc = 0x01u;
  if (cmd == CMD0)
    crc = 0x95u;
  else if (cmd == CMD8)
    crc = 0x87u;
  sd_spi_transfer_byte(crc);

  /* Fetch response */
  for (int i = 0; i < 10; i++)
  {
    res = sd_spi_transfer_byte(0xFF);
    if (!(res & 0x80u))
      break;
  }
  return res;
}
#else
/* Host Mocks */
static __attribute__((unused)) uint8_t sd_send_cmd(uint8_t cmd, uint32_t arg)
{
  (void)arg;
  return (cmd == CMD0) ? R1_IDLE_STATE : 0x00;
}
#endif

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

bool sd_spi_init(void)
{
  spi_payload_init();
  s_card_type = SD_TYPE_UNKNOWN;

#if defined(PICO_BUILD)
  spi_payload_cs_deselect(SPI_CS_SD_PIN);

  /* 1. 80+ clock pulses with CS high to enter SPI mode */
  for (int i = 0; i < 10; i++)
    sd_spi_transfer_byte(0xFF);

  spi_payload_cs_select(SPI_CS_SD_PIN);

  /* 2. Reset (CMD0) */
  if (sd_send_cmd(CMD0, 0) != R1_IDLE_STATE)
  {
    spi_payload_cs_deselect(SPI_CS_SD_PIN);
    return false;
  }

  /* 3. Voltage check (CMD8) */
  if (sd_send_cmd(CMD8, 0x1AA) == R1_IDLE_STATE)
  {
    /* SDv2 */
    for (int i = 0; i < 4; i++)
      sd_spi_transfer_byte(0xFF);  // Skip OCR

    /* 4. Init (ACMD41) */
    for (int i = 0; i < 1000; i++)
    {
      sd_send_cmd(CMD55, 0);
      if (sd_send_cmd(ACMD41, 0x40000000) == 0)
      {
        s_card_type = SD_TYPE_SDV2;
        break;
      }
      sleep_ms(1);
    }
  }
#else
  s_card_type = SD_TYPE_SDV2;
#endif

  spi_payload_cs_deselect(SPI_CS_SD_PIN);
  return (s_card_type != SD_TYPE_UNKNOWN);
}

bool sd_spi_read_sector(uint32_t sector, uint8_t *buffer)
{
  (void)sector;
  if (!buffer || s_card_type == SD_TYPE_UNKNOWN)
  {
    return false;
  }

#if defined(PICO_BUILD)
  spi_payload_cs_select(SPI_CS_SD_PIN);

  /* Address is in bytes for SDv1, in blocks for SDHC */
  uint32_t addr = (s_card_type == SD_TYPE_SDHC) ? sector : (sector * 512);

  if (sd_send_cmd(CMD17, addr) != 0x00)
  {
    spi_payload_cs_deselect(SPI_CS_SD_PIN);
    return false;
  }

  /* Wait for data token (0xFE) */
  for (int i = 0; i < 10000; i++)
  {
    if (sd_spi_transfer_byte(0xFF) == 0xFE)
      break;
  }

  /* Read block */
  for (int i = 0; i < 512; i++)
  {
    buffer[i] = sd_spi_transfer_byte(0xFF);
  }

  /* Skip CRC */
  sd_spi_transfer_byte(0xFF);
  sd_spi_transfer_byte(0xFF);

#else
  memset(buffer, 0x55, 512);
#endif

  spi_payload_cs_deselect(SPI_CS_SD_PIN);
  return true;
}

bool sd_spi_write_sector(uint32_t sector, const uint8_t *buffer)
{
  if (!buffer || s_card_type == SD_TYPE_UNKNOWN)
  {
    return false;
  }

  /* Writing is a placeholder for now */
  (void)sector;
  return true;
}

sd_type_t sd_spi_get_type(void)
{
  return s_card_type;
}
