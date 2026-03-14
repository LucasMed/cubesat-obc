/**
 * @file payload_manager.c
 * @brief Payload subsystem central controller implementation.
 */

#include "payload_manager.h"

#include "pico_pins.h"

/* Drivers */
#include "camera_driver.h"
#include "radiation_driver.h"
#include "rm3100.h"
#include "storage_manager.h"

#if defined(PICO_BUILD)
  #include "hardware/gpio.h"
  #include "pico/time.h"
#endif

#include <string.h>

static payload_status_t s_payload_status;

void payload_manager_init(void)
{
  memset(&s_payload_status, 0, sizeof(s_payload_status));

#if defined(PICO_BUILD)
  /* Configure power rail pin */
  gpio_init(PAYLOAD_ENABLE_PIN);
  gpio_set_dir(PAYLOAD_ENABLE_PIN, GPIO_OUT);
  gpio_put(PAYLOAD_ENABLE_PIN, 0); /* Start disabled */
#endif

  /* Storage is essential for logging */
  storage_init();
}

bool payload_manager_enable(bool enable)
{
#if defined(PICO_BUILD)
  gpio_put(PAYLOAD_ENABLE_PIN, enable ? 1 : 0);
#else
  /* Mock call for host testing */
  void gpio_put(unsigned int pin, int value);
  gpio_put(PAYLOAD_ENABLE_PIN, enable ? 1 : 0);
#endif

  if (enable)
  {
#if defined(PICO_BUILD)
    /* Allow power to stabilize before init */
    sleep_ms(50);
#endif

    /* Re-init devices if rail was just enabled */
    rm3100_init();
    camera_init();
    radiation_driver_init();
  }

  s_payload_status.rail_enabled = enable;
  return true;
}

bool payload_manager_health_check(void)
{
  /* Basic health: connectivity test */
  bool mag_ok = rm3100_init();
  bool cam_ok = camera_init();

  return mag_ok && cam_ok;
}

payload_status_t payload_manager_get_status(void)
{
  /* Fetch latest vector for status */
  rm3100_get_last((rm3100_vector_t *)s_payload_status.last_mag_vector);

  /* Estimate image count from storage if needed (placeholder) */
  s_payload_status.image_count = 0;

  return s_payload_status;
}
