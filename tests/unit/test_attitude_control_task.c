/**
 * @file test_attitude_control_task.c
 * @brief PR-8 gate: Attitude Control Task DLA-migration correctness checks.
 *
 * attitude_ctrl_update() and attitude_dynamics_step() are replaced by
 * instrumented strong-symbol stubs so no control_lib / dynamics_lib is needed.
 *
 * Tests:
 *   1.  Step is skipped in FM_BOOT  (ctrl_update NOT called)
 *   2.  Step is skipped in FM_SAFE  (ctrl_update NOT called)
 *   3.  Step is skipped in FM_DETUMBLE (ctrl_update NOT called)
 *   4.  Step runs in FM_NOMINAL when imu_valid == true
 *   5.  Step runs in FM_DIAGNOSTIC when imu_valid == true
 *   6.  Step is skipped when imu_valid == false even in FM_NOMINAL
 *   7.  attitude_ctrl_update receives attitude and rates from DLA snapshot
 *   8.  attitude_dynamics_step receives the torque output from ctrl_update
 */

#include "../../include/attitude_control.h"
#include "../../include/attitude_control_task.h"
#include "../../include/attitude_dynamics.h"
#include "../../include/data_layer.h"
#include "../../include/flight_mode.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Instrumented control / dynamics stubs                               */
/* ------------------------------------------------------------------ */

static int s_ctrl_calls = 0;
static int s_dyn_calls = 0;
static float s_ctrl_target[3] = {0};
static float s_ctrl_current[3] = {0};
static float s_ctrl_rates[3] = {0};
static float s_ctrl_outputs[3] = {1.0f, 2.0f, 3.0f}; /* fixed output torques */
static float s_dyn_torque[3] = {0};

/* These functions match the real signatures — strong symbols override lib. */

void attitude_ctrl_init(attitude_ctrl_t *ac)
{
  (void)ac;
}

void attitude_ctrl_update(attitude_ctrl_t *ac, const float target[3], const float current[3],
                          const float rates[3], float outputs[3], float dt)
{
  (void)ac;
  (void)dt;
  s_ctrl_calls++;
  memcpy(s_ctrl_target, target, sizeof(s_ctrl_target));
  memcpy(s_ctrl_current, current, sizeof(s_ctrl_current));
  memcpy(s_ctrl_rates, rates, sizeof(s_ctrl_rates));
  /* Return fixed torque vector so dynamics stub can verify it */
  outputs[0] = s_ctrl_outputs[0];
  outputs[1] = s_ctrl_outputs[1];
  outputs[2] = s_ctrl_outputs[2];
}

void attitude_dynamics_init(attitude_dyn_t *ad)
{
  (void)ad;
}

void attitude_dynamics_step(attitude_dyn_t *ad, const float torque[3], float dt)
{
  (void)ad;
  (void)dt;
  s_dyn_calls++;
  memcpy(s_dyn_torque, torque, sizeof(s_dyn_torque));
}

/* ------------------------------------------------------------------ */
/* Test helpers                                                        */
/* ------------------------------------------------------------------ */

static int g_failures = 0;

#define CHECK(cond, msg)                                                                           \
  do                                                                                               \
  {                                                                                                \
    if (!(cond))                                                                                   \
    {                                                                                              \
      printf("FAIL [%s:%d]: %s\n", __FILE__, __LINE__, (msg));                                     \
      g_failures++;                                                                                \
    }                                                                                              \
  } while (0)

#define FPEQ(a, b) (fabsf((a) - (b)) < 1e-6f)

static void reset_stubs(void)
{
  s_ctrl_calls = 0;
  s_dyn_calls = 0;
  memset(s_ctrl_target, 0, sizeof(s_ctrl_target));
  memset(s_ctrl_current, 0, sizeof(s_ctrl_current));
  memset(s_ctrl_rates, 0, sizeof(s_ctrl_rates));
  memset(s_dyn_torque, 0, sizeof(s_dyn_torque));
  s_ctrl_outputs[0] = 1.0f;
  s_ctrl_outputs[1] = 2.0f;
  s_ctrl_outputs[2] = 3.0f;
}

static void set_dla_state(flight_mode_t mode, bool imu_valid, const float att[3],
                          const float rates[3])
{
  data_layer_init();
  data_layer_set_flight_mode(mode);
  if (imu_valid)
  {
    data_layer_write_imu(att, rates);
  }
  else
  {
    data_layer_set_sensor_avail(false, false);
  }
}

/* ------------------------------------------------------------------ */
/* Test 1: FM_BOOT → control skipped                                   */
/* ------------------------------------------------------------------ */

