/**
 * @file fault_manager.h
 * @brief Fault Manager API.
 *
 * The Fault Manager tracks active faults, assigns severity levels, and
 * drives Flight Mode Manager transitions when faults escalate.
 *
 * Fault levels are ordered: NONE < WARNING < ERROR < CRITICAL.
 * A CRITICAL fault triggers an immediate transition to FM_SAFE.
 *
 * Spec ref: SPEC-2-FMM v1.1 §4–6, SPEC-2 v2.0 §4.2
 */

#ifndef FAULT_MANAGER_H
#define FAULT_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "fault_ids.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Fault severity levels                                               */
/* ------------------------------------------------------------------ */

/**
 * @brief Ordered severity levels for fault events.
 *
 * FAULT_LEVEL_CRITICAL causes an immediate FM_SAFE transition.
 * FAULT_LEVEL_ERROR    triggers an anomaly log entry.
 * FAULT_LEVEL_WARNING  is recorded but does not change flight mode.
 */
typedef enum {
    FAULT_LEVEL_NONE     = 0, /**< No active fault (cleared state)  */
    FAULT_LEVEL_WARNING  = 1, /**< Recoverable, informational       */
    FAULT_LEVEL_ERROR    = 2, /**< Degraded operation               */
    FAULT_LEVEL_CRITICAL = 3  /**< Immediate safe-mode required     */
} fault_level_t;

/* ------------------------------------------------------------------ */
/* Fault event record                                                  */
/* ------------------------------------------------------------------ */

/**
 * @brief Description of a single fault instance.
 *
 * Stored in the fault table (one entry per fault ID).
 * @note Callers should treat this struct as read-only; use the API
 *       functions below to modify state.
 */
typedef struct {
    uint16_t      id;           /**< Fault identifier (see fault_ids.h)  */
    fault_level_t level;        /**< Severity of this fault               */
    uint32_t      timestamp_ms; /**< xTaskGetTickCount() at first report  */
    uint32_t      count;        /**< Number of times fault has been filed  */
    bool          active;       /**< True if fault has not been cleared    */
} fault_event_t;

/* ------------------------------------------------------------------ */
/* Fault Manager API                                                   */
/* ------------------------------------------------------------------ */

/**
 * @brief Initialise the Fault Manager.
 *
 * Clears all fault table entries.  Must be called once during system
 * initialisation before any task starts.
 */
void fault_manager_init(void);

/**
 * @brief Report (raise) a fault.
 *
 * If the fault is already active the count is incremented.
 * If the level is FAULT_LEVEL_CRITICAL, fmm_force_safe() is called.
 *
 * @param id     Fault identifier from fault_ids.h.
 * @param level  Severity level to associate with this fault.
 */
void fault_report(uint16_t id, fault_level_t level);

/**
 * @brief Clear (acknowledge) an active fault.
 *
 * The fault entry remains in the table but is marked inactive.
 * Calling fault_report() again after clearing restarts the counter.
 *
 * @param id  Fault identifier to clear.
 */
void fault_clear(uint16_t id);

/**
 * @brief Query whether a specific fault is currently active.
 *
 * @param id  Fault identifier to query.
 * @return    true if the fault is active, false otherwise.
 */
bool fault_is_active(uint16_t id);

/**
 * @brief Return the highest severity level among all active faults.
 * @return FAULT_LEVEL_NONE if no faults are active.
 */
fault_level_t fault_get_highest_level(void);

/**
 * @brief Periodic health tick for the Fault Manager.
 *
 * Must be called at 1 Hz from the health monitor task.
 * Applies automatic fault ageing / auto-clear policies defined in
 * SPEC-2-FMM §6.
 */
void fault_manager_tick(void);

/**
 * @brief Copy the fault event record for a given fault ID.
 *
 * @param id   Fault identifier.
 * @param out  Destination structure to populate.
 * @return     true if the ID is valid, false otherwise.
 */
bool fault_get_event(uint16_t id, fault_event_t *out);

#ifdef __cplusplus
}
#endif

#endif /* FAULT_MANAGER_H */
