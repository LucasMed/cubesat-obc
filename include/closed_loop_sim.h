/**
 * @file closed_loop_sim.h
 * @brief Host-only closed-loop simulation harness: EKF → LQR → dynamics.
 *
 * Propagates the full attitude estimation and control pipeline in a
 * deterministic discrete-time loop so that stability metrics (settling
 * time, torque saturation, gyro-bias convergence, momentum accumulation)
 * can be asserted in unit tests without hardware.
 *
 * Simulation model (one step, dt seconds):
 *   1. Compute ideal body-frame gyro from true rates + injected bias
 *   2. Compute ideal body-frame accel from true attitude (gravity projection)
 *   3. Compute ideal body-frame mag  from true attitude (North-field rotation)
 *   4. ekf_predict(gyro, dt)
 *   5. ekf_update(accel)
 *   6. ekf_update_mag(mag, OBC_MAG_DECLINATION_RAD)
 *   7. Compute bias-corrected rate estimate = gyro - EKF bias estimate
 *   8. lqr_schedule_apply(mode) then lqr_compute(att_err, rate_err)
 *   9. Saturate each torque axis at ±tau_sat
 *  10. Accumulate reaction-wheel momentum: rw_h += torque * dt
 *  11. attitude_dynamics_step(torque, dt)
 *
 * Only `ekf.c`, `lqr.c`, `lqr_schedule.c`, and `attitude_dynamics.c` are
 * compiled in; no FreeRTOS or hardware dependency.
 *
 * Spec ref: PHASE6_PLAN.md PR-24
 */

#ifndef CLOSED_LOOP_SIM_H
#define CLOSED_LOOP_SIM_H

#include "attitude_dynamics.h"
#include "ekf.h"
#include "flight_mode.h"
#include "lqr.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* ---- Simulation constants ---------------------------------------------- */

/** Default time step [s] — matches the 20 Hz control loop. */
#define CLS_DT 0.05f

/** Per-axis torque saturation limit [N·m] — model a 1U reaction wheel. */
#define CLS_TAU_SAT 1.0e-3f

/** Reaction-wheel momentum threshold that would trigger a dump [N·m·s]. */
#define CLS_MOM_THRESH 5.0e-4f

/** Reference horizontal magnetic-field magnitude [µT]. */
#define CLS_MAG_B0 47.0f

/** Gravitational acceleration used for accel sensor model [m/s²]. */
#define CLS_GRAVITY 9.81f

  /* ---- Simulation state -------------------------------------------------- */

  /**
   * @brief Closed-loop simulation instance.
   *
   * All memory lives on the caller's stack — no heap allocation.
   */
  typedef struct
  {
    ekf_t ekf;             /**< Extended Kalman filter              */
    lqr_t lqr;             /**< LQR controller (gains set per step) */
    attitude_dyn_t dyn;    /**< True attitude / rate state          */
    float true_bias[3];    /**< Injected gyro bias [rad/s]          */
    float rw_momentum[3];  /**< Accumulated RW momentum [N·m·s]     */
    float dt;              /**< Time step [s]                       */
    float tau_sat;         /**< Per-axis torque saturation [N·m]    */
    float tau_max_applied; /**< Peak |torque| applied to dyn [N·m]  */
    int momentum_exceeded; /**< 1 once |rw_h| > CLS_MOM_THRESH     */
  } cls_t;

  /* ---- Public API -------------------------------------------------------- */

  /**
   * @brief Initialise the simulation.
   *
   * Resets the EKF (cold start), LQR (nominal gains), and dynamics, then
   * sets the true initial attitude, gyro bias, and simulation parameters.
   *
   * @param sim     Simulation instance to initialise.
   * @param roll0   Initial true roll  [rad].
   * @param pitch0  Initial true pitch [rad].
   * @param yaw0    Initial true yaw   [rad].
   * @param bias    Initial gyro bias to inject [rad/s] (3-element), may be NULL
   *                (treated as zero).
   * @param dt      Time step [s].  Pass CLS_DT for the default 20 Hz loop.
   * @param tau_sat Per-axis torque saturation limit [N·m].  Pass CLS_TAU_SAT.
   */
  void cls_init(cls_t *sim, float roll0, float pitch0, float yaw0, const float bias[3], float dt,
                float tau_sat);

  /**
   * @brief Advance the simulation by one time step.
   *
   * Sensor models → EKF → LQR (scheduled for @p mode) → actuator saturation
   * → reaction-wheel momentum accumulation → attitude dynamics.
   *
   * @param sim   Simulation instance.
   * @param mode  Current flight mode (selects LQR gain set).
   */
  void cls_step(cls_t *sim, flight_mode_t mode);

  /**
   * @brief Run @p n_steps consecutive steps.
   *
   * @param sim     Simulation instance.
   * @param mode    Flight mode applied to every step.
   * @param n_steps Number of steps to execute.
   */
  void cls_run(cls_t *sim, flight_mode_t mode, int n_steps);

#ifdef __cplusplus
}
#endif

#endif /* CLOSED_LOOP_SIM_H */
