/**
 * @file flight_mode_manager.c
 * @brief Flight Mode Manager (FMM) implementation.
 *
 * Implements the five-mode state machine defined in SPEC-2-FMM v1.1.
 * All mode changes are written to the Data Layer so every subsystem
 * sees a consistent view via data_layer_get_flight_mode().
 *
 * @par Allowed-transition matrix (SPEC-2-FMM §3.2)
 *
 *   From \ To  | BOOT  SAFE  DETUMBLE  NOMINAL  DIAGNOSTIC
 *   -----------|------------------------------------------
 *   BOOT       |  —     Y      Y         N         N
 *   SAFE       |  N     —      Y         N         N
 *   DETUMBLE   |  N     Y      —         Y         N
 *   NOMINAL    |  N     Y      Y         —         Y
 *   DIAGNOSTIC |  N     Y      N         Y         —
 *
 * Transitions TO FM_SAFE are always allowed from any mode
 * (fmm_force_safe() hard-codes this path — task context only, NOT ISR-safe).
 *
 * NOTE: data_layer_set_flight_mode() uses xSemaphoreTake() (mutex).
 * fmm_force_safe() must NOT be called from ISR context. See FMM-DES-001 OI-5.
 *
 * Spec ref: SPEC-2-FMM v1.1, SPEC-2 v2.0 §4.1
 */

#include "FreeRTOS.h"
#include "data_layer.h"
#include "fault_manager.h"
#include "flight_mode.h"
#include "logger.h"
#include "task.h"

/* ------------------------------------------------------------------ */
/* Allowed-transition matrix                                           */
/* ------------------------------------------------------------------ */

static const uint8_t g_allowed[FM_COUNT][FM_COUNT] = {
    /*               BOOT  SAFE  DETUMBLE  NOMINAL  DIAGNOSTIC  PAYLOAD */
    /* FM_BOOT       */ {0, 1, 1, 0, 0, 0},
    /* FM_SAFE       */ {0, 0, 1, 0, 0, 0},
    /* FM_DETUMBLE   */ {0, 1, 0, 1, 0, 0},
    /* FM_NOMINAL    */ {0, 1, 1, 0, 1, 1},
    /* FM_DIAGNOSTIC */ {0, 1, 0, 1, 0, 0},
    /* FM_PAYLOAD    */ {0, 1, 0, 1, 0, 0},
};

/* Human-readable mode names for logging */
static const char *const g_mode_names[FM_COUNT] = {
    "BOOT", "SAFE", "DETUMBLE", "NOMINAL", "DIAGNOSTIC", "PAYLOAD",
};

/* ------------------------------------------------------------------ */
/* Weak stub for fault_get_highest_level                               */
/* ------------------------------------------------------------------ */
/*
 * This weak definition is overridden once fault_manager.c (PR-4) is
 * linked.  Until then the FMM treats the fault state as NONE so all
 * transitions are evaluated purely on the matrix above.
 */
__attribute__((weak)) fault_level_t fault_get_highest_level(void)
{
  return FAULT_LEVEL_NONE;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void flight_mode_manager_init(void)
{
  data_layer_set_flight_mode(FM_BOOT);
}

flight_mode_t fmm_get_mode(void)
{
  return data_layer_get_flight_mode();
}

fmm_result_t fmm_request_transition(flight_mode_t target)
{
  /* Validate target range */
  if (target >= FM_COUNT)
  {
    return FMM_ERR_INVALID;
  }

  flight_mode_t current = data_layer_get_flight_mode();

  /* Already in the requested mode — nothing to do */
  if (current == target)
  {
    return FMM_OK;
  }

  /* Transitions TO FM_SAFE are always allowed (unless already safe,
   * handled above). */
  if (target != FM_SAFE)
  {
    /* Check fault level: CRITICAL blocks all non-SAFE transitions */
    // cppcheck-suppress knownConditionTrueFalse
    if (fault_get_highest_level() >= FAULT_LEVEL_CRITICAL)
    {
      return FMM_ERR_FAULT_BLOCK;
    }

    /* Check allowed-transition matrix */
    if (!g_allowed[current][target])
    {
      return FMM_ERR_NOT_ALLOWED;
    }
  }

  data_layer_set_flight_mode(target);

  /* LOG_EVT_MODE_CHANGE (FMM-DES-001 §8.6): emit event with old/new mode in payload.
   * Not called from ISR context — log_event() uses mutex internally and is task-context only. */
  {
    const uint8_t mode_payload[2] = {(uint8_t)current, (uint8_t)target};
    log_event(LOG_EVT_MODE_CHANGE, LOG_CLASS_OPERATIONAL, mode_payload, 2);
  }

  /* Record entry tick for mode timeout enforcement (OI-6) */
  data_layer_set_mode_entry_tick(xTaskGetTickCount());

  return FMM_OK;
}

void fmm_force_safe(void)
{
  /* Bypasses matrix and fault checks.
   * ISR-safe: uses data_layer_set_flight_mode_from_isr() with critical section
   * instead of xSemaphoreTake(). Safe to call from ISR context (CDR-SAF-01, OI-SW-2).
   *
   * NOTE: LOG_EVT_MODE_CHANGE is NOT emitted here because:
   *   - log_event() uses a mutex and is NOT ISR-safe
   *   - The watchdog scratch register is the authoritative forensic record for safe-mode entry
   *   - LOG_EVT_SAFE_ENTRY (0x0001) is already emitted by fault_manager.c when fmm_force_safe() is
   *     called from the fault chain in task context
   * mode_entry_tick is NOT updated — xTaskGetTickCount() is unsafe in ISR context. */
  data_layer_set_flight_mode_from_isr(FM_SAFE);
}

const char *fmm_mode_name(flight_mode_t mode)
{
  if (mode >= FM_COUNT)
  {
    return "UNKNOWN";
  }
  return g_mode_names[mode];
}
