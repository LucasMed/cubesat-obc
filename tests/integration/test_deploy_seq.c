/**
 * @file test_deploy_seq.c
 * @brief Integration tests for the autonomous deploy sequence.
 *
 * T-DEPLOY-01  Boot → Detumble → Nominal (happy path)
 * T-DEPLOY-02  POST fail → no auto-deploy → ground override via DEPLOY
 *
 * Real code compiled:
 *   deploy_monitor.c, flight_mode_manager.c, post.c,
 *   data_layer.c, system_state.c, quaternion.c
 *
 * Stubs provided: log_event, fault_report, w25q64_*, xTaskGetTickCount,
 * sensor driver read functions, gps_get_last_fix
 *
 * Spec ref: FMM-SPEC-030–033, FMM-SPEC-040–043, FMM-SPEC-080–083,
 *           FMM-SPEC-096, FMM-SPEC-052, FMM-SPEC-053
 */

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ---- FreeRTOS host stubs ---- */
#include "FreeRTOS.h"
#include "task.h"

/* ---- Test helpers ---- */
static int g_failures = 0;

#define CHECK(cond, msg)                                                                           \
  do                                                                                               \
  {                                                                                                \
    if (!(cond))                                                                                   \
    {                                                                                              \
      printf("  FAIL [%s:%d] %s\n", __func__, __LINE__, (msg));                                    \
      g_failures++;                                                                                \
    }                                                                                              \
  } while (0)

/* ---- NOTE on xTaskGetTickCount in host builds ----
 * include/host/FreeRTOS.h defines xTaskGetTickCount() as a macro that
 * always returns 0.  This macro applies to ALL translation units, not
 * just this file.  We cannot override it with a mock function.
 *
 * Instead, we exploit unsigned wraparound: set mode_entry_tick to
 * UINT32_MAX - (DEPLOY_BOOT_SETTLE_MS / portTICK_PERIOD_MS) + 1
 * so that (0 - mode_entry_tick) * 1 == DEPLOY_BOOT_SETTLE_MS.
 */

/* ---- Headers needed by compiled code ---- */
/* These MUST come before the stubs that reference their types. */
#include "data_layer.h"
#include "deploy_monitor.h"
#include "fault_ids.h"
#include "flash_layout.h"
#include "flight_mode.h"
#include "gps_driver.h"
#include "post.h"
#include "w25q64.h"

/* ---- Sensor driver stubs (all return success) ---- */
int mpu6050_read(float *roll, float *pitch, float *yaw)
{
  *roll = 0.0f; *pitch = 0.0f; *yaw = 0.0f;
  return 0;
}

int hmc5883l_read(float field_uT[3])
{
  field_uT[0] = field_uT[1] = field_uT[2] = 0.0f;
  return 0;
}

bool sht31_read(float *temp, float *hum)
{
  *temp = 25.0f; *hum = 50.0f;
  return true;
}

bool bh1750_read(float *lux)
{
  *lux = 300.0f;
  return true;
}

bool ds3231_init(void) { return true; }
bool ds3231_read_time(uint16_t *year, uint8_t *month, uint8_t *day, uint8_t *hour,
                      uint8_t *minute, uint8_t *second)
{
  *year = 2026; *month = 5; *day = 20;
  *hour = 12; *minute = 0; *second = 0;
  return true;
}
uint32_t ds3231_to_epoch(uint16_t year, uint8_t month, uint8_t day,
                         uint8_t hour, uint8_t minute, uint8_t second)
{
  (void)year; (void)month; (void)day; (void)hour; (void)minute; (void)second;
  return 1716192000u;
}

int16_t ina219_get_voltage_mv(void) { return 3700; }
bool ina219_solar_read_power(void *data)
{
  memset(data, 0, sizeof(uint32_t) * 4);
  return true;
}



/* ---- gps_get_last_fix stub — must match GpsFix_t* signature ---- */
bool gps_get_last_fix(GpsFix_t *out)
{
  memset(out, 0, sizeof(GpsFix_t));
  return true;
}

