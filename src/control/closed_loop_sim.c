/**
 * @file closed_loop_sim.c
 * @brief Host-only closed-loop simulation harness implementation.
 *
 * See closed_loop_sim.h for the full model description.
 *
 * Sensor models
 * -------------
 * All sensor outputs are derived from the dynamics true state so the EKF
 * sees a physically consistent world (no artificial noise in the default
 * harness — noise is tested in the EKF unit tests separately).
 *
 * Accelerometer (specific force in body frame):
 *   ax = g  · ( -sin(pitch) )
 *   ay = g  · ( cos(pitch) · sin(roll) )
 *   az = g  · ( cos(pitch) · cos(roll) )
 *
 * Magnetometer (world North = [B0, 0, 0], rotated to body via ZYX Euler R^T):
 *   Bx = B0 · cos(yaw)·cos(pitch)
 *   By = B0 · ( cos(yaw)·sin(pitch)·sin(roll) − sin(yaw)·cos(roll) )
 *   Bz = B0 · ( cos(yaw)·sin(pitch)·cos(roll) + sin(yaw)·sin(roll) )
 *
 * For roll = pitch = 0: Bx = B0·cos(yaw), By = −B0·sin(yaw), Bz = 0
 * → matches make_mag() in test_ekf_mag.c; ekf_update_mag recovers yaw ✓
 *
 * Reaction-wheel momentum
 * -----------------------
 * Accumulated per-axis: rw_h[i] += torque_applied[i] * dt
 * Represents the angular momentum absorbed by the reaction wheels.
 * A real dump would fire magnetorquers when |rw_h| > threshold; here we
 * just set the exceeded flag for T-CLS-06.
 */

#include "closed_loop_sim.h"

#include "config.h"
#include "lqr_schedule.h"

#include <math.h>
#include <string.h>

/* =========================================================================
 * Internal helpers
 * ========================================================================= */

/**
 * @brief Compute ideal body-frame specific force from true attitude [rad].
 *
 * @param attitude  [roll, pitch, yaw] in rad.
 * @param a_out     Output accel vector [m/s²].
 */
static void sim_make_accel(const float attitude[3], float a_out[3])
{
  const float roll = attitude[0];
  const float pitch = attitude[1];
  /* yaw has no effect on gravity projection for a level platform */
  const float g = CLS_GRAVITY;

  a_out[0] = g * (-sinf(pitch));
  a_out[1] = g * (cosf(pitch) * sinf(roll));
  a_out[2] = g * (cosf(pitch) * cosf(roll));
}

/**
 * @brief Compute ideal body-frame magnetic field from true attitude [rad].
 *
 * World-frame reference field: B_world = [B0, 0, 0] (pointing North, horizontal).
 * Body-frame field: B_body = R_body_from_world · B_world = R^T · B_world.
 * Using ZYX Euler R_{world←body} rows, B_body = B0 · R^T column-0 = B0 · R row-0.
 *
 * @param attitude  [roll, pitch, yaw] in rad.
 * @param b_out     Output field vector [µT].
 */
static void sim_make_mag(const float attitude[3], float b_out[3])
{
  const float roll = attitude[0];
  const float pitch = attitude[1];
  const float yaw = attitude[2];

  const float cr = cosf(roll);
  const float sr = sinf(roll);
  const float cp = cosf(pitch);
  const float sp = sinf(pitch);
  const float cy = cosf(yaw);
  const float sy = sinf(yaw);

  const float B0 = CLS_MAG_B0;

  /* R[0][0..2] from ZYX Euler rotation matrix R = Rz(yaw)·Ry(pitch)·Rx(roll) */
  b_out[0] = B0 * (cy * cp);
  b_out[1] = B0 * (cy * sp * sr - sy * cr);
  b_out[2] = B0 * (cy * sp * cr + sy * sr);
}

/**
 * @brief Saturate a value to [-limit, +limit].
 */
static float saturate(float v, float limit)
{
  if (v > limit)
    return limit;
  if (v < -limit)
    return -limit;
  return v;
}

