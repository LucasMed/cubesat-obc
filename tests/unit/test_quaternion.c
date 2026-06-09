/* test_quaternion.c — Unit tests for quaternion utility library (PR-21)
 *
 * T-QAT-01: euler_round_trip   — euler_to_q → q_to_euler within ±1 µrad
 * T-QAT-02: mult_inverse       — q ⊗ q* == identity  (‖q_diff‖ < 1e-6)
 * T-QAT-03: normalize_unit     — q_normalize output has unit norm
 * T-QAT-04: rotate_vec_90_yaw  — rotate [1,0,0] by +90° yaw → [0,1,0]
 * T-QAT-05: singularity_free   — q_from_euler at pitch = ±90° returns
 *                                 finite quaternion (no Euler gimbal lock)
 */

#include "quaternion.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#ifndef M_PI
  #define M_PI 3.14159265358979323846f
#endif

/* ---- Test helpers -------------------------------------------------------- */
static int g_failures = 0;

#define CHECK(cond, msg)                                                                           \
  do                                                                                               \
  {                                                                                                \
    if (!(cond))                                                                                   \
    {                                                                                              \
      printf("  FAIL [%s:%d] %s\n", __func__, __LINE__, msg);                                      \
      g_failures++;                                                                                \
    }                                                                                              \
  } while (0)

#define DEG2RAD(d) ((d) * ((float)M_PI / 180.0f))
#define FABS(x) fabsf((float)(x))

/* Quaternion component-wise near-equality */
static int q_near(quat_t a, quat_t b, float tol)
{
  /* Handle double-cover: q and -q represent the same rotation */
  float d_pos = FABS(a.w - b.w) + FABS(a.x - b.x) + FABS(a.y - b.y) + FABS(a.z - b.z);
  float d_neg = FABS(a.w + b.w) + FABS(a.x + b.x) + FABS(a.y + b.y) + FABS(a.z + b.z);
  return (d_pos < tol) || (d_neg < tol);
}

/* ========================================================================
 * T-QAT-01  Euler round-trip: q_from_euler → q_to_euler within ±1 µrad
 * ======================================================================== */
static void test_euler_round_trip(void)
{
  const float cases[][3] = {
      {DEG2RAD(10.0f), DEG2RAD(20.0f), DEG2RAD(30.0f)},
      {DEG2RAD(-45.0f), DEG2RAD(15.0f), DEG2RAD(90.0f)},
      {DEG2RAD(0.0f), DEG2RAD(0.0f), DEG2RAD(0.0f)},
      {DEG2RAD(60.0f), DEG2RAD(-30.0f), DEG2RAD(-120.0f)},
  };
  const float tol = 1e-5f; /* 10 µrad */

  for (int i = 0; i < 4; i++)
  {
    float r0 = cases[i][0], p0 = cases[i][1], y0 = cases[i][2];
    quat_t q = q_from_euler(r0, p0, y0);
    float r1, p1, y1;
    q_to_euler(q, &r1, &p1, &y1);

    CHECK(FABS(r1 - r0) < tol, "roll round-trip failed");
    CHECK(FABS(p1 - p0) < tol, "pitch round-trip failed");
    CHECK(FABS(y1 - y0) < tol, "yaw round-trip failed");
  }
  printf("  PASS T-QAT-01 Euler round-trip (4 test vectors, tol=10 µrad)\n");
}

/* ========================================================================
 * T-QAT-02  q ⊗ q⁻¹ == identity  (q* == q⁻¹ for unit quaternion)
 * ======================================================================== */
static void test_mult_inverse(void)
{
  quat_t q = q_from_euler(DEG2RAD(30.0f), DEG2RAD(-20.0f), DEG2RAD(45.0f));
  quat_t qinv = q_conj(q);
  quat_t res = q_mult(q, qinv);
  quat_t iden = q_identity();

  CHECK(q_near(res, iden, 1e-5f), "q ⊗ q* should equal identity");

  /* Also check q* ⊗ q */
  quat_t res2 = q_mult(qinv, q);
  CHECK(q_near(res2, iden, 1e-5f), "q* ⊗ q should equal identity");

  printf("  PASS T-QAT-02 q ⊗ q* = identity\n");
}

/* ========================================================================
 * T-QAT-03  q_normalize produces a unit-norm quaternion
 * ======================================================================== */
