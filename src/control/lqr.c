/**
 * @file lqr.c
 * @brief Linear Quadratic Regulator — full-state attitude controller.
 *
 * See lqr.h for the design specification and gain derivation.
 *
 * The controller is stateless: u = −K·x.  All computation fits in a
 * handful of multiplications with no dynamic allocation.
 */

#include "lqr.h"

#include <string.h>

/* =========================================================================
 * Default gain matrix
 *
 * Designed for:  I = diag(0.01, 0.01, 0.005) kg·m²
 *                ωn = 10 rad/s,  ζ = 1  (critically damped)
 *
 * Derivation per axis (i):
 *   Open-loop: I_i * e_ddot = u_i
 *   Control:   u_i = −k_att_i * e_i − k_rate_i * e_dot_i
 *   CL char:   s² + (k_rate/I)*s + (k_att/I) = 0
 *   Targets:   k_att/I  = ωn²       → k_att  = I * ωn²
 *              k_rate/I = 2·ζ·ωn   → k_rate = I * 2·ζ·ωn
 *
 * Roll / Pitch (I = 0.01 kg·m²):
 *   k_att  = 0.01 × 100 = 1.00 N·m/rad
 *   k_rate = 0.01 ×  20 = 0.20 N·m·s/rad
 *
 * Yaw (I = 0.005 kg·m²):
 *   k_att  = 0.005 × 100 = 0.50 N·m/rad
 *   k_rate = 0.005 ×  20 = 0.10 N·m·s/rad
 *
 * K columns: [e_roll, e_pitch, e_yaw, e_droll, e_dpitch, e_dyaw]
 * ========================================================================= */
static const float K_DEFAULT[LQR_M][LQR_N] = {
    /* torque_roll  */ {1.00f, 0.00f, 0.00f, 0.20f, 0.00f, 0.00f},
    /* torque_pitch */ {0.00f, 1.00f, 0.00f, 0.00f, 0.20f, 0.00f},
    /* torque_yaw   */ {0.00f, 0.00f, 0.50f, 0.00f, 0.00f, 0.10f},
};

/* =========================================================================
 * Public API
 * ========================================================================= */

void lqr_init(lqr_t *lqr)
{
  (void)memcpy(lqr->K, K_DEFAULT, sizeof(K_DEFAULT));
}

void lqr_set_gains(lqr_t *lqr, const float K[LQR_M][LQR_N])
{
  (void)memcpy(lqr->K, K, sizeof(lqr->K));
}

void lqr_compute(lqr_t *lqr, const float att_err[3], const float rate_err[3], float torque_cmd[3])
{
  /* Build full state error vector x = [att_err | rate_err] */
  float x[LQR_N];
  x[0] = att_err[0];
  x[1] = att_err[1];
  x[2] = att_err[2];
  x[3] = rate_err[0];
  x[4] = rate_err[1];
  x[5] = rate_err[2];

  /* u[i] = −K[i] · x */
  for (int i = 0; i < LQR_M; i++)
  {
    float u = 0.0f;
    for (int j = 0; j < LQR_N; j++)
    {
      u += lqr->K[i][j] * x[j];
    }
    torque_cmd[i] = -u;
  }
}