/**
 * @brief Return the magnitude of a 3-vector.
 */
static float vec3_norm(const float v[3])
{
  return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

/* =========================================================================
 * Public API
 * ========================================================================= */

void cls_init(cls_t *sim, float roll0, float pitch0, float yaw0, const float bias[3], float dt,
              float tau_sat)
{
  memset(sim, 0, sizeof(*sim));

  /* EKF: cold start (zero attitude, large initial covariance) */
  ekf_init(&sim->ekf);

  /* LQR: nominal gains as default — overwritten each step by lqr_schedule */
  lqr_init(&sim->lqr);

  /* Dynamics: set true initial attitude and default inertia */
  attitude_dynamics_init(&sim->dyn);
  sim->dyn.attitude[0] = roll0;
  sim->dyn.attitude[1] = pitch0;
  sim->dyn.attitude[2] = yaw0;
  /* rates start at zero */

  /* Gyro bias */
  if (bias != NULL)
  {
    sim->true_bias[0] = bias[0];
    sim->true_bias[1] = bias[1];
    sim->true_bias[2] = bias[2];
  }

  sim->dt = dt;
  sim->tau_sat = tau_sat;
}

void cls_step(cls_t *sim, flight_mode_t mode)
{
  /* 1. Gyro measurement = true rate + injected bias */
  float gyro[3];
  gyro[0] = sim->dyn.rates[0] + sim->true_bias[0];
  gyro[1] = sim->dyn.rates[1] + sim->true_bias[1];
  gyro[2] = sim->dyn.rates[2] + sim->true_bias[2];

  /* 2. Accelerometer model from true attitude */
  float accel[3];
  sim_make_accel(sim->dyn.attitude, accel);

  /* 3. Magnetometer model from true attitude */
  float mag[3];
  sim_make_mag(sim->dyn.attitude, mag);

  /* 4–6. EKF cycle */
  ekf_predict(&sim->ekf, gyro, sim->dt);
  ekf_update(&sim->ekf, accel);
  ekf_update_mag(&sim->ekf, mag, OBC_MAG_DECLINATION_RAD);

  /* 7. Bias-corrected rate estimate from EKF */
  float rate_corr[3];
  rate_corr[0] = gyro[0] - sim->ekf.x[3];
  rate_corr[1] = gyro[1] - sim->ekf.x[4];
  rate_corr[2] = gyro[2] - sim->ekf.x[5];

  /* 8. LQR: apply scheduled gains, then compute torque command */
  lqr_schedule_apply(&sim->lqr, mode);

  /* att_err = EKF attitude estimate - target (target = [0,0,0]) */
  float att_err[3] = {sim->ekf.x[0], sim->ekf.x[1], sim->ekf.x[2]};
  float torque_cmd[3];
  lqr_compute(&sim->lqr, att_err, rate_corr, torque_cmd);

  /* 9. Saturate each torque axis */
  float torque_applied[3];
  for (int i = 0; i < 3; i++)
  {
    torque_applied[i] = saturate(torque_cmd[i], sim->tau_sat);
  }

  /* Track peak torque magnitude */
  float tau_norm = vec3_norm(torque_applied);
  if (tau_norm > sim->tau_max_applied)
  {
    sim->tau_max_applied = tau_norm;
  }

  /* 10. Accumulate reaction-wheel angular momentum */
  for (int i = 0; i < 3; i++)
  {
    sim->rw_momentum[i] += torque_applied[i] * sim->dt;
  }
  if (vec3_norm(sim->rw_momentum) > CLS_MOM_THRESH)
  {
    sim->momentum_exceeded = 1;
  }

  /* 11. Propagate true attitude dynamics */
  attitude_dynamics_step(&sim->dyn, torque_applied, sim->dt);
}

void cls_run(cls_t *sim, flight_mode_t mode, int n_steps)
{
  for (int k = 0; k < n_steps; k++)
  {
    cls_step(sim, mode);
  }
}
