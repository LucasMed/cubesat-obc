/**
 * @file quaternion.c
 * @brief Unit-quaternion attitude utility implementation
 *
 * Pure math — no RTOS, no hardware dependencies.
 * See include/quaternion.h for API documentation.
 *
 * PR-21 — Phase 6
 */

#include "quaternion.h"

#include <math.h>
#include <string.h>

/* ---- Helpers ------------------------------------------------------------- */

#ifndef M_PI
  #define M_PI 3.14159265358979323846f
#endif

#define Q_NORM_EPSILON 1e-10f /* below this the quaternion is degenerate */

/* ---- Construction -------------------------------------------------------- */

quat_t q_identity(void)
{
  quat_t q = {1.0f, 0.0f, 0.0f, 0.0f};
  return q;
}

/* cppcheck-suppress unusedFunction -- Pico/integration build API */
quat_t q_from_axis_angle(float ax, float ay, float az, float angle_rad)
{
  /* Normalise axis */
  float len = sqrtf(ax * ax + ay * ay + az * az);
  if (len < Q_NORM_EPSILON)
  {
    return q_identity();
  }
  ax /= len;
  ay /= len;
  az /= len;

  float half = angle_rad * 0.5f;
  float s = sinf(half);
  quat_t q = {cosf(half), ax * s, ay * s, az * s};
  return q;
}

quat_t q_from_euler(float roll_rad, float pitch_rad, float yaw_rad)
{
  /* Intrinsic ZYX:  q = qz ⊗ qy ⊗ qx */
  float cr = cosf(roll_rad * 0.5f);
  float sr = sinf(roll_rad * 0.5f);
  float cp = cosf(pitch_rad * 0.5f);
  float sp = sinf(pitch_rad * 0.5f);
  float cy = cosf(yaw_rad * 0.5f);
  float sy = sinf(yaw_rad * 0.5f);

  quat_t q;
  q.w = cr * cp * cy + sr * sp * sy;
  q.x = sr * cp * cy - cr * sp * sy;
  q.y = cr * sp * cy + sr * cp * sy;
  q.z = cr * cp * sy - sr * sp * cy;
  return q;
}

/* ---- Algebra ------------------------------------------------------------- */

quat_t q_normalize(quat_t q)
{
  float n = sqrtf(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
  if (n < Q_NORM_EPSILON)
  {
    return q_identity();
  }
  quat_t r = {q.w / n, q.x / n, q.y / n, q.z / n};
  return r;
}

quat_t q_conj(quat_t q)
{
  quat_t r = {q.w, -q.x, -q.y, -q.z};
  return r;
}

quat_t q_mult(quat_t p, quat_t q)
{
  quat_t r;
  r.w = p.w * q.w - p.x * q.x - p.y * q.y - p.z * q.z;
  r.x = p.w * q.x + p.x * q.w + p.y * q.z - p.z * q.y;
  r.y = p.w * q.y - p.x * q.z + p.y * q.w + p.z * q.x;
  r.z = p.w * q.z + p.x * q.y - p.y * q.x + p.z * q.w;
  return r;
}

/* ---- Rotation ------------------------------------------------------------ */

void q_rotate_vec(quat_t q, const float v[3], float v_out[3])
{
  /* v' = q ⊗ [0,v] ⊗ q*
   * Expanded form avoids constructing a pure quaternion explicitly. */
  float w = q.w;
  float x = q.x;
  float y = q.y;
  float z = q.z;
  float vx = v[0];
  float vy = v[1];
  float vz = v[2];

  /* t = 2 * (q_vec × v) */
  float tx = 2.0f * (y * vz - z * vy);
  float ty = 2.0f * (z * vx - x * vz);
  float tz = 2.0f * (x * vy - y * vx);

  /* v' = v + w*t + q_vec × t */
  v_out[0] = vx + w * tx + y * tz - z * ty;
  v_out[1] = vy + w * ty + z * tx - x * tz;
  v_out[2] = vz + w * tz + x * ty - y * tx;
}

/* ---- Conversion ---------------------------------------------------------- */

void q_to_euler(quat_t q, float *roll_rad, float *pitch_rad, float *yaw_rad)
{
  float w = q.w;
  float x = q.x;
  float y = q.y;
  float z = q.z;

  /* Roll (rotation about X) */
  float sinr_cosp = 2.0f * (w * x + y * z);
  float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
  *roll_rad = atan2f(sinr_cosp, cosr_cosp);

  /* Pitch (rotation about Y) — clamp to avoid NaN at poles and handle singularities */
  float sinp = 2.0f * (w * y - z * x);
  if (fabsf(sinp) >= 0.99999f)
  {
    *pitch_rad = copysignf((float)M_PI / 2.0f, sinp);
  }
  else
  {
    *pitch_rad = asinf(sinp);
  }

  /* Yaw (rotation about Z) */
  float siny_cosp = 2.0f * (w * z + x * y);
  float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
  *yaw_rad = atan2f(siny_cosp, cosy_cosp);
}

/* cppcheck-suppress unusedFunction -- Pico/integration build API */
float q_dot(quat_t p, quat_t q)
{
  return p.w * q.w + p.x * q.x + p.y * q.y + p.z * q.z;
}
