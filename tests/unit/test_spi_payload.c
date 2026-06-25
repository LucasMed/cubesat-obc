/**
 * @file test_spi_payload.c
 * @brief Unit tests for spi_payload.c host stubs.
 *
 * On host, spi_payload_init, cs_select, and cs_deselect are trivial
 * no-ops.  This test exercises all three to cover the host-mode code
 * paths.
 *
 * NOTE: Many payload tests (rm3100, camera, w25q64) compile spi_payload.c
 * but the driver's SPI helper functions use host-extern mocks that bypass
 * cs_select/cs_deselect, so those functions are never reached.
 */

#include "spi_payload.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef PICO_BUILD
  #include "unity.h"
#else
  #include "host/unity.h"
#endif

void setUp(void)
{
  /* Force a clean state: reverse any previous init by marking not-initialised.
   * We cannot directly reset the static bool, so we rely on the fact that
   * a fresh compilation unit starts uninitialised.  Since each test shares
   * the same compilation unit, we call init once in setUp and track state
   * through the function's return value. */
}

void tearDown(void) {}

void test_spi_payload_init_first_call_returns_true(void)
{
  /* First call should succeed */
  bool ok = spi_payload_init();
  TEST_ASSERT_TRUE(ok);
}

void test_spi_payload_init_second_call_returns_false(void)
{
  /* After first init, subsequent calls return false (already done) */
  spi_payload_init(); /* first */
  bool ok = spi_payload_init(); /* second */
  TEST_ASSERT_FALSE(ok);
}

void test_spi_payload_cs_select_does_not_crash(void)
{
  /* On host this is a no-op */
  spi_payload_cs_select(0);
  spi_payload_cs_select(1);
  spi_payload_cs_select(0xFFFFFFFF);
}

void test_spi_payload_cs_deselect_does_not_crash(void)
{
  /* On host this is a no-op */
  spi_payload_cs_deselect(0);
  spi_payload_cs_deselect(1);
  spi_payload_cs_deselect(0xFFFFFFFF);
}

void test_spi_payload_init_then_cs_select_deselect(void)
{
  /* Sanity check: init then select/deselect should not crash */
  spi_payload_init();
  spi_payload_cs_select(5);
  spi_payload_cs_deselect(5);
}

int main(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_spi_payload_init_first_call_returns_true);
  RUN_TEST(test_spi_payload_init_second_call_returns_false);
  RUN_TEST(test_spi_payload_cs_select_does_not_crash);
  RUN_TEST(test_spi_payload_cs_deselect_does_not_crash);
  RUN_TEST(test_spi_payload_init_then_cs_select_deselect);
  return UNITY_END();
}