/* ---- Logger stub ---- */
void log_event(uint16_t event_id, uint8_t log_class, const void *data, uint8_t data_len)
{
  (void)event_id; (void)log_class; (void)data; (void)data_len;
}

/* ---- Fault manager stub ---- */
void fault_report(uint16_t fault_id, uint8_t level)
{
  (void)fault_id; (void)level;
}



/* ========================================================================
 * T-DEPLOY-01  Boot → Detumble → Nominal (happy path)
 *
 * Scenario: POST passes, IMU valid, settle time elapses.
 * deploy_monitor_step() transitions BOOT→DETUMBLE→NOMINAL.
 * ======================================================================== */
static void test_boot_to_detumble_to_nominal(void)
{
  printf("--- T-DEPLOY-01: Boot → Detumble → Nominal ---\n");

  data_layer_init();
  flight_mode_manager_init();           /* sets FM_BOOT */
  deploy_monitor_init();

  /* Inject a valid POST record */
  post_record_t post_rec;
  memset(&post_rec, 0, sizeof(post_rec));
  post_rec.magic = POST_MAGIC;
  post_rec.boot_count = 1;
  post_rec.boot_reason = POST_BOOT_POWER_ON;
  post_rec.test_bitmap = (1u << POST_TEST_IMU) | (1u << POST_TEST_RTC)
                       | (1u << POST_TEST_FLASH);
  post_rec.test_detail = post_rec.test_bitmap;
  data_layer_set_post_last(&post_rec);

  /* Set IMU data (marks imu_valid = true) and settle time to trigger BOOT */
  float att[3] = {0.0f, 0.0f, 0.0f};
  float rates[3] = {0.05f, -0.03f, 0.01f};
  data_layer_write_imu(att, rates);

  /* Reset deploy flag to ensure OI-1 triggers */
  data_layer_set_deploy_in_progress(false);

  /* Use unsigned wraparound so that (xTaskGetTickCount() - mode_entry_tick) * portTICK_PERIOD_MS
   * yields DEPLOY_BOOT_SETTLE_MS.  xTaskGetTickCount() always returns 0 on host.
   * We need: (0u - mode_entry_tick) * 1u >= 5000u
   * So mode_entry_tick = UINT32_MAX + 1 - 5000 = UINT32_MAX - 4999u */
  uint32_t settle_ticks = DEPLOY_BOOT_SETTLE_MS / portTICK_PERIOD_MS;
  data_layer_set_mode_entry_tick(UINT32_MAX - settle_ticks + 1u);

  /* ---- Step 1: BOOT settle elapsed → expect DETUMBLE transition ---- */
  printf("  Step 1: expecting BOOT → DETUMBLE...\n");
  deploy_monitor_step();

  flight_mode_t mode = data_layer_get_flight_mode();
  CHECK(mode == FM_DETUMBLE, "T-DEPLOY-01 step 1: mode must be FM_DETUMBLE");
  CHECK(deploy_is_in_progress(), "T-DEPLOY-01 step 1: deploy_in_progress must be true");

  /* ---- Step 2: Set angular rates below threshold, accumulate stable samples ---- */
  printf("  Step 2: accumulating stable samples for DETUMBLE → NOMINAL...\n");

  /* Write low rates so omega < DEPLOY_DETUMBLE_THRESHOLD (0.05 rad/s) */
  float low_att[3] = {0.0f, 0.0f, 0.0f};
  float low_rates[3] = {0.01f, -0.02f, 0.005f};
  data_layer_write_imu(low_att, low_rates);

  /* Step until stable samples accumulate. We need DEPLOY_STABLE_SAMPLES = 50.
   * Step 55 times to be safe. */
  for (int i = 0; i < 55; i++)
  {
    deploy_monitor_step();
    mode = data_layer_get_flight_mode();
    if (mode == FM_NOMINAL)
    {
      break;
    }
  }

  CHECK(mode == FM_NOMINAL, "T-DEPLOY-01 step 2: mode must be FM_NOMINAL after stable period");
  CHECK(!deploy_is_in_progress(), "T-DEPLOY-01 step 2: deploy_in_progress must be cleared");

  printf("  PASS T-DEPLOY-01\n");
}

