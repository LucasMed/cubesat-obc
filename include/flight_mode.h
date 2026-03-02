/**
 * @file flight_mode.h
 * @brief Flight mode definitions and Flight Mode Manager (FMM) API.
 *
 * Defines the canonical flight mode enumeration and the API for the
 * Flight Mode Manager service.  All subsystems that need to query or
 * request mode transitions MUST use these declarations.
 *
 * Spec ref: SPEC-2-FMM v1.1, SPEC-2 v2.0 §4.1
 */

#ifndef FLIGHT_MODE_H
#define FLIGHT_MODE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /* ------------------------------------------------------------------ */
  /* Flight mode enumeration                                             */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Ordered flight mode states.
   *
   * Higher numeric values indicate progressively more nominal operation.
   * Transitions are governed by the FMM state machine (SPEC-2-FMM §3.2).
   */
  typedef enum
  {
    FM_BOOT = 0,       /**< Power-on / hardware initialisation         */
    FM_SAFE = 1,       /**< Minimum power, fault recovery              */
    FM_DETUMBLE = 2,   /**< Angular-rate reduction via B-dot law        */
    FM_NOMINAL = 3,    /**< Normal three-axis attitude control          */
    FM_DIAGNOSTIC = 4, /**< Ground-commanded diagnostic / testing mode  */
    FM_COUNT           /**< Number of valid modes (sentinel, not a mode) */
  } flight_mode_t;

  /* ------------------------------------------------------------------ */
  /* FMM transition request result codes                                 */
  /* ------------------------------------------------------------------ */

  typedef enum
  {
    FMM_OK = 0,              /**< Transition accepted (or already in target) */
    FMM_ERR_INVALID = 1,     /**< Target mode is not a valid mode            */
    FMM_ERR_NOT_ALLOWED = 2, /**< Transition disallowed from current mode    */
    FMM_ERR_FAULT_BLOCK = 3  /**< Active fault prevents the transition        */
  } fmm_result_t;

  /* ------------------------------------------------------------------ */
  /* FMM API                                                             */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Initialise the Flight Mode Manager.
   *
   * Sets the initial mode to FM_BOOT.  Must be called once during
   * system_init() before any task is created.
   */
  void flight_mode_manager_init(void);

  /**
   * @brief Query the current flight mode.
   * @return Current @ref flight_mode_t value.
   *
   * Thread-safe — uses a taskENTER_CRITICAL section internally.
   */
  flight_mode_t fmm_get_mode(void);

  /**
   * @brief Request a mode transition.
   *
   * The FMM evaluates the allowed-transition matrix before accepting.
   * Transitions into FM_SAFE are always accepted regardless of current
   * mode (unless the system is already in FM_SAFE).
   *
   * @param target  Desired @ref flight_mode_t.
   * @return        @ref fmm_result_t indicating success or reason for refusal.
   */
  fmm_result_t fmm_request_transition(flight_mode_t target);

  /**
   * @brief Force an immediate transition to FM_SAFE.
   *
   * Bypasses the normal allowed-transition matrix.  Intended for fault
   * handlers and watchdog callbacks.  May be called from ISR context.
   */
  void fmm_force_safe(void);

  /**
   * @brief Return human-readable name for a flight mode (for logging).
   * @param mode  Mode to stringify.
   * @return      Pointer to a null-terminated constant string.
   */
  const char *fmm_mode_name(flight_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* FLIGHT_MODE_H */
