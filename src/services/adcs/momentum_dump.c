/**
 * @file momentum_dump.c
 * @brief Momentum dumping algorithm — B×L cross-product control law.
 *
 * Spec ref: SPEC-2-ADCS v1.1 §5.2, PHASE5_PLAN PR-17
 */

#include "momentum_dump.h"

#include <math.h>
#include <string.h>

/** Minimum magnetic field magnitude [µT] below which the field is considered
 *  degenerate and no dipole command is issued. */
#define B_EPSILON 1.0f

void momentum_dump_init(momentum_dump_t *md, float k_dump)
{
  md->k_dump = k_dump;
}

void momentum_dump_step(const momentum_dump_t *md, const float B[3], const float L_rw[3],
                        float dipole_cmd[3])
{
  /* Zero the output unconditionally so the caller always receives a
   * well-defined vector even in the degenerate case. */
  dipole_cmd[0] = 0.0f;
  dipole_cmd[1] = 0.0f;
  dipole_cmd[2] = 0.0f;

  /* Compute field magnitude; bail out if field is degenerate. */
  float B_mag = sqrtf(B[0] * B[0] + B[1] * B[1] + B[2] * B[2]);
  if (B_mag < B_EPSILON)
  {
    return;
  }

  /* Normalise field vector. */
  float B_hat[3];
  B_hat[0] = B[0] / B_mag;
  B_hat[1] = B[1] / B_mag;
  B_hat[2] = B[2] / B_mag;

  /* Cross product: cross = B_hat × L_rw */
  float cross[3];
  cross[0] = B_hat[1] * L_rw[2] - B_hat[2] * L_rw[1];
  cross[1] = B_hat[2] * L_rw[0] - B_hat[0] * L_rw[2];
  cross[2] = B_hat[0] * L_rw[1] - B_hat[1] * L_rw[0];

  /* Apply gain and negate: dipole_cmd = -k_dump * (B_hat × L_rw) */
  dipole_cmd[0] = -md->k_dump * cross[0];
  dipole_cmd[1] = -md->k_dump * cross[1];
  dipole_cmd[2] = -md->k_dump * cross[2];
}

bool momentum_dump_needed(const float L_rw[3], float threshold)
{
  float L_mag_sq = L_rw[0] * L_rw[0] + L_rw[1] * L_rw[1] + L_rw[2] * L_rw[2];
  return L_mag_sq > (threshold * threshold);
}
