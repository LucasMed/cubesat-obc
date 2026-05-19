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
 * Voltage thresholds calibrated for 1S LiPo battery (3.0–4.2 V)
 * measured via resistor divider on ADC0 (GPIO26).
 *   NOMINAL   : V_batt >= 3.6 V
 *   LOW       : 3.3 V <= V_batt < 3.6 V
 *   CRITICAL  : 3.0 V <= V_batt < 3.3 V
 *   EMERGENCY : V_batt < 3.0 V
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
/* flight_mode.h intentionally omitted: EPS does not call FMM directly.
 * The single FDIR authority chain is: EPS → fault_report() → FaultMgr → FMM. */

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
/* Voltage plausibility bounds (V) — anything outside this range is NOT a
 * real 1S LiPo (or a USB-charged bus) and indicates the ADC is reading
 * garbage (USB-only bench, floating pin, weak stub fallback).
 *
 * When USB is connected, the system bus (which the divider may tap) can
 * reach ~5.0 V.  The TP4056 charger ceiling is 4.2 V.  6.0 V covers both
 * plus margin; any real reading above that is hardware malfunction.
 * ------------------------------------------------------------------ */
#define VBATT_PLAUSIBLE_MIN 2.5f
#define VBATT_PLAUSIBLE_MAX 6.0f

/* Voltage classification thresholds (V)                               */
/* ------------------------------------------------------------------ */

#define VBATT_TH_LOW 3.6f       /**< NOMINAL→LOW boundary (falling) — 1S LiPo */
#define VBATT_TH_CRITICAL 3.3f  /**< LOW→CRITICAL boundary (falling) — 1S LiPo */
#define VBATT_TH_EMERGENCY 3.0f /**< CRITICAL→EMERGENCY boundary (falling) — 1S LiPo */
#define VBATT_HYSTERESIS 0.1f   /**< Dead band applied to upward transitions */

/* ------------------------------------------------------------------ */
/* Internal state                                                      */
/* ------------------------------------------------------------------ */

static eps_snapshot_t g_snapshot = {0};
static energy_state_t g_prev_state = ENERGY_NOMINAL;
static bool g_initialised = false;

/* ------------------------------------------------------------------ */
/* HAL entry point — defined by src/drivers/eps_hal.c (same library)  */
/* ------------------------------------------------------------------ */

/* Forward declaration only (MISRA-C:2012 Rule 8.4).
 * The implementation lives in src/drivers/eps_hal.c which is compiled
 * into the SAME library (eps_lib).  No weak stub here — that would
 * prevent the linker from pulling in eps_hal.c.o from a static library.
 * Test files that compile eps_monitor.c directly supply their own
 * definition of eps_hal_read().                                         */
bool eps_hal_read(float *vbatt, float *ibatt, float *temp);

/* ------------------------------------------------------------------ */
/* Schmidt-trigger voltage → energy state                              */
/* ------------------------------------------------------------------ */

/**
 * @brief Classify bus voltage as an energy_state_t with hysteresis.
 *
 * Downward transitions (voltage falling) are accepted immediately at
 * the base threshold for fast fault response.  Upward transitions
 * (voltage recovering) are only accepted when voltage exceeds the base
 * threshold by VBATT_HYSTERESIS, preventing chatter at boundaries.
 *
 * Thresholds are calibrated for a 1S LiPo battery measured via the
 * resistor divider on ADC0 (GPIO26).
 *
 * @param v     Measured bus voltage (V).
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
    fault_clear(FAULT_EPS_VBATT_EMERGENCY);
    break;

  case ENERGY_LOW:
    if (prev > ENERGY_LOW)
    {
      /* Recovering from CRITICAL / EMERGENCY: clear the deeper faults */
      fault_clear(FAULT_EPS_VBATT_CRITICAL);
      fault_clear(FAULT_EPS_VBATT_EMERGENCY);
    }
    fault_report(FAULT_EPS_VBATT_LOW, FAULT_LEVEL_WARNING);
    break;

  case ENERGY_CRITICAL:
    fault_clear(FAULT_EPS_VBATT_LOW);
    /* Single FDIR authority chain: EPS → FaultMgr → FMM.
     * CRITICAL level triggers fmm_force_safe() inside fault_manager. */
    fault_report(FAULT_EPS_VBATT_CRITICAL, FAULT_LEVEL_CRITICAL);
    break;

  case ENERGY_EMERGENCY:
    fault_clear(FAULT_EPS_VBATT_LOW);
    /* EMERGENCY escalates with its own fault ID (FAULT-DES-001 OI-1).
     * CRITICAL level triggers fmm_force_safe() inside fault_manager. */
    fault_report(FAULT_EPS_VBATT_EMERGENCY, FAULT_LEVEL_CRITICAL);
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
  eps_lock();

  (void)memset(&g_snapshot, 0, sizeof(g_snapshot));

  /* All rails enabled by default; OBC rail cannot be disabled */
  for (int i = 0; i < (int)EPS_RAIL_COUNT; i++)
  {
    g_snapshot.rail_enabled[i] = true;
  }

  /* Do NOT read the ADC during init — on RP2350 the ADC clock may not
   * be ready this early in the boot sequence, and adc_read() busy-waits
   * on a READY bit that never asserts.  The first eps_monitor_tick()
   * (called from the FreeRTOS task, well after all clocks are stable)
   * will read the real hardware.  Until then, assume NOMINAL.          */
  g_snapshot.vbatt = 7.6f; /* placeholder, overwritten on first tick */
  g_snapshot.ibatt = 0.0f;
  g_snapshot.temperature = 25.0f;
  g_snapshot.state = ENERGY_NOMINAL;

  g_prev_state = ENERGY_NOMINAL;
  g_initialised = true;

  eps_unlock();

  data_layer_set_energy_state(ENERGY_NOMINAL);
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

  /* Read battery voltage from ADC / INA219 */
  bool ok = eps_hal_read(&vbatt, &ibatt, &temp);

  if (!ok)
  {
    fault_report(FAULT_EPS_READ_ERROR, FAULT_LEVEL_ERROR);
    /* Keep previous state — do not update snapshot on a failed read */
    return;
  }

  /* Plausibility check: if voltage is outside the 1S LiPo / USB-charged-bus
   * range (2.5-6.0 V), the ADC is reading garbage (USB-only bench with
   * floating pin, weak stub fallback, or a hardware fault).
   * Force NOMINAL to prevent false EMERGENCY triggers during development. */
  if (vbatt < VBATT_PLAUSIBLE_MIN || vbatt > VBATT_PLAUSIBLE_MAX)
  {
    eps_lock();
    energy_state_t prev = g_prev_state;
    g_snapshot.vbatt = 7.6f;
    g_snapshot.ibatt = 0.0f;
    g_snapshot.temperature = temp;
    g_snapshot.state = ENERGY_NOMINAL;
    g_prev_state = ENERGY_NOMINAL;
    eps_unlock();
    handle_state_change(prev, ENERGY_NOMINAL);
    data_layer_set_energy_state(ENERGY_NOMINAL);
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
