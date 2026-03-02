/**
 * @file lqr_schedule.h
 * @brief Mode-scheduled LQR gain table.
 *
 * Provides a single call to configure an lqr_t with the gains appropriate
 * for the current flight mode.  The scheduler is a pure look-up table with
 * no dynamic allocation or state.
 *
 * Gain sets
 * ---------
 * FM_NOMINAL  — default 1U CubeSat design (ωn = 10 rad/s, ζ = 1).
 *               K[roll/pitch]: k_att = 1.00, k_rate = 0.20
 *               K[yaw]:        k_att = 0.50, k_rate = 0.10
 *
 * FM_DETUMBLE — high-bandwidth design (ωn = 30 rad/s, ζ = 1).
 *               Faster angular-rate damping while B×L is active.
 *               K[roll/pitch]: k_att = 9.00, k_rate = 0.60
 *               K[yaw]:        k_att = 4.50, k_rate = 0.30
 *
 * All other modes — fall back to the FM_NOMINAL gain set.
 *
 * Spec ref: PHASE6_PLAN.md PR-23
 */

#ifndef LQR_SCHEDULE_H
#define LQR_SCHEDULE_H

#include "flight_mode.h"
#include "lqr.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Apply the scheduled LQR gains for the given flight mode.
   *
   * Writes the mode-specific gain matrix K directly into @p lqr via
   * lqr_set_gains().  If @p mode has no dedicated entry the FM_NOMINAL
   * (default) gain set is used as a safe fallback.
   *
   * @param lqr   LQR controller instance to configure.  Must not be NULL.
   * @param mode  Current flight mode.
   */
  void lqr_schedule_apply(lqr_t *lqr, flight_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* LQR_SCHEDULE_H */
