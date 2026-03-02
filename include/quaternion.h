/**
 * @file quaternion.h
 * @brief Unit-quaternion attitude representation utilities
 *
 * Provides a minimal unit-quaternion library for cross-checking Euler-angle
 * EKF outputs and for future migration to quaternion-state EKF (Phase 7+).
 * All functions are pure math — no RTOS or hardware dependencies.
 *
 * Convention
 * ----------
 *   q = [w, x, y, z]  (Hamilton convention, scalar-first)
 *   Positive rotation: right-hand rule
 *   Euler angles: intrinsic ZYX (yaw ψ, pitch θ, roll φ)
 *     i.e. R = Rz(ψ) · Ry(θ) · Rx(φ)
 *
 * PR-21 — Phase 6 (Closed-Loop Stability & Architectural Consolidation)
 * Tests: T-QAT-01..05 in tests/unit/test_quaternion.c
 */

#ifndef QUATERNION_H
#define QUATERNION_H

#ifdef __cplusplus
extern "C"
{
#endif

  /* ---- Type ---------------------------------------------------------------- */

  /**
   * @brief Unit quaternion  q = w + xi + yj + zk
   *
   * Invariant: w*w + x*x + y*y + z*z == 1  (after normalisation).
   */
  typedef struct
  {
    float w; /**< Scalar part                 */
    float x; /**< i component                 */
    float y; /**< j component                 */
    float z; /**< k component  */
  } quat_t;

  /* ---- Construction -------------------------------------------------------- */

  /**
   * @brief Return the identity quaternion (no rotation).
   */
  quat_t q_identity(void);

  /**
   * @brief Build a quaternion from an axis-angle pair.
   *
   * @param ax  Axis x (need not be unit length — normalised internally)
   * @param ay  Axis y
   * @param az  Axis z
   * @param angle_rad  Rotation angle [rad] (right-hand rule)
   * @return Unit quaternion representing the rotation
   */
  quat_t q_from_axis_angle(float ax, float ay, float az, float angle_rad);

  /**
   * @brief Build a quaternion from intrinsic ZYX Euler angles.
   *
   * @param roll_rad   Rotation about X [rad]
   * @param pitch_rad  Rotation about Y [rad]
   * @param yaw_rad    Rotation about Z [rad]
   * @return Unit quaternion  R = Rz(yaw) · Ry(pitch) · Rx(roll)
   */
  quat_t q_from_euler(float roll_rad, float pitch_rad, float yaw_rad);

  /* ---- Algebra ------------------------------------------------------------- */

  /**
   * @brief Normalise a quaternion to unit length.
   *
   * If the input has near-zero norm the identity quaternion is returned.
   */
  quat_t q_normalize(quat_t q);

  /**
   * @brief Conjugate (== inverse for unit quaternions).
   */
  quat_t q_conj(quat_t q);

  /**
   * @brief Hamilton product  p ⊗ q.
   *
   * Result is NOT automatically normalised; call q_normalize() if accumulated
   * floating-point error matters.
   */
  quat_t q_mult(quat_t p, quat_t q);

  /* ---- Rotation ------------------------------------------------------------ */

  /**
   * @brief Rotate vector [vx, vy, vz] by quaternion q.
   *
   * Computes  v' = q ⊗ [0,v] ⊗ q*
   *
   * @param[in]  q         Unit quaternion representing the rotation
   * @param[in]  v         Input vector (3 elements)
   * @param[out] v_out     Rotated vector (3 elements, may alias v)
   */
  void q_rotate_vec(quat_t q, const float v[3], float v_out[3]);

  /* ---- Conversion ---------------------------------------------------------- */

  /**
   * @brief Extract intrinsic ZYX Euler angles from a unit quaternion.
   *
   * @param[in]  q           Unit quaternion
   * @param[out] roll_rad    Rotation about X [rad]  ∈ (-π, +π]
   * @param[out] pitch_rad   Rotation about Y [rad]  ∈ [-π/2, +π/2]
   * @param[out] yaw_rad     Rotation about Z [rad]  ∈ (-π, +π]
   */
  void q_to_euler(quat_t q, float *roll_rad, float *pitch_rad, float *yaw_rad);

  /**
   * @brief Dot product of two quaternions (used for angle-between).
   *
   * @return  w0*w1 + x0*x1 + y0*y1 + z0*z1  (always in [-1, +1] for unit q)
   */
  float q_dot(quat_t p, quat_t q);

#ifdef __cplusplus
}
#endif

#endif /* QUATERNION_H */
