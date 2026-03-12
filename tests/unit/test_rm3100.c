/**
 * @file test_rm3100.c
 * @brief WP-7.2 unit tests for the RM3100 magnetometer driver.
 *
 * Tests (T-PLD-MAG-01..03):
 *   01 — SPI init succeeds and CMM write is issued
 *   02 — 24-bit signed values are converted correctly to nT
 *   03 — DRDY-low returns false; DRDY-high succeeds
 *
 * Runs on host (no hardware required).  Mocks replace the Pico SDK
 * spi_write_blocking / spi_read_blocking / gpio_get symbols.
 */

#ifdef PICO_BUILD
  #include "unity.h"
#else
  #include "host/unity.h"
#endif
#include "rm3100.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Hardware mocks                                                      */
/* ------------------------------------------------------------------ */

static int      g_spi_write_calls = 0;
static int      g_spi_read_calls  = 0;
static int      g_drdy_state      = 1;  /* 1 = DRDY high (data ready) */

/* Simulated measurement: X = 0x001234, Y = 0xFFEDCB (negative), Z = 0x000001 */
static const uint8_t k_raw_sim[9] = {
  0x00, 0x12, 0x34,   /* X — 0x001234  = 4660  → 4660 * 13 nT = 60 580 nT */
  0xFF, 0xED, 0xCB,   /* Y — sign-ext  = -4661 → -4661 * 13 nT = -60 593 nT */
  0x00, 0x00, 0x01,   /* Z — 0x000001  = 1     → 1 * 13 nT = 13 nT */
};

int spi_write_blocking(void *spi, const uint8_t *src, size_t len)
{
  (void)spi; (void)src;
  g_spi_write_calls++;
  return (int)len;
}

int spi_read_blocking(void *spi, uint8_t filler, uint8_t *dst, size_t len)
{
  (void)spi; (void)filler;
  g_spi_read_calls++;
  if (len == 9)
  {
    for (size_t i = 0; i < 9; i++) { dst[i] = k_raw_sim[i]; }
  }
  else if (len == 1)
  {
    dst[0] = 0x22; /* REVID */
  }
  return (int)len;
}

int gpio_get(unsigned int pin)
{
  (void)pin;
  return g_drdy_state;
}

/* ------------------------------------------------------------------ */
/* Unity boilerplate                                                   */
/* ------------------------------------------------------------------ */

void setUp(void)
{
  g_spi_write_calls = 0;
  g_spi_read_calls  = 0;
  g_drdy_state      = 1;
}
void tearDown(void) {}

/* ------------------------------------------------------------------ */
/* T-PLD-MAG-01: SPI init and CMM register write                      */
/* ------------------------------------------------------------------ */

void test_T_PLD_MAG_01_init_and_cmm(void)
{
  bool ok = rm3100_init();
  TEST_ASSERT_TRUE(ok);

  ok = rm3100_config_cmm(200);
  TEST_ASSERT_TRUE(ok);

  /* At least one SPI write must have been issued for the CMM register */
  TEST_ASSERT_GREATER_THAN(0, g_spi_write_calls);
}

/* ------------------------------------------------------------------ */
/* T-PLD-MAG-02: 24-bit → nT conversion                              */
/* ------------------------------------------------------------------ */

void test_T_PLD_MAG_02_vector_conversion(void)
{
  g_drdy_state = 1;
  rm3100_vector_t vec;
  bool ok = rm3100_read_vector(&vec, 0);
  TEST_ASSERT_TRUE(ok);

  /* Expected values: raw * 13 nT/LSB */
  TEST_ASSERT_FLOAT_WITHIN(1.0f, (float)0x001234 * 13.0f, vec.x_nT);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, (float)(int32_t)0xFFFFEDCB * 13.0f, vec.y_nT);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, (float)0x000001 * 13.0f, vec.z_nT);
}

/* ------------------------------------------------------------------ */
/* T-PLD-MAG-03: DRDY pin logic                                       */
/* ------------------------------------------------------------------ */

void test_T_PLD_MAG_03_drdy_gating(void)
{
  rm3100_vector_t vec;

  g_drdy_state = 0;
  bool ok = rm3100_read_vector(&vec, 0);
  TEST_ASSERT_FALSE(ok);

  g_drdy_state = 1;
  ok = rm3100_read_vector(&vec, 0);
  TEST_ASSERT_TRUE(ok);
}

/* ------------------------------------------------------------------ */
/* Entry point                                                         */
/* ------------------------------------------------------------------ */

int main(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_T_PLD_MAG_01_init_and_cmm);
  RUN_TEST(test_T_PLD_MAG_02_vector_conversion);
  RUN_TEST(test_T_PLD_MAG_03_drdy_gating);
  return UNITY_END();
}
