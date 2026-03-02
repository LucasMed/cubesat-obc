/**
 * @file eps_monitor.c
 * @brief Electrical Power System (EPS) monitor implementation.
 *
 * Reads battery voltage every tick, classifies the energy state using
 * a Schmidt-trigger hysteresis model, updates the shared Data Layer
 * snapshot, and raises / clears faults via the Fault Manager.
 *
 * HAL entry point: eps_hal_read() is declared __attribute__((weak)) so
 * that unit tests can override it with a stub that injects test voltages.
 *
 * Voltage thresholds per SPEC-2-EPS v1.14 Table 3-1:
 *   NOMINAL   : V_batt >= 7.4 V
 *   LOW       : 7.0 V <= V_batt < 7.4 V
 *   CRITICAL  : 6.6 V <= V_batt < 7.0 V
 *   EMERGENCY : V_batt < 6.6 V
 *
 * Hysteresis of 0.1 V is applied to upward (recovering) transitions
 * only; downward transitions are accepted immediately (safety-first).
 *
 * Spec ref: SPEC-2-EPS v1.14, SPEC-2 v2.0 §4.9
 */

#include "data_layer.h"
#include "eps.h"
#include "fault_ids.h"
#include "fault_manager.h"
#include "flight_mode.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef PICO_BUILD
  #include "FreeRTOS.h"
  #include "task.h"
  #define eps_lock() taskENTER_CRITICAL()
  #define eps_unlock() taskEXIT_CRITICAL()
#else
  #define eps_lock() ((void)0)
  #define eps_unlock() ((void)0)
#endif

/* ------------------------------------------------------------------ */
/* Voltage classification thresholds (V)                               */
/* ------------------------------------------------------------------ */

#define VBATT_TH_LOW 7.4f       /**< NOMINAL→LOW boundary (falling)      */
#define VBATT_TH_CRITICAL 7.0f  /**< LOW→CRITICAL boundary (falling)     */
#define VBATT_TH_EMERGENCY 6.6f /**< CRITICAL→EMERGENCY boundary (falling) */
#define VBATT_HYSTERESIS 0.1f   /**< Dead band applied to upward transitions */

/* ------------------------------------------------------------------ */
/* Internal state                                                      */
/* ------------------------------------------------------------------ */

static eps_snapshot_t g_snapshot = {0};
static energy_state_t g_prev_state = ENERGY_NOMINAL;
static bool g_initialised = false;

/* ------------------------------------------------------------------ */
/* HAL stub (weak — override in PICO driver layer or unit tests)       */
/* ------------------------------------------------------------------ */

/**
 * @brief Read raw EPS telemetry from hardware.
 *
 * The default implementation returns nominal values for host builds.
 * Override with a strong symbol in the hardware driver layer or in
 * unit test files.
 *
 * @param vbatt  Output: measured battery voltage (V).
 * @param ibatt  Output: battery current, positive = charging (A).
 * @param temp   Output: PCB / cell temperature (°C).
 * @return true on success, false if the sensor read failed.
 */
__attribute__((weak)) bool eps_hal_read(float *vbatt, float *ibatt, float *temp)
{
  if (vbatt)
  {
    *vbatt = 7.6f;
  }
  if (ibatt)
  {
    *ibatt = 0.5f;
  }
  if (temp)
  {
    *temp = 25.0f;
  }
  return true;
}

/* ------------------------------------------------------------------ */
/* Schmidt-trigger voltage → energy state                              */
/* ------------------------------------------------------------------ */

/**
 * @brief Classify battery voltage as an energy_state_t with hysteresis.
 *
 * Downward transitions (voltage falling) are accepted immediately at
 * the base threshold for fast fault response.  Upward transitions
 * (voltage recovering) are only accepted when voltage exceeds the base
 * threshold by VBATT_HYSTERESIS, preventing chatter at boundaries.
 *
 * @param v     Measured battery voltage (V).
 * @param prev  Energy state from the previous evaluation cycle.
 * @return      New energy state.
 */
static energy_state_t compute_energy_state(float v, energy_state_t prev)
{
  /* Raw classification using base thresholds (no hysteresis). */
  energy_state_t raw;
  if (v < VBATT_TH_EMERGENCY)
  {
    raw = ENERGY_EMERGENCY;
  }
  else if (v < VBATT_TH_CRITICAL)
  {
    raw = ENERGY_CRITICAL;
  }
  else if (v < VBATT_TH_LOW)
  {
    raw = ENERGY_LOW;
  }
  else
  {
    raw = ENERGY_NOMINAL;
  }

  /*
   * If the raw state is WORSE than or equal to the previous state,
   * accept it immediately — fast path for safety-critical drops.
   */
  if ((int)raw >= (int)prev)
  {
    return raw;
  }

  /*
   * State would IMPROVE: subtract VBATT_HYSTERESIS before re-classifying.
   * This effectively raises each upward threshold by 0.1 V.
   */
  float v_hyst = v - VBATT_HYSTERESIS;
  if (v_hyst < VBATT_TH_EMERGENCY)
  {
    return ENERGY_EMERGENCY;
  }
  if (v_hyst < VBATT_TH_CRITICAL)
  {
    return ENERGY_CRITICAL;
  }
  if (v_hyst < VBATT_TH_LOW)
  {
    return ENERGY_LOW;
  }
  return ENERGY_NOMINAL;
}

