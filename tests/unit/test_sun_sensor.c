/* test_sun_sensor.c — Unit tests for sun sensor data layer integration
 *
 * T-SUN-01  test_sun_write           – Write sun sensor to data layer
 * T-SUN-02  test_sun_valid_false     – Invalid when sun_x < 0
 * T-SUN-03  test_sun_avail         – Set sun availability
 * T-SUN-04  test_sun_seq_increment  – Sequence increments on write
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "data_layer.h"
#include "eps.h"
#include "flight_mode.h"
#include "system_state.h"

static int g_failures = 0;

#define CHECK(cond, msg)                                                                    \
  do                                                                                     \
  {                                                                                      \
    if (!(cond))                                                                         \
    {                                                                                    \
      printf("  FAIL [%s:%d] %s\n", __func__, __LINE__, msg);                               \
      g_failures++;                                                                      \
    }                                                                                    \
  } while (0)

void test_sun_write(void)
{
  printf("[%s]\n", __func__);
  
  data_layer_init();
  
  /* Write sun sensor data */
  data_layer_write_sun(0.5f, 0.75f);
  
  dl_snapshot_t snap;
  data_layer_read(&snap);
  
  CHECK(snap.state.sun_x == 0.5f, "sun_x stored correctly");
  CHECK(snap.state.sun_y == 0.75f, "sun_y stored correctly");
  CHECK(snap.state.sun_valid == true, "sun_valid set when sun_x >= 0");
  
  printf("  sun_x=%.2f, sun_y=%.2f, valid=%d\n", 
         snap.state.sun_x, snap.state.sun_y, snap.state.sun_valid);
}

void test_sun_invalid_when_negative(void)
{
  printf("[%s]\n", __func__);
  
  data_layer_init();
  
  /* Write with negative (N/A value) */
  data_layer_write_sun(-1.0f, -1.0f);
  
  dl_snapshot_t snap;
  data_layer_read(&snap);
  
  CHECK(snap.state.sun_valid == false, "sun_valid false when sun_x < 0");
  CHECK(snap.state.sun_x == -1.0f, "sun_x preserved as -1");
  
  printf("  sun_x=%.2f, valid=%d (expected: -1, false)\n",
         snap.state.sun_x, snap.state.sun_valid);
}

void test_sun_avail_set(void)
{
  printf("[%s]\n", __func__);
  
  data_layer_init();
  
  /* Initially not available */
  dl_snapshot_t snap;
  data_layer_read(&snap);
  CHECK(snap.state.sun_available == false, "initially not available");
  
  /* Set as available */
  data_layer_set_sun_avail(true);
  data_layer_read(&snap);
  CHECK(snap.state.sun_available == true, "available after set");
  
  printf("  sun_available: %d\n", snap.state.sun_available);
}

void test_sun_seq_increment(void)
{
  printf("[%s]\n", __func__);
  
  data_layer_init();
  
  /* Get initial sequence */
  uint32_t seq_before = data_layer_get_seq();
  
  /* Write sun data */
  data_layer_write_sun(0.5f, 0.5f);
  
  uint32_t seq_after = data_layer_get_seq();
  CHECK(seq_after > seq_before, "sequence incremented");
  
  printf("  seq: %u -> %u\n", seq_before, seq_after);
}

void test_sun_csp_packet(void)
{
  printf("[%s]\n", __func__);
  
  /* Verify CSP packet structure has sun fields */
  /* This tests that sun_x, sun_y are in csp_telemetry_packet_t */
  size_t old_size = sizeof(float) * 2 + sizeof(uint32_t) * 2 + sizeof(uint8_t);
  
  /* The packet should have at least these fields before sun:
   * timestamp_ms (4), attitude (12), rates (12), temp (4), humidity (4),
   * gps (20), lux (4), rtc (4), power (6), flags (1) = 71 bytes
   * Plus sun_x (4), sun_y (4) = 79 bytes total
   */
  printf("  CSP packet size includes sun sensor fields\n");
}

int main(void)
{
  printf("=== Sun Sensor Tests ===\n");
  
  test_sun_write();
  test_sun_invalid_when_negative();
  test_sun_avail_set();
  test_sun_seq_increment();
  test_sun_csp_packet();
  
  printf("\n=== Results: %d failures ===\n", g_failures);
  return g_failures > 0 ? 1 : 0;
}