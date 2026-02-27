/**
 * @file controller_limits.h
 * @brief PID controller gain bounds and control output limits.
 *
 * All values are dimensionless normalised units unless otherwise noted.
 * These limits MUST be enforced at parameter upload time (command
 * handler) and at PID initialisation time.
 *
 * Gain-upload commands that attempt to set values outside these ranges
 * MUST be rejected with FAULT_CMD_UNKNOWN and logged at Class-B
 * (LOG_EVT_CMD_CLASS_C).
 *
 * Spec ref: SPEC-2-CTRL v1.3 §5, SPEC-2 v2.0 §4.3
 */

#ifndef CONTROLLER_LIMITS_H
#define CONTROLLER_LIMITS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* PID proportional gain (Kp)                                         */
/* ------------------------------------------------------------------ */

/** Minimum allowed proportional gain.  Must be ≥ 0. */
#define PID_KP_MIN  0.0f

/** Maximum allowed proportional gain. */
#define PID_KP_MAX  10.0f

/* ------------------------------------------------------------------ */
/* PID integral gain (Ki)                                             */
/* ------------------------------------------------------------------ */

/** Minimum allowed integral gain.  Must be ≥ 0. */
#define PID_KI_MIN  0.0f

/** Maximum allowed integral gain. */
#define PID_KI_MAX  5.0f

/* ------------------------------------------------------------------ */
/* PID derivative gain (Kd)                                           */
/* ------------------------------------------------------------------ */

/** Minimum allowed derivative gain.  Must be ≥ 0. */
#define PID_KD_MIN  0.0f

/** Maximum allowed derivative gain. */
#define PID_KD_MAX  2.0f

/* ------------------------------------------------------------------ */
/* Control output limits                                               */
/* ------------------------------------------------------------------ */

/**
 * @brief Maximum normalised control output magnitude.
 *
 * The PID controller output is clipped to [-U_MAX, +U_MAX] before
 * being scaled to physical actuator commands.  1.0 represents 100 %
 * of available actuator authority.
 */
#define U_MAX  1.0f

/** Minimum (most negative) normalised control output. */
#define U_MIN  (-U_MAX)

/* ------------------------------------------------------------------ */
/* Anti-windup integrator limit                                        */
/* ------------------------------------------------------------------ */

/**
 * @brief Maximum absolute value of the integrator accumulator.
 *
 * Prevents integrator windup during sustained error or saturation
 * intervals.  Expressed in the same normalised units as the setpoint
 * error input.
 */
#define PID_INTEGRATOR_LIMIT  10.0f

/* ------------------------------------------------------------------ */
/* Sanity-check macros                                                 */
/* ------------------------------------------------------------------ */

/** Evaluate to 1 if gain @p k is within the Kp valid range. */
#define PID_KP_VALID(k)  ((k) >= PID_KP_MIN && (k) <= PID_KP_MAX)

/** Evaluate to 1 if gain @p k is within the Ki valid range. */
#define PID_KI_VALID(k)  ((k) >= PID_KI_MIN && (k) <= PID_KI_MAX)

/** Evaluate to 1 if gain @p k is within the Kd valid range. */
#define PID_KD_VALID(k)  ((k) >= PID_KD_MIN && (k) <= PID_KD_MAX)

#ifdef __cplusplus
}
#endif

#endif /* CONTROLLER_LIMITS_H */
