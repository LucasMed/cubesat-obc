/**
 * @file test_payload_manager.c
 * @brief Unit tests for the payload manager.
 */

#ifdef PICO_BUILD
  #include "unity.h"
#else
  #include "host/unity.h"
#endif

#include "payload_manager.h"
#include "rm3100.h"
#include "camera_driver.h"
#include "radiation_driver.h"
#include "storage_manager.h"

#include <stdbool.h>
#include <string.h>

/* Mock globals */
static bool g_rm3100_init_called = false;
static bool g_camera_init_called = false;
static bool g_rail_enabled = false;

/* Hardware mocks for host-based unit tests */
bool rm3100_init(void) { g_rm3100_init_called = true; return true; }
bool camera_init(void) { g_camera_init_called = true; return true; }
bool radiation_driver_init(void) { return true; }
float radiation_driver_read_dose(void) { return 1.23f; }
storage_status_t storage_init(void) { return STORAGE_OK; }
void rm3100_get_last(rm3100_vector_t *vec) { memset(vec, 0, sizeof(*vec)); }

#ifndef PICO_BUILD
void gpio_put(unsigned int pin, int value) { (void)pin; g_rail_enabled = (value != 0); }
void gpio_init(unsigned int pin) { (void)pin; }
void gpio_set_dir(unsigned int pin, bool out) { (void)pin; (void)out; }
void sleep_ms(uint32_t ms) { (void)ms; }
#endif

void setUp(void)
{
  g_rm3100_init_called = false;
  g_camera_init_called = false;
  g_rail_enabled = false;
  payload_manager_init();
}

void tearDown(void) {}

void test_payload_manager_power_control(void)
{
  payload_manager_enable(true);
  TEST_ASSERT_TRUE(g_rail_enabled);
  TEST_ASSERT_TRUE(g_rm3100_init_called);
  TEST_ASSERT_TRUE(g_camera_init_called);
  
  payload_status_t status = payload_manager_get_status();
  TEST_ASSERT_TRUE(status.rail_enabled);
}

void test_payload_manager_health(void)
{
  /* Enable rail first */
  payload_manager_enable(true);
  
  bool ok = payload_manager_health_check();
  TEST_ASSERT_TRUE(ok);
}

int main(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_payload_manager_power_control);
  RUN_TEST(test_payload_manager_health);
  return UNITY_END();
}
