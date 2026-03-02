/**
 * @file momentum_dump.h
 * @brief Momentum dumping (de-tumbling) algorithm via magnetorquer B×L law.
 *
 * Implements the cross-product control law used during FM_DETUMBLE to
 * bleed off excess angular momentum stored in the reaction wheels:
 *
 *   dipole_cmd = -k_dump × (B̂ × L_rw)
 *
 * where:
 *   B̂     = unit vector of the local magnetic field [dimensionless]
 *   L_rw  = reaction wheel angular momentum vector [kg·m²/s]
 *   k_dump = scalar gain [A·m²·s / (kg·m²)]
 *
 * The algorithm is purely computational — it takes inputs and produces
 * a dipole command.  It has no hardware dependency and is fully host-
 * testable.  The caller is responsible for applying the dipole command
 * to the magnetorquer hardware (via magnetorquer_set_moment()).
 *
 * Reference: Wie, B. "Space Vehicle Dynamics and Control", §10.4
 * Spec ref:  SPEC-2-ADCS v1.1 §5.2, PHASE5_PLAN PR-17
 */

#ifndef MOMENTUM_DUMP_H
#define MOMENTUM_DUMP_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Momentum dump controller state.
 */
typedef struct
{
  float k_dump; /**< Control gain [A·m²·s / (kg·m²)] */
} momentum_dump_t;

/**
 * @brief Initialise the momentum dump controller.
 *
 * @param md     Pointer to the controller instance to initialise.
 * @param k_dump Scalar gain.  Positive value; typical range 0.001–0.1.
 */
void momentum_dump_init(momentum_dump_t *md, float k_dump);

/**
 * @brief Compute one momentum-dump step.
 *
 * Evaluates:
 *   dipole_cmd = -k_dump × (B̂ × L_rw)
 *
 * If the magnetic field vector magnitude is below a small epsilon (field
 * measurement unavailable or degenerate), dipole_cmd is zeroed and the
 * function returns without producing a command (safe degradation).
 *
 * @param md         Controller instance (read-only for the gain).
 * @param B          Local magnetic field vector [µT] — any consistent unit.
 * @param L_rw       Reaction wheel angular momentum vector [kg·m²/s].
 * @param dipole_cmd Output magnetic dipole command [A·m²].  Must be a
 *                   3-element float array; overwritten by this call.
 */
void momentum_dump_step(const momentum_dump_t *md, const float B[3], const float L_rw[3],
                        float dipole_cmd[3]);

/**
 * @brief Test whether a momentum dump is needed.
 *
 * Returns true when the magnitude of the reaction wheel angular momentum
 * exceeds @p threshold, indicating that stored momentum should be dumped.
 *
 * @param L_rw      Reaction wheel angular momentum vector [kg·m²/s].
 * @param threshold Positive threshold magnitude [kg·m²/s].
 * @return true  A dump is needed.
 * @return false Angular momentum is within acceptable bounds.
 */
bool momentum_dump_needed(const float L_rw[3], float threshold);

#endif /* MOMENTUM_DUMP_H */
