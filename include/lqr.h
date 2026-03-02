/**
 * @file lqr.h
 * @brief Linear Quadratic Regulator — full-state attitude controller.
 *
 * Control law:
 *   u = −K · x
 *
 * State vector (6 × 1):
 *   x = [roll_err, pitch_err, yaw_err,
 *        roll_rate_err, pitch_rate_err, yaw_rate_err]^T   (rad, rad/s)
 *
 * Control output (3 × 1):
 *   u = [torque_roll, torque_pitch, torque_yaw]^T   (N·m)
 *
 * Gain matrix K (3 × 6) is pre-computed offline via LQR Riccati solution
 * for a 1U CubeSat inertia tensor I = diag(0.01, 0.01, 0.005) kg·m² with
 * cost weights Q = diag(100, 100, 100, 1, 1, 1), R = I_3.
 *
 * The default K gives critically-damped closed-loop poles at s = −10 rad/s
 * for each axis (settling time ≈ 0.4 s from 30° initial error).
 *
 * Callers may override K with lqr_set_gains() for different spacecraft
 * configurations.
 *
 * Spec ref: PHASE4_PLAN.md PR-13
 */

#ifndef LQR_H
#define LQR_H

#ifdef __cplusplus
extern "C"
{
#endif

/** LQR state dimension. */
#define LQR_N 6
/** LQR control output dimension. */
#define LQR_M 3

  /**
   * @brief LQR controller instance.
   *
   * Contains only the 3×6 gain matrix.  No internal state — the controller
   * is purely memoryless (proportional to error).
   */
  typedef struct
  {
    float K[LQR_M][LQR_N]; /**< Gain matrix (3×6) */
  } lqr_t;

  /**
   * @brief Initialise LQR with the default 1U CubeSat gain matrix.
   *
   * Default design: I = diag(0.01, 0.01, 0.005) kg·m²,
   *                 ωn = 10 rad/s, ζ = 1 (critically damped) per axis.
   *
   *        K = [ 1.00  0     0     0.20  0     0    ]
   *            [ 0     1.00  0     0     0.20  0    ]
   *            [ 0     0     0.50  0     0     0.10 ]
   */
  void lqr_init(lqr_t *lqr);

  /**
   * @brief Override the gain matrix.
   *
   * @param lqr  Controller instance.
   * @param K    New 3×6 gain matrix (row-major, K[output][state]).
   */
  void lqr_set_gains(lqr_t *lqr, const float K[LQR_M][LQR_N]);

  /**
   * @brief Compute control torques for the given attitude and rate errors.
   *
   * u[i] = − sum_j ( K[i][j] * x[j] )
   *
   * where x = [att_err[0..2], rate_err[0..2]].
   *
   * Output is NOT saturated here; callers are responsible for actuator limits.
   *
   * @param lqr        Controller instance.
   * @param att_err    Attitude error [rad]: target − current  (3-element).
   * @param rate_err   Rate error [rad/s]:  target_rate − current_rate (3-element).
   * @param torque_cmd Output torque commands [N·m] (3-element).
   */
  void lqr_compute(lqr_t *lqr, const float att_err[3], const float rate_err[3],
                   float torque_cmd[3]);

#ifdef __cplusplus
}
#endif

#endif /* LQR_H */