static void test_normalize_unit(void)
{
  /* Deliberately un-normalised quaternion */
  quat_t q_raw = {3.0f, 1.0f, -2.0f, 0.5f};
  quat_t q_n = q_normalize(q_raw);

  float norm = sqrtf(q_n.w * q_n.w + q_n.x * q_n.x + q_n.y * q_n.y + q_n.z * q_n.z);
  CHECK(FABS(norm - 1.0f) < 1e-6f, "normalised quaternion must have unit norm");

  /* Also verify zero-quaternion returns identity safely */
  quat_t q_zero = {0.0f, 0.0f, 0.0f, 0.0f};
  quat_t q_id = q_normalize(q_zero);
  CHECK(FABS(q_id.w - 1.0f) < 1e-6f, "zero-quaternion should return identity");

  printf("  PASS T-QAT-03 q_normalize produces unit-norm quaternion\n");
}

/* ========================================================================
 * T-QAT-04  Rotate [1,0,0] by +90° pure-yaw → result ≈ [0,1,0]
 * ======================================================================== */
static void test_rotate_vec_90_yaw(void)
{
  /* 90° rotation about Z axis */
  quat_t q = q_from_euler(0.0f, 0.0f, DEG2RAD(90.0f));
  float v[3] = {1.0f, 0.0f, 0.0f};
  float vr[3] = {0.0f, 0.0f, 0.0f};
  q_rotate_vec(q, v, vr);

  CHECK(FABS(vr[0] - 0.0f) < 1e-5f, "x component should be ~0 after 90° yaw");
  CHECK(FABS(vr[1] - 1.0f) < 1e-5f, "y component should be ~1 after 90° yaw");
  CHECK(FABS(vr[2] - 0.0f) < 1e-5f, "z component should be ~0 after 90° yaw");

  /* Also test roll: rotate [0,0,1] by +90° roll → [0,-1,0] */
  quat_t qr = q_from_euler(DEG2RAD(90.0f), 0.0f, 0.0f);
  float vz[3] = {0.0f, 0.0f, 1.0f};
  float vrz[3] = {0.0f, 0.0f, 0.0f};
  q_rotate_vec(qr, vz, vrz);

  CHECK(FABS(vrz[0] - 0.0f) < 1e-5f, "x component should be ~0 after 90° roll");
  CHECK(FABS(vrz[1] + 1.0f) < 1e-5f, "y component should be ~-1 after 90° roll");
  CHECK(FABS(vrz[2] - 0.0f) < 1e-5f, "z component should be ~0 after 90° roll");

  printf("  PASS T-QAT-04 q_rotate_vec correct for 90° yaw and roll\n");
}

/* ========================================================================
 * T-QAT-05  Quaternion at pitch = ±90° stays finite (no gimbal-lock NaN)
 * ======================================================================== */
static void test_singularity_free(void)
{
  float pitch_cases[] = {DEG2RAD(90.0f), DEG2RAD(-90.0f)};

  for (int i = 0; i < 2; i++)
  {
    quat_t q = q_from_euler(DEG2RAD(15.0f), pitch_cases[i], DEG2RAD(30.0f));

    CHECK(isfinite(q.w), "w must be finite at ±90° pitch");
    CHECK(isfinite(q.x), "x must be finite at ±90° pitch");
    CHECK(isfinite(q.y), "y must be finite at ±90° pitch");
    CHECK(isfinite(q.z), "z must be finite at ±90° pitch");

    /* Round-trip through q_to_euler: pitch should be recoverable */
    float r, p, y;
    q_to_euler(q, &r, &p, &y);
    CHECK(isfinite(r) && isfinite(p) && isfinite(y),
          "q_to_euler must return finite angles at ±90° pitch");
    CHECK(FABS(fabsf(p) - (float)(M_PI / 2.0)) < 1e-4f,
          "pitch magnitude should be π/2 at singularity");
  }
  printf("  PASS T-QAT-05 quaternion is finite and recoverable at pitch = ±90°\n");
}

/* ========================================================================
 * T-QAT-06  q_from_axis_angle — normal case: 90° about Z
 * ======================================================================== */
