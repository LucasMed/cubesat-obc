/**
 * @file eps.h
 * @brief Electrical Power System (EPS) monitor API and types.
 *
 * Defines the energy state enumeration and provides the API for
 * interacting with the EPS monitor task.
 *
 * Voltage thresholds (from SPEC-2-EPS v1.14 Table 3-1):
 *   NOMINAL   : V_batt ≥ 7.4 V
 *   LOW       : 7.0 V ≤ V_batt < 7.4 V
 *   CRITICAL  : 6.6 V ≤ V_batt < 7.0 V
 *   EMERGENCY : V_batt < 6.6 V  (non-essential loads shed immediately)
 *
 * Spec ref: SPEC-2-EPS v1.14, SPEC-2 v2.0 §4.9
 */

#ifndef EPS_H
#define EPS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Energy state enumeration                                            */
/* ------------------------------------------------------------------ */

/**
 * @brief Battery / bus energy state.
 *
 * The EPS monitor evaluates V_batt every 5 s and transitions between
 * states using the thresholds documented above.  Hysteresis of 0.1 V
 * is applied to prevent rapid toggling.
 */
typedef enum {
    ENERGY_NOMINAL    = 0, /**< Full authority — all loads allowed       */
    ENERGY_LOW        = 1, /**< Non-critical payloads optionally shed     */
    ENERGY_CRITICAL   = 2, /**< Attitude loads only — FM_SAFE requested  */
    ENERGY_EMERGENCY  = 3  /**< Minimal loads — immediate FM_SAFE forced */
} energy_state_t;

/* ------------------------------------------------------------------ */
/* Subsystem power rail identifiers                                    */
/* ------------------------------------------------------------------ */

typedef enum {
    EPS_RAIL_PAYLOAD   = 0, /**< Payload / mission instruments            */
    EPS_RAIL_COMMS     = 1, /**< Communication subsystem                  */
    EPS_RAIL_ADCS      = 2, /**< ADCS (reaction wheels + magnetorquers)   */
    EPS_RAIL_OBC       = 3, /**< On-board computer (cannot be shed)       */
    EPS_RAIL_COUNT          /**< Sentinel                                 */
} eps_rail_t;

/* ------------------------------------------------------------------ */
/* EPS snapshot                                                        */
/* ------------------------------------------------------------------ */

/**
 * @brief Point-in-time EPS telemetry snapshot.
 *
 * Obtained via eps_snapshot_get().
 */
typedef struct {
    float          vbatt;        /**< Battery voltage (V)                  */
    float          ibatt;        /**< Battery current, positive = charging (A) */
    float          temperature;  /**< PCB / battery temperature (°C)       */
    energy_state_t state;        /**< Current energy state                 */
    uint32_t       timestamp_ms; /**< Snapshot time (xTaskGetTickCount)     */
    bool           rail_enabled[EPS_RAIL_COUNT]; /**< Per-rail enable state */
} eps_snapshot_t;

/* ------------------------------------------------------------------ */
/* EPS monitor API                                                     */
/* ------------------------------------------------------------------ */

/**
 * @brief Initialise the EPS monitor.
 *
 * Performs initial sensor read and sets the energy state.
 * Must be called from system_init() before the EPS task starts.
 *
 * @return 0 on success, negative errno on failure.
 */
int eps_monitor_init(void);

/**
 * @brief Copy the most recent EPS snapshot into @p out.
 *
 * Thread-safe.  The snapshot is updated every 5 s by the EPS task.
 *
 * @param out  Caller-allocated @ref eps_snapshot_t to populate.
 * @return     0 on success, -1 if EPS monitor is not yet initialised.
 */
int eps_snapshot_get(eps_snapshot_t *out);

/**
 * @brief Enable or disable a power rail.
 *
 * OBC rail (EPS_RAIL_OBC) may not be disabled; the call is silently
 * ignored for that rail.
 *
 * @param rail    Rail identifier.
 * @param enable  true to enable, false to disable.
 */
void eps_set_power(eps_rail_t rail, bool enable);

/**
 * @brief Return the current energy state.
 *
 * Lightweight accessor — does not re-read sensors.
 */
energy_state_t eps_get_energy_state(void);

#ifdef __cplusplus
}
#endif

#endif /* EPS_H */
