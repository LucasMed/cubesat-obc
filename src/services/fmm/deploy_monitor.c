/**
 * @file deploy_monitor.c
 * @brief Deploy Monitor — autonomous orbital deployment sequence.
 *
 * Implements the auto-transition logic (OI-1, OI-2) and mode timeout
 * enforcement (OI-6) as defined in FMM-DES-001.
 *
 * The monitor is an *external operator* on the FMM state machine — it
 * reads the Data Layer, evaluates pre-conditions, and calls the FMM API
 * (fmm_request_transition()).  It does NOT mutate the FMM table directly.
 *
 * Spec ref: FMM-SPEC-030–067, FMM-DES-001 §5
 */

#include "deploy_monitor.h"

#include "FreeRTOS.h"
#include "data_layer.h"
#include "flight_mode.h"
#include "post.h"
#include "task.h"

#include <math.h>

/* ------------------------------------------------------------------ */
/* Internal state                                                      */
/* ------------------------------------------------------------------ */

static bool s_deploy_running = false;
static uint16_t s_detumble_stable_count = 0;

/* ------------------------------------------------------------------ */
/* Timeout table                                                       */
/* ------------------------------------------------------------------ */

static const struct
{
  flight_mode_t mode;
  uint32_t timeout_ms;
  flight_mode_t fallback;
} s_timeouts[] = {
    {FM_BOOT, DEPLOY_TIMEOUT_BOOT_MS, FM_SAFE},
    {FM_DETUMBLE, DEPLOY_TIMEOUT_DETUMBLE_MS, FM_SAFE},
    {FM_DIAGNOSTIC, DEPLOY_TIMEOUT_DIAGNOSTIC_MS, FM_NOMINAL},
};

#define TIMEOUT_COUNT (sizeof(s_timeouts) / sizeof(s_timeouts[0]))

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void deploy_monitor_init(void)
{
  s_deploy_running = false;
}

bool deploy_is_in_progress(void)
{
  return data_layer_get_deploy_in_progress();
}

/* ------------------------------------------------------------------ */
/* Task entry point                                                    */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* Step function (extracted for testability)                           */
/* ------------------------------------------------------------------ */

void deploy_monitor_step(void)
{
  flight_mode_t mode = data_layer_get_flight_mode();

  /* ---------------------------------------------------------------- */
  /* OI-1: Auto BOOT → DETUMBLE                                      */
  /* ---------------------------------------------------------------- */
  if (mode == FM_BOOT && !deploy_is_in_progress())
  {
    post_record_t post_rec;
    data_layer_get_post_last(&post_rec);

    dl_snapshot_t snap;
    data_layer_read(&snap);

    uint32_t elapsed =
        (xTaskGetTickCount() - data_layer_get_mode_entry_tick()) * portTICK_PERIOD_MS;

    if (post_rec.magic == POST_MAGIC && !post_is_critical_fail(&post_rec) && snap.state.imu_valid &&
        elapsed >= DEPLOY_BOOT_SETTLE_MS)
    {
      if (fmm_request_transition(FM_DETUMBLE) == FMM_OK)
      {
        data_layer_set_deploy_in_progress(true);
      }
    }
  }

  /* ---------------------------------------------------------------- */
  /* OI-2: Auto DETUMBLE → NOMINAL (leaky counter)                   */
  /* ---------------------------------------------------------------- */
  if (mode == FM_DETUMBLE)
  {
    dl_snapshot_t snap;
    data_layer_read(&snap);

    float omega = sqrtf(snap.state.rates[0] * snap.state.rates[0] +
                        snap.state.rates[1] * snap.state.rates[1] +
                        snap.state.rates[2] * snap.state.rates[2]);

    if (omega < DEPLOY_DETUMBLE_THRESHOLD)
    {
      if (s_detumble_stable_count < DEPLOY_STABLE_SAMPLES)
      {
        s_detumble_stable_count++;
      }
    }
    else
    {
      if (omega > DEPLOY_DETUMBLE_HARD_RESET)
      {
        s_detumble_stable_count = 0; /* Hard reset */
      }
      else if (s_detumble_stable_count > 0)
      {
        s_detumble_stable_count--; /* Leaky decrement */
      }
    }

    if (s_detumble_stable_count >= DEPLOY_STABLE_SAMPLES)
    {
      if (fmm_request_transition(FM_NOMINAL) == FMM_OK)
      {
        data_layer_set_deploy_in_progress(false);
        s_detumble_stable_count = 0;
      }
    }
  }

  /* ---------------------------------------------------------------- */
  /* OI-6: Mode timeouts                                              */
  /* ---------------------------------------------------------------- */
  for (size_t i = 0; i < TIMEOUT_COUNT; i++)
  {
    if (mode == s_timeouts[i].mode)
    {
      uint32_t elapsed =
          (xTaskGetTickCount() - data_layer_get_mode_entry_tick()) * portTICK_PERIOD_MS;
      if (elapsed >= s_timeouts[i].timeout_ms)
      {
        /* fmm_request_transition() will emit LOG_EVT_MODE_CHANGE internally */
        fmm_request_transition(s_timeouts[i].fallback);

        /* FMM-SPEC-083: clear deploy_in_progress when timing out to SAFE */
        if (s_timeouts[i].fallback == FM_SAFE)
        {
          data_layer_set_deploy_in_progress(false);
        }
      }
    }
  }
}

/* ------------------------------------------------------------------ */
/* Task entry point                                                    */
/* ------------------------------------------------------------------ */

void vDeployMonitorTask(void *pvParams)
{
  (void)pvParams;

  deploy_monitor_init();
  s_deploy_running = true;

  for (;;)
  {
    deploy_monitor_step();
    vTaskDelay(pdMS_TO_TICKS(100u));
  }
}
