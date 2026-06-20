/**
 * @file test_watchdog_hal_host.c
 * @brief Unit tests for watchdog_hal_host.c host stubs.
 *
 * The host stubs are no-ops — this test verifies they exist and can be
 * called without crash.
 *
 * NOTE: test_watchdog.c provides its own strong-symbol overrides for
 * watchdog_hal_*, so this separate test is needed to exercise the real
 * host implementations in watchdog_hal_host.c.
 */

#include "watchdog_hal.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef PICO_BUILD
  #include "unity.h"
#else
  #include "host/unity.h"
#endif

void setUp(void) {}
void tearDown(void) {}

void test_watchdog_hal_init_does_not_crash(void)
{
  /* Must accept any timeout value without crashing */
  watchdog_hal_init(0);
  watchdog_hal_init(1000);
  watchdog_hal_init(0xFFFFFFFF);
}

void test_watchdog_hal_feed_does_not_crash(void)
{
  watchdog_hal_feed();
  /* Can be called multiple times */
  watchdog_hal_feed();
  watchdog_hal_feed();
}

void test_watchdog_hal_triggered_returns_false(void)
{
  /* On host there is no watchdog, so triggered must always be false */
  TEST_ASSERT_FALSE(watchdog_hal_triggered());
}

void test_watchdog_hal_clear_triggered_does_not_crash(void)
{
  watchdog_hal_clear_triggered();
  /* Can be called multiple times */
  watchdog_hal_clear_triggered();
}

int main(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_watchdog_hal_init_does_not_crash);
  RUN_TEST(test_watchdog_hal_feed_does_not_crash);
  RUN_TEST(test_watchdog_hal_triggered_returns_false);
  RUN_TEST(test_watchdog_hal_clear_triggered_does_not_crash);
  return UNITY_END();
}
