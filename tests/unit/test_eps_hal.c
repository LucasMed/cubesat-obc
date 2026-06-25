/**
 * @file test_eps_hal.c
 * @brief Unit tests for eps_hal.c host fallback.
 *
 * The host stub returns fixed values: Vbatt = 7.6 V, Ibatt = 0.5 A,
 * Temp = 25 °C.  This test is compiled WITHOUT a strong-symbol override
 * so it exercises the real eps_hal_read() from eps_hal.c.
 *
 * Must NOT be linked against eps_lib (which contains eps_monitor.c's
 * weak eps_hal_read) or test_eps_monitor (which provides a strong
 * override).  Instead, compile eps_hal.c directly.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef PICO_BUILD
  #include "unity.h"
#else
  #include "host/unity.h"
#endif

/* Function under test — forward-declared in eps_hal.c */
bool eps_hal_read(float *vbatt, float *ibatt, float *temp);

void setUp(void) {}
void tearDown(void) {}

void test_eps_hal_read_returns_expected_values(void)
{
  float vbatt = 0.0f;
  float ibatt = 0.0f;
  float temp  = 0.0f;

  bool ok = eps_hal_read(&vbatt, &ibatt, &temp);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 7.6f, vbatt);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, ibatt);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 25.0f, temp);
}

void test_eps_hal_read_null_vbatt(void)
{
  float ibatt = 0.0f;
  float temp  = 0.0f;
  bool ok = eps_hal_read(NULL, &ibatt, &temp);
  TEST_ASSERT_TRUE(ok);
}

void test_eps_hal_read_null_ibatt(void)
{
  float vbatt = 0.0f;
  float temp  = 0.0f;
  bool ok = eps_hal_read(&vbatt, NULL, &temp);
  TEST_ASSERT_TRUE(ok);
}

void test_eps_hal_read_null_temp(void)
{
  float vbatt = 0.0f;
  float ibatt = 0.0f;
  bool ok = eps_hal_read(&vbatt, &ibatt, NULL);
  TEST_ASSERT_TRUE(ok);
}

void test_eps_hal_read_all_null(void)
{
  bool ok = eps_hal_read(NULL, NULL, NULL);
  TEST_ASSERT_TRUE(ok);
}

int main(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_eps_hal_read_returns_expected_values);
  RUN_TEST(test_eps_hal_read_null_vbatt);
  RUN_TEST(test_eps_hal_read_null_ibatt);
  RUN_TEST(test_eps_hal_read_null_temp);
  RUN_TEST(test_eps_hal_read_all_null);
  return UNITY_END();
}