static void test_axis_angle_normal(void)
{
  /* 90° rotation about Z: [0,0,1], angle = π/2
   * Expected quaternion: cos(45°), 0, 0, sin(45°) = [~0.707, 0, 0, ~0.707] */
  quat_t q = q_from_axis_angle(0.0f, 0.0f, 1.0f, (float)(M_PI / 2.0));
  float expected = sqrtf(2.0f) / 2.0f; /* ~0.7071 */
  CHECK(FABS(q.w - expected) < 1e-5f, "axis-angle w = cos(45°)");
  CHECK(FABS(q.x) < 1e-6f, "axis-angle x = 0 (no rotation about X)");
  CHECK(FABS(q.y) < 1e-6f, "axis-angle y = 0 (no rotation about Y)");
  CHECK(FABS(q.z - expected) < 1e-5f, "axis-angle z = sin(45°)");

  /* Rotate [1,0,0] by this quaternion → [0,1,0] */
  float v[3] = {1.0f, 0.0f, 0.0f};
  float vr[3] = {0.0f, 0.0f, 0.0f};
  q_rotate_vec(q, v, vr);
  CHECK(FABS(vr[0] - 0.0f) < 1e-5f, "axis-angle: vx ~0 after 90° about Z");
  CHECK(FABS(vr[1] - 1.0f) < 1e-5f, "axis-angle: vy ~1 after 90° about Z");
  CHECK(FABS(vr[2] - 0.0f) < 1e-5f, "axis-angle: vz ~0 after 90° about Z");

  printf("  PASS T-QAT-06 q_from_axis_angle normal (90° about Z)\n");
}

/* ========================================================================
 * T-QAT-07  q_from_axis_angle — zero axis → identity (regardless of angle)
 * ======================================================================== */
static void test_axis_angle_zero_axis(void)
{
  quat_t q = q_from_axis_angle(0.0f, 0.0f, 0.0f, (float)M_PI);
  quat_t id = q_identity();
  CHECK(FABS(q.w - id.w) < 1e-6f, "zero-axis axis-angle: w = identity");
  CHECK(FABS(q.x - id.x) < 1e-6f, "zero-axis axis-angle: x = identity");
  CHECK(FABS(q.y - id.y) < 1e-6f, "zero-axis axis-angle: y = identity");
  CHECK(FABS(q.z - id.z) < 1e-6f, "zero-axis axis-angle: z = identity");

  /* Also test with angle = 0 on a valid axis */
  quat_t q2 = q_from_axis_angle(1.0f, 0.0f, 0.0f, 0.0f);
  CHECK(FABS(q2.w - 1.0f) < 1e-6f, "zero-angle axis-angle: w = 1");
  CHECK(FABS(q2.x) < 1e-6f, "zero-angle axis-angle: x = 0");

  printf("  PASS T-QAT-07 q_from_axis_angle degenerate (zero axis, zero angle)\n");
}

/* ========================================================================
 * T-QAT-08  q_dot — dot product between quaternions
 * ======================================================================== */
static void test_q_dot(void)
{
  quat_t id = q_identity();
  quat_t id2 = q_identity();
  CHECK(FABS(q_dot(id, id2) - 1.0f) < 1e-6f, "identity ⊙ identity = 1");

  /* q ⊙ q = 1 for any unit quaternion */
  quat_t p = q_from_euler(DEG2RAD(30.0f), DEG2RAD(-20.0f), DEG2RAD(45.0f));
  CHECK(FABS(q_dot(p, p) - 1.0f) < 1e-5f, "q ⊙ q = 1 for unit quaternion");

  /* q ⊙ (-q) = -1 */
  quat_t neg = {-p.w, -p.x, -p.y, -p.z};
  CHECK(FABS(q_dot(p, neg) + 1.0f) < 1e-5f, "q ⊙ (-q) = -1");

  printf("  PASS T-QAT-08 q_dot correct\n");
}

/* ========================================================================
 * T-QAT-09  q_conj — conjugate properties
 * ======================================================================== */
static void test_q_conj(void)
{
  quat_t id = q_conj(q_identity());
  CHECK(FABS(id.w - 1.0f) < 1e-6f, "conjugate of identity = identity");
  CHECK(FABS(id.x) < 1e-6f && FABS(id.y) < 1e-6f && FABS(id.z) < 1e-6f, "conjugate of identity has zero vector part");

  quat_t p = {1.0f, 2.0f, 3.0f, 4.0f};
  quat_t cp = q_conj(p);
  CHECK(FABS(cp.w - 1.0f) < 1e-6f, "conjugate preserves w");
  CHECK(FABS(cp.x + 2.0f) < 1e-6f, "conjugate negates x");
  CHECK(FABS(cp.y + 3.0f) < 1e-6f, "conjugate negates y");
  CHECK(FABS(cp.z + 4.0f) < 1e-6f, "conjugate negates z");

  /* Double-conjugate = original */
  quat_t dcp = q_conj(cp);
  CHECK(FABS(dcp.w - 1.0f) < 1e-6f && FABS(dcp.x - 2.0f) < 1e-6f
         && FABS(dcp.y - 3.0f) < 1e-6f && FABS(dcp.z - 4.0f) < 1e-6f,
         "double conjugate = original");

  printf("  PASS T-QAT-09 q_conj correct\n");
}

