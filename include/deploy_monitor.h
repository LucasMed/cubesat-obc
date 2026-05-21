/**
 * @file deploy_monitor.h
 * @brief Deploy Monitor API — auto-transition and timeout enforcement.
 *
 * Implements the autonomous orbital deployment sequence (OI-1, OI-2, OI-6)
 * as a standalone FreeRTOS task that drives the FMM via fmm_request_transition().
 *
 * The deploy monitor is an *external operator* — it reads the data layer,
 * evaluates conditions, and calls the FMM API.  It does NOT mutate the
 * FMM state machine directly.
 *
 * Spec ref: FMM-SPEC-030–067, FMM-DES-001 §5
 */

#ifndef DEPLOY_MONITOR_H
#define DEPLOY_MONITOR_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /* ------------------------------------------------------------------ */
  /* Auto-transition thresholds (compile-time macros)                   */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Settling time in BOOT before auto-transition to DETUMBLE.
   *
   * Waits for subsystems to stabilise and POST to complete before starting
   * the deploy sequence.
   */
#define DEPLOY_BOOT_SETTLE_MS      5000u   /* 5 s */

  /**
   * @brief Required time below detumble threshold for stable count.
   *
   * The leaky counter must reach this many consecutive stable samples
   * (at 100 ms loop) before transitioning DETUMBLE → NOMINAL.
   */
#define DEPLOY_DETUMBLE_STABLE_MS  5000u   /* 5 s */

  /**
   * @brief Angular rate threshold (rad/s) for detumble stability.
   *
   * |ω| < this value counts as a "stable" sample.  Each stable sample
   * increments the leaky counter.
   */
#define DEPLOY_DETUMBLE_THRESHOLD  0.05f   /* rad/s */

  /**
   * @brief Hard-reset threshold (rad/s).
   *
   * When |ω| > this value the leaky counter is immediately reset to zero
   * (FMM-DES-001 §4.2.3 OI-2).
   */
#define DEPLOY_DETUMBLE_HARD_RESET 0.10f   /* rad/s */

  /* ------------------------------------------------------------------ */
  /* Mode timeout durations (milliseconds)                               */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Maximum time allowed in FM_BOOT during deploy sequence.
   *
   * On timeout the monitor transitions to FM_SAFE.
   */
#define DEPLOY_TIMEOUT_BOOT_MS        300000u   /* 5 min */

  /**
   * @brief Maximum time allowed in FM_DETUMBLE during deploy sequence.
   *
   * On timeout the monitor transitions to FM_SAFE.
   */
#define DEPLOY_TIMEOUT_DETUMBLE_MS   1200000u   /* 20 min */

  /**
   * @brief Maximum time allowed in FM_DIAGNOSTIC during deploy sequence.
   *
   * On timeout the monitor transitions to FM_NOMINAL (back to nominal ops
   * after debugging).
   */
#define DEPLOY_TIMEOUT_DIAGNOSTIC_MS 1800000u   /* 30 min */

  /* ------------------------------------------------------------------ */
  /* Derived constants                                                   */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Required consecutive stable samples for detumble completion.
   *
   * Computed from the loop period (100 ms) and the stable window.
   */
#define DEPLOY_STABLE_SAMPLES \
    (DEPLOY_DETUMBLE_STABLE_MS / 100u)   /* = 50 */

  /* ------------------------------------------------------------------ */
  /* Public API                                                          */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Initialise the deploy monitor module.
   *
   * Resets internal state (leaky counter, running flag).
   * Called once from vDeployMonitorTask() before the main loop.
   */
  void deploy_monitor_init(void);

  /**
   * @brief Single-step the deploy monitor logic.
   *
   * Evaluates auto-transition conditions (OI-1, OI-2) and mode timeouts
   * (OI-6) once.  Exposed for integration testing — production code
   * should call vDeployMonitorTask() instead.
   */
  void deploy_monitor_step(void);

  /**
   * @brief Deploy monitor task entry point.
   *
   * Calls deploy_monitor_step() in a 100 ms loop.
   * Must be created as a FreeRTOS task.
   *
   * @param pvParams  Unused (reserved for future use).
   */
  void vDeployMonitorTask(void *pvParams);

  /**
   * @brief Check whether the deploy sequence is currently active.
   *
   * @return true if deploy_in_progress is set in the data layer.
   */
  bool deploy_is_in_progress(void);

#ifdef __cplusplus
}
#endif

#endif /* DEPLOY_MONITOR_H */
