/**
 * @file attitude_control_task.c
 * @brief Attitude control task — momentum dump / LQR / PID mode dispatch.
 *
 * All shared state is read exclusively through data_layer.h.
 *
 * Controller dispatch (PR-15 / PR-17):
 *   FM_DETUMBLE                 → momentum_dump_step() → magnetorquer (B×L law)
 *   FM_NOMINAL + imu_ekf_valid  → LQR (precise nadir tracking)
 *   FM_NOMINAL + !imu_ekf_valid → PID fallback (EKF converging)
 *   FM_DIAGNOSTIC               → PID (diagnostic / tuning mode)
 *   FM_BOOT / FM_SAFE           → return immediately, no actuator output
 *
 * Sensor validity guard (NOMINAL / DIAGNOSTIC only):
 *   If imu_valid == false the LQR/PID paths are skipped to avoid
 *   computing torques from zeroed attitude/rate data.
 *
 * Spec ref: SPEC-2-CTRL v1.3 §4.1, SPEC-2-DLA v1.6 §2.4,
 *           SPEC-2-ADCS v1.1 §5.2, PHASE5_PLAN PR-17
 */

#include "attitude_control_task.h"

#include "FreeRTOS.h"
#include "attitude_control.h"
#include "attitude_dynamics.h"
#include "config.h"
#include "data_layer.h"
#include "flight_mode.h"
#include "lqr.h"
#include "lqr_schedule.h"
#include "magnetorquer.h"
#include "momentum_dump.h"
#include "task.h"

#include <stdio.h>

#ifdef PICO_BUILD
  #include "pico/time.h"
#endif

static attitude_ctrl_t g_ctrl;
static attitude_dyn_t g_dyn;
static lqr_t g_lqr;
static momentum_dump_t g_mdump;
static magnetorquer_t g_mtq;

// Core logic for attitude control (independent of FreeRTOS task loop)
void vAttitudeControlTask_Step(void)
{
  dl_snapshot_t snap;
  data_layer_read(&snap);

  /* FM_DETUMBLE: bleed reaction-wheel momentum via magnetorquer B×L law.
   * B field placeholder is zero until PR-18 wires the magnetometer into
   * the DLA.  When B == 0 momentum_dump_step() safely outputs a zero
   * dipole command, so no spurious torque is applied. */
  if (snap.mode == FM_DETUMBLE)
  {
    float B[3] = {0.0f, 0.0f, 0.0f}; /* TODO PR-18: read DLA mag_field */
    float dipole[3] = {0.0f, 0.0f, 0.0f};
    momentum_dump_step(&g_mdump, B, snap.state.rates, dipole);
    magnetorquer_set_moment(&g_mtq, dipole[0], dipole[1], dipole[2]);
    return;
  }

  /* Only compute attitude control torques in fully operational flight modes */
  if (snap.mode != FM_NOMINAL && snap.mode != FM_DIAGNOSTIC)
  {
    return;
  }

  /* Skip if IMU data is not yet valid */
  if (!snap.state.imu_valid)
  {
    return;
  }

  float target[3] = {0.0f, 0.0f, 0.0f}; /* setpoint: nadir-pointing */
  float torque[3] = {0.0f, 0.0f, 0.0f};
  const float dt = 1.0f / CONTROL_LOOP_HZ;

  if (snap.mode == FM_NOMINAL && snap.state.imu_ekf_valid)
  {
    /* Precise nadir tracking: use LQR with EKF attitude estimate.
     * Apply mode-scheduled gains before computing torques (PR-23). */
    lqr_schedule_apply(&g_lqr, snap.mode);
    float att_err[3] = {snap.state.attitude[0] - target[0], snap.state.attitude[1] - target[1],
                        snap.state.attitude[2] - target[2]};
    lqr_compute(&g_lqr, att_err, snap.state.rates, torque);
  }
  else
  {
    /* FM_DIAGNOSTIC, or FM_NOMINAL before EKF has converged: use PID. */
    float outputs[3] = {0.0f, 0.0f, 0.0f};
    attitude_ctrl_update(&g_ctrl, target, snap.state.attitude, snap.state.rates, outputs, dt);
    torque[0] = outputs[0];
    torque[1] = outputs[1];
    torque[2] = outputs[2];
  }

  attitude_dynamics_step(&g_dyn, torque, dt);
}

// Attitude control task: runs control loop at 20 Hz
void vAttitudeControlTask(void *pvParameters)
{
  (void)pvParameters;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(50);  // 20 Hz

#ifdef PICO_BUILD
  uint64_t last_wake = time_us_64();
  uint64_t min_int = 0xFFFFFFFFFFFFFFFF;
  uint64_t max_int = 0;
  uint64_t sum_int = 0;
  uint32_t samples = 0;
#endif

  // Initialize control, dynamics, LQR, and momentum dump
  attitude_ctrl_init(&g_ctrl);
  attitude_dynamics_init(&g_dyn);
  lqr_init(&g_lqr);
  momentum_dump_init(&g_mdump, DETUMBLE_K_DUMP);
  magnetorquer_init(&g_mtq);

  printf("[attitude_control_task] Started\n");
  fflush(stdout);

  while (1)
  {
    vTaskDelayUntil(&xLastWakeTime, xFrequency);

#ifdef PICO_BUILD
    uint64_t now = time_us_64();
    uint64_t interval = now - last_wake;
    last_wake = now;

    if (samples > 0)
    {
      if (interval < min_int)
        min_int = interval;
      if (interval > max_int)
        max_int = interval;
      sum_int += interval;
    }
    samples++;

    if (samples >= 201)
    {  // 200 samples for 20Hz (~10 seconds)
      printf("[attitude_control_task] Timing (200 samples): min=%llu, max=%llu, avg=%llu us\n",
             (unsigned long long)min_int, (unsigned long long)max_int,
             (unsigned long long)(sum_int / 200));
      min_int = 0xFFFFFFFFFFFFFFFF;
      max_int = 0;
      sum_int = 0;
      samples = 1;
    }
#endif

    vAttitudeControlTask_Step();
  }
}
