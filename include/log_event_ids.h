/**
 * @file log_event_ids.h
 * @brief Persistent Log event identifier catalogue.
 *
 * Log event IDs are grouped into three classes (A, B, C) that map
 * directly to @ref log_class_t in logger.h.  The upper nibble of the
 * high byte encodes the class; the remaining bits encode the specific
 * event within that class.
 *
 *   Class A (Critical)     — 0x0001..0x00FF
 *   Class B (Operational)  — 0x0101..0x01FF
 *   Class C (Informational)— 0x0201..0x02FF
 *
 * Spec ref: SPEC-2-PLG v1.16 §3–4, SPEC-2 v2.0 §4.10
 */

#ifndef LOG_EVENT_IDS_H
#define LOG_EVENT_IDS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Class A — Critical events (always persisted, highest priority)     */
/* ------------------------------------------------------------------ */

#define LOG_EVT_SAFE_ENTRY          0x0001u /**< System entered FM_SAFE               */
#define LOG_EVT_BROWNOUT            0x0002u /**< Supply voltage brownout detected      */
#define LOG_EVT_WATCHDOG_RESET      0x0003u /**< Watchdog-triggered MCU reset          */
#define LOG_EVT_EST_DIVERGENCE      0x0004u /**< Attitude estimator divergence          */
#define LOG_EVT_FAULT_CRITICAL      0x0005u /**< A FAULT_LEVEL_CRITICAL was raised      */
#define LOG_EVT_LOG_CORRUPTION      0x0006u /**< Log storage corruption detected        */

/* ------------------------------------------------------------------ */
/* Class B — Operational events (persisted under normal conditions)   */
/* ------------------------------------------------------------------ */

#define LOG_EVT_MODE_CHANGE         0x0101u /**< Flight mode transition completed       */
#define LOG_EVT_ENERGY_CHANGE       0x0102u /**< Energy state changed (EPS)             */
#define LOG_EVT_CMD_CLASS_C         0x0103u /**< Class-C (privileged) command received  */
#define LOG_EVT_FAULT_ERROR         0x0104u /**< A FAULT_LEVEL_ERROR was raised         */

/* ------------------------------------------------------------------ */
/* Class C — Informational events (circular buffer, may be overwritten)*/
/* ------------------------------------------------------------------ */

#define LOG_EVT_SYSTEM_BOOT         0x0201u /**< Normal power-on boot sequence complete */
#define LOG_EVT_CONFIG_APPLIED      0x0202u /**< Configuration parameters accepted       */
#define LOG_EVT_CMD_CLASS_B         0x0203u /**< Class-B command received                */
#define LOG_EVT_LOG_OVERFLOW        0x0204u /**< Log circular buffer wrapped             */

#ifdef __cplusplus
}
#endif

#endif /* LOG_EVENT_IDS_H */
