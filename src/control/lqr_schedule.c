/**
 * @file lqr_schedule.c
 * @brief Mode-scheduled LQR gain table implementation.
 *
 * See lqr_schedule.h for the design rationale and gain derivations.
 *
 * The two static gain tables are sized for a 1U CubeSat with
 * I = diag(0.01, 0.01, 0.005) kg·m².
 *
 * FM_NOMINAL (ωn = 10 rad/s, ζ = 1)
 *   Roll/Pitch (I = 0.01):  k_att = I·ωn²     = 1.00 N·m/rad
 *                           k_rate = I·2·ζ·ωn = 0.20 N·m·s/rad
 *   Yaw        (I = 0.005): k_att = 0.50, k_rate = 0.10
 *
 * FM_DETUMBLE (ωn = 30 rad/s, ζ = 1)
 *   Roll/Pitch (I = 0.01):  k_att = I·ωn²     = 9.00 N·m/rad
 *                           k_rate = I·2·ζ·ωn = 0.60 N·m·s/rad
 *   Yaw        (I = 0.005): k_att = 4.50, k_rate = 0.30
 */

#include "lqr_schedule.h"

/* =========================================================================
 * Static gain tables
 * ========================================================================= */

/** FM_NOMINAL: default critically-damped gains (ωn = 10 rad/s). */
static const float K_NOMINAL[LQR_M][LQR_N] = {
    /* torque_roll  */ {1.00f, 0.00f, 0.00f, 0.20f, 0.00f, 0.00f},
    /* torque_pitch */ {0.00f, 1.00f, 0.00f, 0.00f, 0.20f, 0.00f},
    /* torque_yaw   */ {0.00f, 0.00f, 0.50f, 0.00f, 0.00f, 0.10f},
};

/** FM_DETUMBLE: high-bandwidth gains (ωn = 30 rad/s) for fast rate damping. */
static const float K_DETUMBLE[LQR_M][LQR_N] = {
    /* torque_roll  */ {9.00f, 0.00f, 0.00f, 0.60f, 0.00f, 0.00f},
    /* torque_pitch */ {0.00f, 9.00f, 0.00f, 0.00f, 0.60f, 0.00f},
    /* torque_yaw   */ {0.00f, 0.00f, 4.50f, 0.00f, 0.00f, 0.30f},
};

/* =========================================================================
 * Public API
 * ========================================================================= */

void lqr_schedule_apply(lqr_t *lqr, flight_mode_t mode)
{
  const float(*gains)[LQR_N];

  switch (mode)
  {
  case FM_DETUMBLE:
    gains = K_DETUMBLE;
    break;

  case FM_NOMINAL:
  /* Fall through — all other modes use the nominal (safe) gains. */
  case FM_BOOT:
  case FM_SAFE:
  case FM_DIAGNOSTIC:
  case FM_COUNT:
  default:
    gains = K_NOMINAL;
    break;
  }

  lqr_set_gains(lqr, gains);
}