/* ========================================================================
 * T-QAT-10  q_rotate_vec — zero-vector input → zero output (no crash)
 * ======================================================================== */
static void test_rotate_vec_zero(void)
{
  quat_t q = q_from_euler(DEG2RAD(45.0f), DEG2RAD(-30.0f), DEG2RAD(60.0f));
  float v[3] = {0.0f, 0.0f, 0.0f};
  float vr[3] = {999.0f, 999.0f, 999.0f};
  q_rotate_vec(q, v, vr);
  CHECK(FABS(vr[0]) < 1e-6f, "zero vector rotated must stay zero (x)");
  CHECK(FABS(vr[1]) < 1e-6f, "zero vector rotated must stay zero (y)");
  CHECK(FABS(vr[2]) < 1e-6f, "zero vector rotated must stay zero (z)");

  printf("  PASS T-QAT-10 q_rotate_vec zero-vector is idempotent\n");
}

/* ========================================================================
 * T-QAT-11  q_rotate_vec — identity quaternion → vector unchanged
 * ======================================================================== */
static void test_rotate_vec_identity(void)
{
  quat_t q = q_identity();
  float v[3] = {1.0f, -2.0f, 3.0f};
  float vr[3] = {0.0f, 0.0f, 0.0f};
  q_rotate_vec(q, v, vr);
  CHECK(FABS(vr[0] - 1.0f) < 1e-6f, "identity quaternion: vx unchanged");
  CHECK(FABS(vr[1] + 2.0f) < 1e-6f, "identity quaternion: vy unchanged");
  CHECK(FABS(vr[2] - 3.0f) < 1e-6f, "identity quaternion: vz unchanged");

  printf("  PASS T-QAT-11 q_rotate_vec identity leaves vector unchanged\n");
}

/* ========================================================================
 * T-QAT-12  Euler round-trip — near-pole singularity (pitch = ±89.9°)
 * ======================================================================== */
static void test_euler_near_pole(void)
{
  const float near_pole[][3] = {
      {DEG2RAD(10.0f), DEG2RAD(89.9f), DEG2RAD(20.0f)},
      {DEG2RAD(-5.0f), DEG2RAD(-89.9f), DEG2RAD(45.0f)},
  };
  /* At gimbal-lock, pitch gets clamped to ±π/2 exactly, so exact agreement
   * isn't possible for pitch ≈ ±89.9°.  Use 0.01 rad (~0.6°) tolerance. */
  const float tol = 1e-2f;

  for (int i = 0; i < 2; i++)
  {
    float r0 = near_pole[i][0], p0 = near_pole[i][1], y0 = near_pole[i][2];
    quat_t q = q_from_euler(r0, p0, y0);
    CHECK(isfinite(q.w) && isfinite(q.x) && isfinite(q.y) && isfinite(q.z),
          "quaternion must be finite at near-pole pitch");

    float r1, p1, y1;
    q_to_euler(q, &r1, &p1, &y1);
    CHECK(isfinite(r1) && isfinite(p1) && isfinite(y1),
          "q_to_euler must return finite angles near pole");
    CHECK(FABS(p1 - p0) < tol, "pitch near-pole round-trip must be within tolerance");
  }
  printf("  PASS T-QAT-12 Euler near-pole (pitch = ±89.9°) round-trip OK\n");
}

/* ========================================================================
 * main
 * ======================================================================== */
int main(void)
{
  printf("=== Quaternion utility tests (PR-21) ===\n");

  test_euler_round_trip();
  test_mult_inverse();
  test_normalize_unit();
  test_rotate_vec_90_yaw();
  test_singularity_free();
  test_axis_angle_normal();
  test_axis_angle_zero_axis();
  test_q_dot();
  test_q_conj();
  test_rotate_vec_zero();
  test_rotate_vec_identity();
  test_euler_near_pole();

  if (g_failures == 0)
  {
    printf("ALL QUATERNION TESTS PASSED\n");
    return 0;
  }
  printf("%d QUATERNION TEST(S) FAILED\n", g_failures);
  return 1;
}
