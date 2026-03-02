/**
 * @file attitude_control_task.c
 * @brief Attitude control task — migrated to Data Layer Abstraction (PR-8).
 *
 * All shared state is read exclusively through data_layer.h.
 * The legacy system_state.h API is no longer used here.
 *
 * Flight-mode guard (per SPEC-2-CTRL §4.1):
 *   Control torques are computed ONLY when the flight mode is
 *   FM_NOMINAL or FM_DIAGNOSTIC.  In FM_BOOT, FM_SAFE, or FM_DETUMBLE
 *   the step returns immediately without touching the actuators.
 *
 * Sensor validity guard:
 *   If the IMU data is stale (imu_valid == false) the step is skipped
 *   to avoid computing torques from zeroed attitude/rate data.
 *
 * Spec ref: SPEC-2-CTRL v1.3 §4.1, SPEC-2-DLA v1.6 §2.4
 */

#include "attitude_control_task.h"

#include "FreeRTOS.h"
#include "attitude_control.h"
#include "attitude_dynamics.h"
#include "config.h"
#include "data_layer.h"
#include "flight_mode.h"
#include "task.h"

#include <stdio.h>

#ifdef PICO_BUILD
  #include "pico/time.h"
#endif

static attitude_ctrl_t g_ctrl;
static attitude_dyn_t g_dyn;

// Core logic for attitude control (independent of FreeRTOS task loop)
void vAttitudeControlTask_Step(void)
{
  dl_snapshot_t snap;
  data_layer_read(&snap);

  /* Only compute control torques in fully operational flight modes */
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
  float outputs[3] = {0.0f, 0.0f, 0.0f};
  const float dt = 1.0f / CONTROL_LOOP_HZ;

  attitude_ctrl_update(&g_ctrl, target, snap.state.attitude, snap.state.rates, outputs, dt);

  float torque[3] = {outputs[0], outputs[1], outputs[2]};
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

  // Initialize control and dynamics
  attitude_ctrl_init(&g_ctrl);
  attitude_dynamics_init(&g_dyn);

  printf("[attitude_control_task] Started\n");

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
