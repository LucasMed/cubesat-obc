/* test_system_state.c — Unit tests for system_state.c (DLA shim)
 *
 * T-SYS-01  system_state_init calls data_layer_init (no crash)
 * T-SYS-02  system_state_set_available writes sensor availability
 * T-SYS-03  system_state_set_imu writes attitude/rates via DLA
 * T-SYS-04  system_state_set_temp writes temperature via DLA
 * T-SYS-05  system_state_get returns current snapshot
 * T-SYS-06  system_state_get with NULL returns safely
 */

#include "data_layer.h"
#include "flight_mode.h"
#include "system_state.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ---- Mock data layer state ---- */
static dl_snapshot_t g_snap;

/* Replace data_layer functions with tracking stubs.
 * We need to support the functions that system_state.c calls. */

static int g_init_calls = 0;
static int g_sensor_avail_calls = 0;
static bool g_last_imu_avail = false;
static bool g_last_temp_avail = false;
static int g_write_imu_calls = 0;
static float g_last_att[3] = {0};
static float g_last_rates[3] = {0};
static int g_write_temp_calls = 0;
static float g_last_temp_val = 0;
static int g_read_calls = 0;

void data_layer_init(void)
{
  g_init_calls++;
}

void data_layer_set_sensor_avail(bool imu, bool temp)
{
  g_sensor_avail_calls++;
  g_last_imu_avail = imu;
  g_last_temp_avail = temp;
}

void data_layer_write_imu(const float att[3], const float rates[3])
{
  g_write_imu_calls++;
  if (att)
  {
    g_last_att[0] = att[0];
    g_last_att[1] = att[1];
    g_last_att[2] = att[2];
  }
  if (rates)
  {
    g_last_rates[0] = rates[0];
    g_last_rates[1] = rates[1];
    g_last_rates[2] = rates[2];
  }
}

void data_layer_write_temp(float temp)
{
  g_write_temp_calls++;
  g_last_temp_val = temp;
}

void data_layer_read(dl_snapshot_t *snap)
{
  g_read_calls++;
  if (snap)
    *snap = g_snap;
}

/* ---- Test helpers ---- */

static int g_failures = 0;

#define CHECK(cond, msg) \
  do \
  { \
    if (!(cond)) \
    { \
      printf("  FAIL [%s:%d] %s\n", __func__, __LINE__, msg); \
      g_failures++; \
    } \
  } while (0)

#define FPEQ(a, b) (fabsf((a) - (b)) < 1e-5f)

static void reset_all(void)
{
  g_init_calls = 0;
  g_sensor_avail_calls = 0;
  g_last_imu_avail = false;
  g_last_temp_avail = false;
  g_write_imu_calls = 0;
  g_last_att[0] = g_last_att[1] = g_last_att[2] = 0.0f;
  g_last_rates[0] = g_last_rates[1] = g_last_rates[2] = 0.0f;
  g_write_temp_calls = 0;
  g_last_temp_val = 0.0f;
  g_read_calls = 0;
  memset(&g_snap, 0, sizeof(g_snap));
}

/* ========================================================================
 * T-SYS-01  system_state_init calls data_layer_init
 * ======================================================================== */
static void test_sys_init_calls_dla_init(void)
{
  int failures_before = g_failures;
  reset_all();

  system_state_init();

  CHECK(g_init_calls == 1, "data_layer_init called once");

  printf("[T-SYS-01] test_sys_init_calls_dla_init: %s\n",
         g_failures == failures_before ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-SYS-02  system_state_set_available writes sensor availability
 * ======================================================================== */
static void test_sys_set_available(void)
{
  int failures_before = g_failures;
  reset_all();

  system_state_set_available(true, false);

  CHECK(g_sensor_avail_calls == 1, "data_layer_set_sensor_avail called once");
  CHECK(g_last_imu_avail == true, "imu availability set to true");
  CHECK(g_last_temp_avail == false, "temp availability set to false");

  printf("[T-SYS-02] test_sys_set_available: %s\n",
         g_failures == failures_before ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-SYS-03  system_state_set_imu writes attitude/rates via DLA
 * ======================================================================== */
static void test_sys_set_imu(void)
{
  int failures_before = g_failures;
  reset_all();

  float att[3] = {0.1f, -0.2f, 1.5f};
  float rates[3] = {0.01f, 0.02f, -0.005f};
  system_state_set_imu(att, rates);

  CHECK(g_write_imu_calls == 1, "data_layer_write_imu called once");
  CHECK(FPEQ(g_last_att[0], 0.1f), "att[0] forwarded correctly");
  CHECK(FPEQ(g_last_att[1], -0.2f), "att[1] forwarded correctly");
  CHECK(FPEQ(g_last_att[2], 1.5f), "att[2] forwarded correctly");
  CHECK(FPEQ(g_last_rates[0], 0.01f), "rates[0] forwarded correctly");
  CHECK(FPEQ(g_last_rates[1], 0.02f), "rates[1] forwarded correctly");
  CHECK(FPEQ(g_last_rates[2], -0.005f), "rates[2] forwarded correctly");

  printf("[T-SYS-03] test_sys_set_imu: %s\n",
         g_failures == failures_before ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-SYS-04  system_state_set_temp writes temperature
 * ======================================================================== */
static void test_sys_set_temp(void)
{
  int failures_before = g_failures;
  reset_all();

  system_state_set_temp(25.5f);

  CHECK(g_write_temp_calls == 1, "data_layer_write_temp called once");
  CHECK(FPEQ(g_last_temp_val, 25.5f), "temperature forwarded correctly");

  printf("[T-SYS-04] test_sys_set_temp: %s\n",
         g_failures == failures_before ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-SYS-05  system_state_get returns current snapshot
 * ======================================================================== */
static void test_sys_get_returns_snapshot(void)
{
  int failures_before = g_failures;
  reset_all();

  /* Set up known state in the mock DLA */
  g_snap.state.attitude[0] = 0.5f;
  g_snap.state.attitude[1] = -0.3f;
  g_snap.state.attitude[2] = 1.2f;
  g_snap.mode = FM_NOMINAL;

  system_state_t out;
  memset(&out, 0xFF, sizeof(out));
  system_state_get(&out);

  CHECK(g_read_calls == 1, "data_layer_read called once");
  CHECK(FPEQ(out.attitude[0], 0.5f), "attitude[0] matches DLA snapshot");
  CHECK(FPEQ(out.attitude[1], -0.3f), "attitude[1] matches");

  printf("[T-SYS-05] test_sys_get_returns_snapshot: %s\n",
         g_failures == failures_before ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-SYS-06  system_state_get with NULL returns safely
 * ======================================================================== */
static void test_sys_get_null_safe(void)
{
  int failures_before = g_failures;
  reset_all();

  /* Should not crash */
  system_state_get(NULL);

  CHECK(g_read_calls == 0, "data_layer_read NOT called when out_state is NULL");

  printf("[T-SYS-06] test_sys_get_null_safe: %s\n",
         g_failures == failures_before ? "PASS" : "FAIL");
}

/* ======================================================================== */
int main(void)
{
  printf("=== System State Unit Tests ===\n");
  test_sys_init_calls_dla_init();
  test_sys_set_available();
  test_sys_set_imu();
  test_sys_set_temp();
  test_sys_get_returns_snapshot();
  test_sys_get_null_safe();
  printf("==============================\n");
  if (g_failures == 0)
  {
    printf("All tests passed!\n");
    return 0;
  }
  printf("%d test(s) FAILED.\n", g_failures);
  return 1;
}