/* ------------------------------------------------------------------ */
/* Fault and flight-mode side-effects on energy state transitions     */
/* ------------------------------------------------------------------ */

static void handle_state_change(energy_state_t prev, energy_state_t next)
{
  if (next == prev)
  {
    return;
  }

  switch (next)
  {
  case ENERGY_NOMINAL:
    fault_clear(FAULT_EPS_VBATT_LOW);
    fault_clear(FAULT_EPS_VBATT_CRITICAL);
    break;

  case ENERGY_LOW:
    if (prev > ENERGY_LOW)
    {
      /* Recovering from CRITICAL / EMERGENCY: clear the CRITICAL fault */
      fault_clear(FAULT_EPS_VBATT_CRITICAL);
    }
    fault_report(FAULT_EPS_VBATT_LOW, FAULT_LEVEL_WARNING);
    break;

  case ENERGY_CRITICAL:
    fault_clear(FAULT_EPS_VBATT_LOW);
    /* ERROR level records the anomaly without forcing FM_SAFE directly */
    fault_report(FAULT_EPS_VBATT_CRITICAL, FAULT_LEVEL_ERROR);
    /* Spec: ENERGY_CRITICAL → FM_SAFE requested */
    (void)fmm_request_transition(FM_SAFE);
    break;

  case ENERGY_EMERGENCY:
    fault_clear(FAULT_EPS_VBATT_LOW);
    /*
     * CRITICAL fault level triggers fmm_force_safe() inside
     * fault_manager — no need to call it explicitly here.
     */
    fault_report(FAULT_EPS_VBATT_CRITICAL, FAULT_LEVEL_CRITICAL);
    break;

  default:
    break;
  }
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

int eps_monitor_init(void)
{
  float vbatt = 7.6f;
  float ibatt = 0.5f;
  float temp = 25.0f;
  bool ok = eps_hal_read(&vbatt, &ibatt, &temp);

  eps_lock();

  (void)memset(&g_snapshot, 0, sizeof(g_snapshot));

  /* All rails enabled by default; OBC rail cannot be disabled */
  for (int i = 0; i < (int)EPS_RAIL_COUNT; i++)
  {
    g_snapshot.rail_enabled[i] = true;
  }

  if (ok)
  {
    g_snapshot.vbatt = vbatt;
    g_snapshot.ibatt = ibatt;
    g_snapshot.temperature = temp;
    g_snapshot.state = compute_energy_state(vbatt, ENERGY_NOMINAL);
  }
  else
  {
    /* Fail-safe: assume nominal until we get a valid reading */
    g_snapshot.state = ENERGY_NOMINAL;
    fault_report(FAULT_EPS_READ_ERROR, FAULT_LEVEL_ERROR);
  }

  g_prev_state = g_snapshot.state;
  g_initialised = true;

  eps_unlock();

  data_layer_set_energy_state(g_snapshot.state);
  return 0;
}

void eps_monitor_tick(void)
{
  if (!g_initialised)
  {
    return;
  }

  float vbatt = 0.0f;
  float ibatt = 0.0f;
  float temp = 0.0f;
  bool ok = eps_hal_read(&vbatt, &ibatt, &temp);

  if (!ok)
  {
    fault_report(FAULT_EPS_READ_ERROR, FAULT_LEVEL_ERROR);
    /* Keep previous state — do not update snapshot on a failed read */
    return;
  }

  eps_lock();
  energy_state_t prev = g_prev_state;
  g_snapshot.vbatt = vbatt;
  g_snapshot.ibatt = ibatt;
  g_snapshot.temperature = temp;
  energy_state_t next = compute_energy_state(vbatt, prev);
  g_snapshot.state = next;
  g_prev_state = next;
  eps_unlock();

  /* Side-effects (fault_report / fmm calls) outside the lock */
  handle_state_change(prev, next);
  data_layer_set_energy_state(next);
}

int eps_snapshot_get(eps_snapshot_t *out)
{
  if (!out || !g_initialised)
  {
    return -1;
  }
  eps_lock();
  *out = g_snapshot;
  eps_unlock();
  return 0;
}

void eps_set_power(eps_rail_t rail, bool enable)
{
  /* OBC rail cannot be disabled; silently ignore the call */
  if (rail == EPS_RAIL_OBC || rail >= EPS_RAIL_COUNT)
  {
    return;
  }
  eps_lock();
  g_snapshot.rail_enabled[rail] = enable;
  eps_unlock();
}

energy_state_t eps_get_energy_state(void)
{
  return g_snapshot.state;
}