/* ========================================================================
 * T-DEPLOY-02  POST fail → no auto-deploy → ground override
 *
 * Scenario: POST critical fail (IMU bit clear).  deploy_monitor must NOT
 * auto-transition BOOT→DETUMBLE.  After manual SAFE, ground DEPLOY command
 * (simulated by calling fmm_request_transition directly) transitions to
 * DETUMBLE.
 * ======================================================================== */
static void test_post_fail_then_ground_override(void)
{
  printf("--- T-DEPLOY-02: POST fail → no auto-deploy → ground override ---\n");

  data_layer_init();
  flight_mode_manager_init();           /* FM_BOOT */
  deploy_monitor_init();

  /* Inject a CRITICAL POST record (IMU bit clear) */
  post_record_t post_rec;
  memset(&post_rec, 0, sizeof(post_rec));
  post_rec.magic = POST_MAGIC;
  post_rec.boot_count = 1;
  post_rec.boot_reason = POST_BOOT_POWER_ON;
  post_rec.test_bitmap = (1u << POST_TEST_RTC) | (1u << POST_TEST_FLASH); /* IMU MISSING */
  post_rec.test_detail = (1u << POST_TEST_IMU) | (1u << POST_TEST_RTC) | (1u << POST_TEST_FLASH);
  data_layer_set_post_last(&post_rec);

  /* IMU data stays unset (imu_valid = false) to reflect failed IMU test */
  data_layer_set_mode_entry_tick(UINT32_MAX - (DEPLOY_BOOT_SETTLE_MS / portTICK_PERIOD_MS) + 1u);
  data_layer_set_deploy_in_progress(false);

  /* ---- Step 1: auto-deploy must NOT trigger ---- */
  printf("  Step 1: verifying auto-deploy is blocked...\n");

  deploy_monitor_step();

  flight_mode_t mode = data_layer_get_flight_mode();
  CHECK(mode == FM_BOOT, "T-DEPLOY-02 step 1: mode must remain FM_BOOT (blocked)");
  CHECK(!deploy_is_in_progress(), "T-DEPLOY-02 step 1: deploy must not be in progress");

  /* ---- Step 2: Manually go to SAFE (simulating fmm_force_safe) ---- */
  printf("  Step 2: simulating ground override via DEPLOY from SAFE...\n");

  flight_mode_manager_init(); /* back to BOOT */
  /* Force to SAFE (simulate the fault chain) */
  fmm_request_transition(FM_SAFE);
  mode = data_layer_get_flight_mode();
  CHECK(mode == FM_SAFE, "T-DEPLOY-02 step 2: mode must be FM_SAFE after force");

  /* ---- Step 3: DEPLOY from SAFE must succeed (ground override) ---- */
  fmm_request_transition(FM_DETUMBLE);
  mode = data_layer_get_flight_mode();
  CHECK(mode == FM_DETUMBLE, "T-DEPLOY-02 step 3: DEPLOY from SAFE must transition to DETUMBLE");

  printf("  PASS T-DEPLOY-02\n");
}

/* ========================================================================
 * main
 * ======================================================================== */
int main(void)
{
  printf("=== Deploy Sequence Integration Tests ===\n\n");

  test_boot_to_detumble_to_nominal();
  printf("\n");
  test_post_fail_then_ground_override();

  printf("\n");
  if (g_failures == 0)
  {
    printf("ALL DEPLOY SEQUENCE TESTS PASSED\n");
    return 0;
  }

  printf("%d TEST(S) FAILED\n", g_failures);
  return 1;
}
