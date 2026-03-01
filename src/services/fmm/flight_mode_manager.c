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
 * (fmm_force_safe() hard-codes this path and may be called from ISR).
 *
 * FAULT_LEVEL_CRITICAL blocks ALL transitions except to FM_SAFE.
 *
 * Spec ref: SPEC-2-FMM v1.1, SPEC-2 v2.0 §4.1
 */

#include "data_layer.h"
#include "fault_manager.h"
#include "flight_mode.h"

/* ------------------------------------------------------------------ */
/* Allowed-transition matrix                                           */
/* ------------------------------------------------------------------ */

/* g_allowed[from][to] == 1 means the transition is permitted.        */
/* The diagonal (same→same) is handled separately as FMM_OK (no-op). */
/* FM_COUNT is used as array size sentinel.                           */
static const uint8_t g_allowed[FM_COUNT][FM_COUNT] = {
    /*               BOOT  SAFE  DETUMBLE  NOMINAL  DIAGNOSTIC */
    /* FM_BOOT       */ {0, 1, 1, 0, 0},
    /* FM_SAFE       */ {0, 0, 1, 0, 0},
    /* FM_DETUMBLE   */ {0, 1, 0, 1, 0},
    /* FM_NOMINAL    */ {0, 1, 1, 0, 1},
    /* FM_DIAGNOSTIC */ {0, 1, 0, 1, 0},
};

/* Human-readable mode names for logging */
static const char *const g_mode_names[FM_COUNT] = {
    "BOOT", "SAFE", "DETUMBLE", "NOMINAL", "DIAGNOSTIC",
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
  return FMM_OK;
}

void fmm_force_safe(void)
{
  /* Bypasses matrix and fault checks — safe for ISR context because
   * data_layer_set_flight_mode uses a critical section, not a mutex. */
  data_layer_set_flight_mode(FM_SAFE);
}

const char *fmm_mode_name(flight_mode_t mode)
{
  if (mode >= FM_COUNT)
  {
    return "UNKNOWN";
  }
  return g_mode_names[mode];
}