static void test_skip_in_fm_boot(void)
{
  float att[3] = {0};
  float rates[3] = {0};
  reset_stubs();
  set_dla_state(FM_BOOT, true, att, rates);
  vAttitudeControlTask_Step();
  CHECK(s_ctrl_calls == 0, "ctrl_update must NOT be called in FM_BOOT");
  CHECK(s_dyn_calls == 0, "dyn_step must NOT be called in FM_BOOT");
  printf("test_skip_in_fm_boot: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 2: FM_SAFE → control skipped                                   */
/* ------------------------------------------------------------------ */

static void test_skip_in_fm_safe(void)
{
  float att[3] = {0};
  float rates[3] = {0};
  reset_stubs();
  set_dla_state(FM_SAFE, true, att, rates);
  vAttitudeControlTask_Step();
  CHECK(s_ctrl_calls == 0, "ctrl_update must NOT be called in FM_SAFE");
  printf("test_skip_in_fm_safe: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 3: FM_DETUMBLE → control skipped                               */
/* ------------------------------------------------------------------ */

static void test_skip_in_fm_detumble(void)
{
  float att[3] = {0};
  float rates[3] = {0};
  reset_stubs();
  set_dla_state(FM_DETUMBLE, true, att, rates);
  vAttitudeControlTask_Step();
  CHECK(s_ctrl_calls == 0, "ctrl_update must NOT be called in FM_DETUMBLE");
  printf("test_skip_in_fm_detumble: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 4: FM_NOMINAL + imu_valid → control runs                       */
/* ------------------------------------------------------------------ */

static void test_runs_in_fm_nominal(void)
{
  float att[3] = {0.1f, 0.2f, 0.3f};
  float rates[3] = {0.01f, 0.02f, 0.03f};
  reset_stubs();
  set_dla_state(FM_NOMINAL, true, att, rates);
  vAttitudeControlTask_Step();
  CHECK(s_ctrl_calls == 1, "ctrl_update must be called once in FM_NOMINAL");
  CHECK(s_dyn_calls == 1, "dyn_step must be called once in FM_NOMINAL");
  printf("test_runs_in_fm_nominal: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 5: FM_DIAGNOSTIC + imu_valid → control runs                    */
/* ------------------------------------------------------------------ */

static void test_runs_in_fm_diagnostic(void)
{
  float att[3] = {0};
  float rates[3] = {0};
  reset_stubs();
  set_dla_state(FM_DIAGNOSTIC, true, att, rates);
  vAttitudeControlTask_Step();
  CHECK(s_ctrl_calls == 1, "ctrl_update must be called in FM_DIAGNOSTIC");
  printf("test_runs_in_fm_diagnostic: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 6: FM_NOMINAL + imu_valid == false → skipped                   */
/* ------------------------------------------------------------------ */

static void test_skip_when_imu_invalid(void)
{
  float att[3] = {0};
  float rates[3] = {0};
  reset_stubs();
  set_dla_state(FM_NOMINAL, false, att, rates);
  vAttitudeControlTask_Step();
  CHECK(s_ctrl_calls == 0, "ctrl_update must NOT be called when imu_valid is false");
  printf("test_skip_when_imu_invalid: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 7: ctrl_update receives correct DLA attitude & rates           */
/* ------------------------------------------------------------------ */

static void test_ctrl_receives_dla_data(void)
{
  float att[3] = {0.5f, -0.3f, 1.2f};
  float rates[3] = {0.10f, -0.05f, 0.30f};
  reset_stubs();
  set_dla_state(FM_NOMINAL, true, att, rates);
  vAttitudeControlTask_Step();

  CHECK(FPEQ(s_ctrl_current[0], att[0]) && FPEQ(s_ctrl_current[1], att[1]) &&
            FPEQ(s_ctrl_current[2], att[2]),
        "ctrl_update must receive DLA attitude values");

  CHECK(FPEQ(s_ctrl_rates[0], rates[0]) && FPEQ(s_ctrl_rates[1], rates[1]) &&
            FPEQ(s_ctrl_rates[2], rates[2]),
        "ctrl_update must receive DLA rate values");

  /* Target must be zero-vector (nadir-pointing default) */
  CHECK(FPEQ(s_ctrl_target[0], 0.0f) && FPEQ(s_ctrl_target[1], 0.0f) &&
            FPEQ(s_ctrl_target[2], 0.0f),
        "ctrl_update target must be zero-vector");
  printf("test_ctrl_receives_dla_data: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 8: dyn_step receives torque output from ctrl_update            */
/* ------------------------------------------------------------------ */

static void test_dyn_receives_torque(void)
{
  float att[3] = {0};
  float rates[3] = {0};
  reset_stubs();
  s_ctrl_outputs[0] = 4.0f;
  s_ctrl_outputs[1] = 5.0f;
  s_ctrl_outputs[2] = 6.0f;
  set_dla_state(FM_NOMINAL, true, att, rates);
  vAttitudeControlTask_Step();

  CHECK(FPEQ(s_dyn_torque[0], 4.0f) && FPEQ(s_dyn_torque[1], 5.0f) && FPEQ(s_dyn_torque[2], 6.0f),
        "dyn_step must receive the torque vector returned by ctrl_update");
  printf("test_dyn_receives_torque: OK\n");
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(void)
{
  test_skip_in_fm_boot();
  test_skip_in_fm_safe();
  test_skip_in_fm_detumble();
  test_runs_in_fm_nominal();
  test_runs_in_fm_diagnostic();
  test_skip_when_imu_invalid();
  test_ctrl_receives_dla_data();
  test_dyn_receives_torque();

  if (g_failures == 0)
  {
    printf("All Attitude Control Task DLA checks PASSED.\n");
    return 0;
  }
  printf("%d Attitude Control Task check(s) FAILED.\n", g_failures);
  return 1;
}
