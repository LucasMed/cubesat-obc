/**
 * @file test_radiation_driver.c
 * @brief WP-7.3 unit tests for the radiation detector driver.
 *
 * Tests (T-PLD-RAD-01..02):
 *   01 — ADC read, voltage conversion, and threshold detection
 *   02 — Event accumulation and dose counter; get_and_reset clears state
 *
 * Runs on host — adc_read() is provided as a mock below.
 */

#ifdef PICO_BUILD
  #include "unity.h"
#else
  #include "host/unity.h"
#endif
#include "radiation_driver.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* ADC mock                                                            */
/* ------------------------------------------------------------------ */

static uint16_t g_mock_adc_value = 0;

uint16_t adc_read(void)
{
  return g_mock_adc_value;
}

/* ------------------------------------------------------------------ */
/* Unity boilerplate                                                   */
/* ------------------------------------------------------------------ */

void setUp(void)
{
  g_mock_adc_value = 0;
  radiation_init();
}
void tearDown(void) {}

/* ------------------------------------------------------------------ */
/* T-PLD-RAD-01: ADC conversion and threshold detection               */
/* ------------------------------------------------------------------ */

void test_T_PLD_RAD_01_conversion_and_threshold(void)
{
  rad_sample_t s;

  /* Below threshold — 5% of full scale (204 counts) */
  g_mock_adc_value = 204;
  bool ok = radiation_read(&s);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_FALSE(s.threshold);
  /* Voltage should be approximately 0.164 V (204/4095 * 3.3) */
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.164f, s.voltage_V);

  /* Above threshold — 20% of full scale (819 counts) */
  g_mock_adc_value = 819;
  ok = radiation_read(&s);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_TRUE(s.threshold);
  /* Voltage should be approximately 0.66 V (819/4095 * 3.3) */
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.66f, s.voltage_V);

  /* Full scale */
  g_mock_adc_value = 4095;
  ok = radiation_read(&s);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.3f, s.voltage_V);

  /* NULL pointer guard */
  ok = radiation_read(NULL);
  TEST_ASSERT_FALSE(ok);
}

/* ------------------------------------------------------------------ */
/* T-PLD-RAD-02: Event accumulation and reset                         */
/* ------------------------------------------------------------------ */

void test_T_PLD_RAD_02_accumulation_and_reset(void)
{
  rad_sample_t s;
  rad_accumulator_t acc;

  /* Simulate 3 events above threshold */
  g_mock_adc_value = 2048; /* well above 10% threshold */
  for (int i = 0; i < 3; i++)
  {
    radiation_read(&s);
    radiation_accumulate(&s);
  }

  /* One sample below threshold — should NOT count */
  g_mock_adc_value = 50;
  radiation_read(&s);
  radiation_accumulate(&s);

  /* Retrieve and verify */
  radiation_get_and_reset(&acc);
  TEST_ASSERT_FLOAT_WITHIN(0, 3, (float)acc.event_count);
  TEST_ASSERT_FLOAT_WITHIN(1e-9f, 3.0f * 1e-6f, acc.dose_Gy);

  /* After reset, accumulator must be zero */
  radiation_get_and_reset(&acc);
  TEST_ASSERT_FLOAT_WITHIN(0, 0, (float)acc.event_count);
  TEST_ASSERT_FLOAT_WITHIN(1e-9f, 0.0f, acc.dose_Gy);
}

/* ================================================================== */
/* T-PLD-RAD-03: radiation_driver_init delegates to radiation_init     */
/* ================================================================== */

void test_T_PLD_RAD_03_driver_init(void)
{
  /* Reset and verify init succeeds */
  setUp();
  TEST_ASSERT_TRUE(radiation_driver_init());
}

/* ================================================================== */
/* T-PLD-RAD-04: radiation_driver_read_dose returns current dose       */
/* ================================================================== */

void test_T_PLD_RAD_04_read_dose(void)
{
  rad_sample_t s;

  /* Accumulate a few events */
  for (int i = 0; i < 5; i++)
  {
    g_mock_adc_value = 3500; /* above threshold */
    radiation_read(&s);
    radiation_accumulate(&s);
  }

  float dose = radiation_driver_read_dose();
  TEST_ASSERT_TRUE(dose > 0.0f);
}

/* ------------------------------------------------------------------ */
/* Entry point                                                         */
/* ------------------------------------------------------------------ */

int main(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_T_PLD_RAD_01_conversion_and_threshold);
  RUN_TEST(test_T_PLD_RAD_02_accumulation_and_reset);
  RUN_TEST(test_T_PLD_RAD_03_driver_init);
  RUN_TEST(test_T_PLD_RAD_04_read_dose);
  return UNITY_END();
}
