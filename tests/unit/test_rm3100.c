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
static bool     s_spi_fail        = false;  /* force SPI read/write to fail */
static uint8_t  s_revid_return    = 0x22;   /* mock REVID value */

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
  if (s_spi_fail) return 0;
  return (int)len;
}

int spi_read_blocking(void *spi, uint8_t filler, uint8_t *dst, size_t len)
{
  (void)spi; (void)filler;
  g_spi_read_calls++;
  if (s_spi_fail) return 0;
  if (len == 9)
  {
    for (size_t i = 0; i < 9; i++) { dst[i] = k_raw_sim[i]; }
  }
  else if (len == 1)
  {
    dst[0] = s_revid_return; /* REVID — controllable for test */
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
  s_spi_fail        = false;
  s_revid_return    = 0x22;
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

/* ================================================================== */
/* T-PLD-MAG-04: rm3100_get_last returns last read vector              */
/* ================================================================== */

void test_T_PLD_MAG_04_get_last(void)
{
  rm3100_vector_t vec, last;

  /* First read a vector (DRDY=1, timeout=0 → immediate read) */
  g_drdy_state = 1;
  bool ok = rm3100_read_vector(&vec, 0);
  TEST_ASSERT_TRUE(ok);

  /* get_last should return the same values */
  rm3100_get_last(&last);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, vec.x_nT, last.x_nT);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, vec.y_nT, last.y_nT);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, vec.z_nT, last.z_nT);
}

void test_T_PLD_MAG_05_get_last_null_ptr(void)
{
  /* Must not crash */
  rm3100_get_last(NULL);
}

/* ================================================================== */
/* T-PLD-MAG-06a: init fails on SPI read failure (L149)               */
/* ================================================================== */

void test_T_PLD_MAG_06a_init_spi_fail(void)
{
  /* Force SPI read to fail before REVID check — hits L149 */
  s_spi_fail = true;
  bool ok = rm3100_init();
  TEST_ASSERT_FALSE(ok);
}

/* ================================================================== */
/* T-PLD-MAG-06b: init fails on REVID mismatch (not L149, but tests   */
/*                the revid != EXPECTED return path)                   */
/* ================================================================== */

void test_T_PLD_MAG_06b_init_revid_mismatch(void)
{
  /* Make the REVID register return an unexpected value */
  s_revid_return = 0xFF;
  bool ok = rm3100_init();
  TEST_ASSERT_FALSE(ok);
}

/* ================================================================== */
/* T-PLD-MAG-07: config_cmm fails when SPI write fails (L166)         */
/* ================================================================== */

void test_T_PLD_MAG_07_config_cmm_write_fail(void)
{
  bool ok = rm3100_init();
  TEST_ASSERT_TRUE(ok);

  s_spi_fail = true;
  ok = rm3100_config_cmm(200);
  TEST_ASSERT_FALSE(ok);
}

/* ================================================================== */
/* T-PLD-MAG-08: read_vector with NULL vec returns false (L178)       */
/* ================================================================== */

void test_T_PLD_MAG_08_read_vector_null(void)
{
  bool ok = rm3100_read_vector(NULL, 0);
  TEST_ASSERT_FALSE(ok);
}

/* ================================================================== */
/* T-PLD-MAG-09: read_vector loops on DRDY poll (L192)                */
/* ================================================================== */

void test_T_PLD_MAG_09_drdy_poll_loop(void)
{
  g_drdy_state = 0;  /* DRDY low — read will busy-wait then time out */
  rm3100_vector_t vec;

  /* timeout_ms=5: loop runs L192 (elapsed++) until elapsed >= 5 */
  bool ok = rm3100_read_vector(&vec, 5);
  TEST_ASSERT_FALSE(ok);
}

/* ================================================================== */
/* T-PLD-MAG-10: read_vector SPI read failure (L198)                  */
/* ================================================================== */

void test_T_PLD_MAG_10_read_vector_spi_fail(void)
{
  /* Ensure DRDY is high so we pass the poll loop */
  g_drdy_state = 1;

  rm3100_vector_t vec;
  s_spi_fail = true;
  bool ok = rm3100_read_vector(&vec, 0);
  TEST_ASSERT_FALSE(ok);
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
  RUN_TEST(test_T_PLD_MAG_04_get_last);
  RUN_TEST(test_T_PLD_MAG_05_get_last_null_ptr);
  RUN_TEST(test_T_PLD_MAG_06a_init_spi_fail);
  RUN_TEST(test_T_PLD_MAG_06b_init_revid_mismatch);
  RUN_TEST(test_T_PLD_MAG_07_config_cmm_write_fail);
  RUN_TEST(test_T_PLD_MAG_08_read_vector_null);
  RUN_TEST(test_T_PLD_MAG_09_drdy_poll_loop);
  RUN_TEST(test_T_PLD_MAG_10_read_vector_spi_fail);
  return UNITY_END();
}
