/**
 * @file ekf.h
 * @brief Extended Kalman Filter — attitude estimation with gyro-bias correction
 *        and yaw update via magnetometer (PR-19).
 *
 * State vector (6 × 1):
 *   x = [roll, pitch, yaw, bias_x, bias_y, bias_z]^T   (rad, rad/s)
 *
 * Process model (continuous):
 *   attitude_dot[i] = gyro_measured[i] - bias[i]
 *   bias_dot[i]     = 0   (random-walk, driven by Q)
 *
 * Measurement models:
 *   Accel  (M=2): z = [roll_accel, pitch_accel]
 *     H = [1 0 0 0 0 0]
 *         [0 1 0 0 0 0]
 *
 *   Mag (M=1, yaw): z = yaw_tilt_compensated
 *     H = [0 0 1 0 0 0]
 *     Tilt compensation uses current roll/pitch from x[0], x[1].
 *
 * Spec ref: PHASE4_PLAN.md PR-12, PHASE5_PLAN.md PR-19
 */

#ifndef EKF_H
#define EKF_H

#ifdef __cplusplus
extern "C"
{
#endif

/** State and measurement dimensions. */
#define EKF_N 6 /**< state:       [roll, pitch, yaw, bx, by, bz] */
#define EKF_M 2 /**< measurement: [roll_accel, pitch_accel]       */

  /**
   * @brief EKF instance.  All memory is on the stack — no heap allocation.
   */
  typedef struct
  {
    float x[EKF_N];        /**< State estimate                         */
    float P[EKF_N][EKF_N]; /**< State covariance                       */
    float Q[EKF_N][EKF_N]; /**< Process noise covariance (diagonal)    */
    float R[EKF_M][EKF_M]; /**< Accel measurement noise covariance     */
    float r_mag;           /**< Magnetometer yaw measurement noise [rad²] */
  } ekf_t;

  /**
   * @brief Initialise EKF with default noise parameters.
   *
   *   Q_att  = 1e-4 rad²/step   (gyro noise ~0.01 rad/s @ 10 Hz)
   *   Q_bias = 1e-6 (rad/s)²/step (slow random walk)
   *   R_att  = 1e-2 rad²         (accel tilt noise ~0.06 rad rms)
   *   P0_att = 1.0  rad²         (large initial uncertainty)
   *   P0_bias= 1e-2 (rad/s)²
   */
  void ekf_init(ekf_t *ekf);

  /**
   * @brief Prediction step: propagate state and covariance using gyro input.
   *
   * @param ekf   Filter instance.
   * @param gyro  Measured body-frame angular rate [rad/s] (3-element).
   * @param dt    Time step [s].
   */
  void ekf_predict(ekf_t *ekf, const float gyro[3], float dt);

  /**
   * @brief Update step: correct state using accelerometer measurement.
   *
   * Derives roll and pitch via:
   *   roll  = atan2(ay, az)
   *   pitch = atan2(-ax, sqrt(ay² + az²))
   *
   * Skips the update if the accelerometer vector is degenerate (near zero).
   *
   * @param ekf    Filter instance.
   * @param accel  Body-frame specific force [m/s²] (3-element, ~9.81 at rest).
   */
  void ekf_update(ekf_t *ekf, const float accel[3]);

  /**
   * @brief Read back the estimated attitude [rad].
   * @param att  Output: [roll, pitch, yaw].
   */
  void ekf_get_attitude(const ekf_t *ekf, float att[3]);

  /**
   * @brief Read back the estimated gyro bias [rad/s].
   * @param bias  Output: [bx, by, bz].
   */
  void ekf_get_bias(const ekf_t *ekf, float bias[3]);

  /**
   * @brief Yaw update step: correct yaw using tilt-compensated magnetometer.
   *
   * Uses the current roll/pitch estimates (x[0], x[1]) to project the
   * raw magnetic field onto the horizontal plane, then computes:
   *
   *   Bh_x = Bx*cos(p) + By*sin(r)*sin(p) + Bz*cos(r)*sin(p)
   *   Bh_y = By*cos(r) - Bz*sin(r)
   *   yaw_meas = atan2(-Bh_y, Bh_x) + declination
   *
   * H = [0 0 1 0 0 0] — scalar measurement.
   * Innovation is wrapped to [-π, +π] before the update.
   * Skipped if the horizontal field magnitude is below a small epsilon
   * (degenerate field or sensor not available).
   *
   * @param ekf            Filter instance.
   * @param mag_field_uT   Body-frame magnetic field vector [µT] (3-element).
   * @param declination_rad Magnetic declination at current location [rad].
   *                        Use 0.0f if declination is unknown.
   */
  void ekf_update_mag(ekf_t *ekf, const float mag_field_uT[3], float declination_rad);

#ifdef __cplusplus
}
#endif

#endif /* EKF_H */
