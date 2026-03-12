/**
 * @file rm3100.c
 * @brief PNI RM3100 Magnetometer Driver — SPI implementation.
 *
 * Communicates via the shared SPI0 payload bus (see spi_payload.h).
 * On PICO_BUILD: uses hardware/spi.h Pico SDK.
 * On host builds: uses weak-symbol mocks provided by the unit test.
 *
 * Spec ref: ICD-PAYLOAD-001 §11.1, §11.3
 */

#include "rm3100.h"

#include "pico_pins.h"
#include "spi_payload.h"

#include <string.h>

#if defined(PICO_BUILD)
  #include "hardware/gpio.h"
  #include "hardware/spi.h"
#endif

/* ------------------------------------------------------------------ */
/* Constants                                                           */
/* ------------------------------------------------------------------ */

/* RM3100 register addresses */
#define RM3100_REG_CMM 0x01u   /**< Continuous Measurement Mode control    */
#define RM3100_REG_CCXY 0x04u  /**< Cycle count X (2 bytes)                */
#define RM3100_REG_CCYZ 0x06u  /**< Cycle count Y/Z pair (4 bytes)         */
#define RM3100_REG_MX 0x24u    /**< Measurement result X (3 bytes)         */
#define RM3100_REG_REVID 0x36u /**< Revision ID — expected 0x22            */

#define RM3100_REVID_EXPECTED 0x22u

/**
 * Sensitivity: 13 nT per LSB at default cycle count of 200.
 * Reference: PNI RM3100 datasheet, Table 4-4.
 */
#define RM3100_SCALE_NT_PER_LSB 13.0f

/* SPI read bit: set MSB to request a read transaction */
#define RM3100_SPI_READ 0x80u

/* ------------------------------------------------------------------ */
/* Module state                                                        */
/* ------------------------------------------------------------------ */

static rm3100_vector_t s_last_vec;

/* ------------------------------------------------------------------ */
/* SPI helpers (hardware or host mock)                                 */
/* ------------------------------------------------------------------ */

#if defined(PICO_BUILD)
static bool rm3100_spi_write(uint8_t reg, const uint8_t *data, size_t len)
{
  spi_payload_cs_select(SPI_CS_MAG_PIN);
  bool ok = (spi_write_blocking(SPI0_PORT, &reg, 1) == 1);
  if (ok && len > 0)
  {
    ok = (spi_write_blocking(SPI0_PORT, data, len) == (int)len);
  }
  spi_payload_cs_deselect(SPI_CS_MAG_PIN);
  return ok;
}

static bool rm3100_spi_read(uint8_t reg, uint8_t *data, size_t len)
{
  uint8_t cmd = reg | RM3100_SPI_READ;
  spi_payload_cs_select(SPI_CS_MAG_PIN);
  bool ok = (spi_write_blocking(SPI0_PORT, &cmd, 1) == 1);
  if (ok)
  {
    ok = (spi_read_blocking(SPI0_PORT, 0x00, data, len) == (int)len);
  }
  spi_payload_cs_deselect(SPI_CS_MAG_PIN);
  return ok;
}
#else
/* Host build: weak mock symbols provided by the unit test translation unit */
extern int spi_write_blocking(void *spi, const uint8_t *src, size_t len);
extern int spi_read_blocking(void *spi, uint8_t filler, uint8_t *dst, size_t len);

static bool rm3100_spi_write(uint8_t reg, const uint8_t *data, size_t len)
{
  (void)data;
  (void)len;
  return spi_write_blocking(NULL, &reg, 1) == 1;
}

static bool rm3100_spi_read(uint8_t reg, uint8_t *data, size_t len)
{
  (void)reg;
  return spi_read_blocking(NULL, 0x00, data, len) == (int)len;
}
#endif

/* ------------------------------------------------------------------ */
/* DRDY polling helper                                                 */
/* ------------------------------------------------------------------ */

#if defined(PICO_BUILD)
static bool drdy_is_set(void)
{
  return gpio_get(MAG_DRDY_PIN) != 0;
}
#else
extern int gpio_get(unsigned int pin);
static bool drdy_is_set(void)
{
  return gpio_get(MAG_DRDY_PIN) != 0;
}
#endif

/* ------------------------------------------------------------------ */
/* 24-bit signed conversion                                            */
/* ------------------------------------------------------------------ */

static int32_t raw24_to_int32(const uint8_t *b)
{
  uint32_t u = ((uint32_t)b[0] << 16) | ((uint32_t)b[1] << 8) | (uint32_t)b[2];
  if (u & 0x800000u)
  {
    u |= 0xFF000000u; /* sign-extend */
  }
  return (int32_t)u;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

bool rm3100_init(void)
{
  spi_payload_init(); /* idempotent */

#if defined(PICO_BUILD)
  gpio_init(MAG_DRDY_PIN);
  gpio_set_dir(MAG_DRDY_PIN, GPIO_IN);
#endif

  /* Verify sensor connectivity via REVID register */
  uint8_t revid = 0;
  if (!rm3100_spi_read(RM3100_REG_REVID, &revid, 1))
  {
    return false;
  }
  return (revid == RM3100_REVID_EXPECTED);
}

bool rm3100_config_cmm(uint16_t cycle_count)
{
  /* Write cycle count to all three axes (2 bytes each, big-endian) */
  uint8_t cc_buf[6];
  cc_buf[0] = (uint8_t)(cycle_count >> 8);
  cc_buf[1] = (uint8_t)(cycle_count & 0xFFu);
  cc_buf[2] = cc_buf[0];
  cc_buf[3] = cc_buf[1];
  cc_buf[4] = cc_buf[0];
  cc_buf[5] = cc_buf[1];
  if (!rm3100_spi_write(RM3100_REG_CCXY, cc_buf, 6))
  {
    return false;
  }

  /* Enable CMM on all axes: CMX | CMY | CMZ | START = 0x71 */
  uint8_t cmm = 0x71u;
  return rm3100_spi_write(RM3100_REG_CMM, &cmm, 1);
}

bool rm3100_read_vector(rm3100_vector_t *vec, uint32_t timeout_ms)
{
  if (!vec)
  {
    return false;
  }

  /* Poll DRDY — simple busy-wait loop (1 ms granularity) */
  uint32_t elapsed = 0;
  while (!drdy_is_set())
  {
    if (timeout_ms == 0 || elapsed >= timeout_ms)
    {
      return false; /* data not ready */
    }
#if defined(PICO_BUILD)
    sleep_ms(1);
#endif
    elapsed++;
  }

  uint8_t raw[9];
  if (!rm3100_spi_read(RM3100_REG_MX, raw, 9))
  {
    return false;
  }

  vec->x_nT = (float)raw24_to_int32(&raw[0]) * RM3100_SCALE_NT_PER_LSB;
  vec->y_nT = (float)raw24_to_int32(&raw[3]) * RM3100_SCALE_NT_PER_LSB;
  vec->z_nT = (float)raw24_to_int32(&raw[6]) * RM3100_SCALE_NT_PER_LSB;

  s_last_vec = *vec;
  return true;
}

void rm3100_get_last(rm3100_vector_t *vec)
{
  if (vec)
  {
    *vec = s_last_vec;
  }
}
