/**
 * @file test_ekf.c
 * @brief Unit tests for the EKF attitude estimator (PR-12).
 *
 * Tests
 *  T-EKF-01  Initialisation — state zero, covariance diagonal positive
 *  T-EKF-02  Predict only    — state propagates from gyro; bias unchanged
 *  T-EKF-03  Convergence     — roll/pitch converge to within ±2° in 5 s
 *  T-EKF-04  Bias estimation — roll/pitch gyro bias converges in 10 s
 *  T-EKF-05  Degenerate accel — update skipped when |accel| ≈ 0
 *  T-EKF-06  Covariance symmetry — P stays symmetric after 50 steps
 *  T-EKF-07  mat33_inverse identity — I⁻¹ = I
 *  T-EKF-08  mat33_inverse diagonal — diag(a,b,c)⁻¹ = diag(1/a, 1/b, 1/c)
 *  T-EKF-09  mat33_inverse singular — zero matrix returns false
 *  T-EKF-10  mat33_inverse round-trip — S × S⁻¹ ≈ I for known matrix
 */

#include "../../include/ekf.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* Forward declaration of internal EKF helper (not in public header). */
bool mat33_inverse(const float S[3][3], float Si[3][3]);

#define DEG2RAD(d) ((d) * (3.14159265358979f / 180.0f))
#define RAD2DEG(r) ((r) * (180.0f / 3.14159265358979f))
#define G_MS2 9.80665f

#define PASS(msg) printf("[PASS] %s\n", msg)
#define FAIL(msg)                                                                                  \
  do                                                                                               \
  {                                                                                                \
    printf("[FAIL] %s\n", msg);                                                                    \
    return 1;                                                                                      \
  } while (0)

/* ------------------------------------------------------------------ */
/* Helper: gravity vector in body frame given true roll/pitch          */
/* accel = R^T * [0,0,g]  for small-angle Rx*Ry rotation              */
/* ------------------------------------------------------------------ */
static void true_accel(float roll_rad, float pitch_rad, float accel[3])
{
  accel[0] = -sinf(pitch_rad) * G_MS2;
  accel[1] = sinf(roll_rad) * cosf(pitch_rad) * G_MS2;
  accel[2] = cosf(roll_rad) * cosf(pitch_rad) * G_MS2;
}

/* ================================================================== */
/* T-EKF-01: Initialisation                                           */
/* ================================================================== */
static int test_init(void)
{
  ekf_t ekf;
  ekf_init(&ekf);

  /* Initial state: [1, 0, 0, 0, 0, 0, 0] (identity quat) */
  if (fabsf(ekf.x[0] - 1.0f) > 1e-9f)
  {
    FAIL("T-EKF-01: initial q0 not 1.0");
  }
  for (int i = 1; i < EKF_N; i++)
  {
    if (fabsf(ekf.x[i]) > 1e-9f)
    {
      FAIL("T-EKF-01: internal state elements not zero");
    }
  }

  /* Diagonal of P must be positive */
  for (int i = 0; i < EKF_N; i++)
  {
    if (ekf.P[i][i] <= 0.0f)
    {
      FAIL("T-EKF-01: diagonal of P not positive");
    }
  }

  /* Q diagonals must be positive (7 elements) */
  for (int i = 0; i < EKF_N; i++)
  {
    if (ekf.Q[i][i] <= 0.0f)
    {
      FAIL("T-EKF-01: diagonal of Q not positive");
    }
  }
  for (int i = 0; i < EKF_M; i++)
  {
    if (ekf.R[i][i] <= 0.0f)
    {
      FAIL("T-EKF-01: diagonal of R not positive");
    }
  }

  PASS("T-EKF-01: initialisation — state zero, covariance positive");
  return 0;
}

/* ================================================================== */
/* T-EKF-02: Predict only — state propagates from gyro                */
/* ================================================================== */
static int test_predict_propagation(void)
{
  ekf_t ekf;
  ekf_init(&ekf);

  /* Inject known gyro rate on roll axis, no bias */
  float gyro[3] = {0.1f, 0.0f, 0.0f};
  float dt = 0.1f;

  ekf_predict(&ekf, gyro, dt);

  /* roll should advance. q0 = cos(theta/2), q1 = sin(theta/2) */
  /* For small theta=0.01: q0 ~ 0.99998, q1 ~ 0.005 */
  float theta = gyro[0] * dt;
  float expected_q0 = cosf(theta / 2.0f);
  float expected_q1 = sinf(theta / 2.0f);
  if (fabsf(ekf.x[0] - expected_q0) > 1e-4f || fabsf(ekf.x[1] - expected_q1) > 1e-4f)
  {
    printf("  q0 = %f  expected = %f\n", ekf.x[0], expected_q0);
    printf("  q1 = %f  expected = %f\n", ekf.x[1], expected_q1);
    FAIL("T-EKF-02: quaternion state not propagated correctly");
  }

  /* q2 and q3 must stay zero */
  if (fabsf(ekf.x[2]) > 1e-9f || fabsf(ekf.x[3]) > 1e-9f)
  {
    FAIL("T-EKF-02: spurious q2/q3 from roll-only gyro");
  }

  /* Bias must stay zero [4..6] */
  for (int i = 4; i < EKF_N; i++)
  {
    if (fabsf(ekf.x[i]) > 1e-9f)
    {
      FAIL("T-EKF-02: bias state changed during predict");
    }
  }

  /* Covariance must grow (P[0][0] > initial) */
  if (ekf.P[0][0] <= 1.0f)
  {
    FAIL("T-EKF-02: covariance did not grow after predict");
  }

  PASS("T-EKF-02: predict — state and covariance propagate correctly");
  return 0;
}

/* ================================================================== */
/* T-EKF-03: Convergence — cold start to within ±2° in 5 s           */
/*                                                                    */
/* Scenario: spacecraft at roll=15°, pitch=10°, stationary.          */
/* EKF starts at zero. Gyro = 0. Accel = gravity in body frame.      */
/* ================================================================== */
static int test_convergence(void)
{
  const float TRUE_ROLL = DEG2RAD(15.0f);
  const float TRUE_PITCH = DEG2RAD(10.0f);
  const float dt = 0.1f; /* 10 Hz */
  const int steps = 50;  /* 5 s   */

  ekf_t ekf;
  ekf_init(&ekf);

  float gyro[3] = {0.0f, 0.0f, 0.0f};
  float accel[3];
  true_accel(TRUE_ROLL, TRUE_PITCH, accel);

  for (int i = 0; i < steps; i++)
  {
    ekf_predict(&ekf, gyro, dt);
    ekf_update(&ekf, accel);
  }

  float att[3];
  ekf_get_attitude(&ekf, att);

  float err_roll = fabsf(att[0] - TRUE_ROLL);
  float err_pitch = fabsf(att[1] - TRUE_PITCH);

  if (err_roll > DEG2RAD(2.0f))
  {
    printf("  roll error = %.2f° (limit 2°)\n", RAD2DEG(err_roll));
    FAIL("T-EKF-03: roll did not converge within ±2° in 5 s");
  }
  if (err_pitch > DEG2RAD(2.0f))
  {
    printf("  pitch error = %.2f° (limit 2°)\n", RAD2DEG(err_pitch));
    FAIL("T-EKF-03: pitch did not converge within ±2° in 5 s");
  }

  PASS("T-EKF-03: convergence — roll/pitch within ±2° after 5 s");
  return 0;
}

/* ================================================================== */
/* T-EKF-04: Bias estimation — converges to injected bias in 10 s     */
/*                                                                    */
/* Scenario: spacecraft level (roll=pitch=0), stationary.             */
/* Gyro measures constant bias [0.01, 0.008, 0] rad/s.               */
/* Accel = [0, 0, g].                                                 */
/* After 10 s, estimated bias[0] and bias[1] within ±0.5°/s.         */
/* (yaw bias not observable from accel alone — not tested)            */
/* ================================================================== */
static int test_bias_estimation(void)
{
  const float TRUE_BIAS_X = 0.01f;        /* rad/s */
  const float TRUE_BIAS_Y = 0.008f;       /* rad/s */
  const float dt = 0.1f;                  /* 10 Hz */
  const int steps = 100;                  /* 10 s  */
  const float BIAS_LIMIT = DEG2RAD(0.5f); /* 0.00873 rad/s */

  /* Level spacecraft: accel = [0, 0, g] */
  float accel[3];
  true_accel(0.0f, 0.0f, accel);

  /* Gyro sees only bias (no real rotation) */
  float gyro[3] = {TRUE_BIAS_X, TRUE_BIAS_Y, 0.0f};

  ekf_t ekf;
  ekf_init(&ekf);

  for (int i = 0; i < steps; i++)
  {
    ekf_predict(&ekf, gyro, dt);
    ekf_update(&ekf, accel);
  }

  float bias[3];
  ekf_get_bias(&ekf, bias);

  float err_bx = fabsf(bias[0] - TRUE_BIAS_X);
  float err_by = fabsf(bias[1] - TRUE_BIAS_Y);

  if (err_bx > BIAS_LIMIT)
  {
    printf("  bias_x error = %.4f rad/s  (%.2f°/s, limit 0.5°/s)\n", err_bx, RAD2DEG(err_bx));
    FAIL("T-EKF-04: bias_x did not converge within ±0.5°/s in 10 s");
  }
  if (err_by > BIAS_LIMIT)
  {
    printf("  bias_y error = %.4f rad/s  (%.2f°/s, limit 0.5°/s)\n", err_by, RAD2DEG(err_by));
    FAIL("T-EKF-04: bias_y did not converge within ±0.5°/s in 10 s");
  }

  PASS("T-EKF-04: bias estimation — roll/pitch bias within ±0.5°/s after 10 s");
  return 0;
}

/* ================================================================== */
/* T-EKF-05: Degenerate accelerometer — update must be skipped        */
/* ================================================================== */
static int test_degenerate_accel(void)
{
  ekf_t ekf;
  ekf_init(&ekf);

  /* Give non-zero initial state (near identity) */
  ekf.x[0] = 0.99f;
  ekf.x[1] = 0.1f;

  float P_before[EKF_N][EKF_N];
  memcpy(P_before, ekf.P, sizeof(P_before));

  /* Degenerate: |accel| ≈ 0 */
  float accel[3] = {0.0f, 0.0f, 0.0f};
  ekf_update(&ekf, accel);

  /* State and P must be unchanged */
  if (fabsf(ekf.x[0] - 0.99f) > 1e-9f || fabsf(ekf.x[1] - 0.1f) > 1e-9f)
  {
    FAIL("T-EKF-05: state modified by degenerate accel update");
  }
  for (int i = 0; i < EKF_N; i++)
  {
    for (int j = 0; j < EKF_N; j++)
    {
      if (fabsf(ekf.P[i][j] - P_before[i][j]) > 1e-12f)
      {
        FAIL("T-EKF-05: covariance modified by degenerate accel update");
      }
    }
  }

  PASS("T-EKF-05: degenerate accel update correctly skipped");
  return 0;
}

/* ================================================================== */
/* T-EKF-06: Covariance symmetry — P[i][j] == P[j][i] after 50 steps */
/* ================================================================== */
static int test_covariance_symmetry(void)
{
  ekf_t ekf;
  ekf_init(&ekf);

  float gyro[3] = {0.02f, -0.01f, 0.005f};
  float accel[3];
  true_accel(DEG2RAD(5.0f), DEG2RAD(3.0f), accel);

  for (int k = 0; k < 50; k++)
  {
    ekf_predict(&ekf, gyro, 0.1f);
    ekf_update(&ekf, accel);
  }

  for (int i = 0; i < EKF_N; i++)
  {
    for (int j = i + 1; j < EKF_N; j++)
    {
      float asymm = fabsf(ekf.P[i][j] - ekf.P[j][i]);
      if (asymm > 1e-6f)
      {
        printf("  P[%d][%d]=%.2e  P[%d][%d]=%.2e  diff=%.2e\n", i, j, ekf.P[i][j], j, i,
               ekf.P[j][i], asymm);
        FAIL("T-EKF-06: covariance matrix not symmetric after 50 steps");
      }
    }
  }

  PASS("T-EKF-06: covariance remains symmetric after 50 steps");
  return 0;
}

/* ================================================================== */
/* Helper: check that two 3×3 matrices are approximately equal         */
/* ================================================================== */
static int mat33_approx_eq(const float A[3][3], const float B[3][3], float tol)
{
  for (int i = 0; i < 3; i++)
  {
    for (int j = 0; j < 3; j++)
    {
      if (fabsf(A[i][j] - B[i][j]) > tol)
      {
        return 0;
      }
    }
  }
  return 1;
}

/* ================================================================== */
/* Helper: multiply two 3×3 matrices: C = A * B                         */
/* ================================================================== */
static void mat33_mul(const float A[3][3], const float B[3][3], float C[3][3])
{
  for (int i = 0; i < 3; i++)
  {
    for (int j = 0; j < 3; j++)
    {
      C[i][j] = 0.0f;
      for (int k = 0; k < 3; k++)
      {
        C[i][j] += A[i][k] * B[k][j];
      }
    }
  }
}

/* ================================================================== */
/* T-EKF-07: Identity matrix inverse                                   */
/* ================================================================== */
static int test_mat33_inverse_identity(void)
{
  const float I[3][3] = {{1.0f, 0.0f, 0.0f},
                         {0.0f, 1.0f, 0.0f},
                         {0.0f, 0.0f, 1.0f}};
  float Si[3][3];
  float expected[3][3];
  memcpy(expected, I, sizeof(expected));

  if (!mat33_inverse(I, Si))
  {
    FAIL("T-EKF-07: mat33_inverse(I) returned false");
  }
  if (!mat33_approx_eq(Si, expected, 1e-6f))
  {
    printf("  I⁻¹ deviates from identity\n");
    FAIL("T-EKF-07: identity inverse not identity");
  }

  PASS("T-EKF-07: mat33_inverse — identity");
  return 0;
}

/* ================================================================== */
/* T-EKF-08: Diagonal matrix inverse                                   */
/* ================================================================== */
static int test_mat33_inverse_diagonal(void)
{
  const float D[3][3] = {{2.0f, 0.0f, 0.0f},
                         {0.0f, 4.0f, 0.0f},
                         {0.0f, 0.0f, 5.0f}};
  const float expected[3][3] = {{0.5f, 0.0f, 0.0f},
                                {0.0f, 0.25f, 0.0f},
                                {0.0f, 0.0f, 0.2f}};
  float Si[3][3];

  if (!mat33_inverse(D, Si))
  {
    FAIL("T-EKF-08: mat33_inverse(diag) returned false");
  }
  if (!mat33_approx_eq(Si, expected, 1e-6f))
  {
    printf("  diag(2,4,5)⁻¹:\n");
    printf("    [0][0]=%f (expected 0.5)\n", (double)Si[0][0]);
    printf("    [1][1]=%f (expected 0.25)\n", (double)Si[1][1]);
    printf("    [2][2]=%f (expected 0.2)\n", (double)Si[2][2]);
    FAIL("T-EKF-08: diagonal inverse incorrect");
  }

  PASS("T-EKF-08: mat33_inverse — diagonal");
  return 0;
}

/* ================================================================== */
/* T-EKF-09: Singular matrix returns false                             */
/* ================================================================== */
static int test_mat33_inverse_singular(void)
{
  const float zero[3][3] = {{0.0f, 0.0f, 0.0f},
                            {0.0f, 0.0f, 0.0f},
                            {0.0f, 0.0f, 0.0f}};
  float Si[3][3];

  if (mat33_inverse(zero, Si))
  {
    FAIL("T-EKF-09: mat33_inverse(zero) should return false");
  }

  PASS("T-EKF-09: mat33_inverse — singular detected");
  return 0;
}

/* ================================================================== */
/* T-EKF-10: Known matrix round-trip — S × S⁻¹ ≈ I                    */
/* ================================================================== */
static int test_mat33_inverse_roundtrip(void)
{
  /* A non-singular, non-diagonal 3×3 matrix */
  const float S[3][3] = {{3.0f, 1.0f, 2.0f},
                         {1.0f, 4.0f, 0.0f},
                         {2.0f, 0.0f, 5.0f}};
  float Si[3][3], prod[3][3];
  const float I[3][3] = {{1.0f, 0.0f, 0.0f},
                         {0.0f, 1.0f, 0.0f},
                         {0.0f, 0.0f, 1.0f}};

  if (!mat33_inverse(S, Si))
  {
    FAIL("T-EKF-10: mat33_inverse(known) returned false");
  }

  mat33_mul(S, Si, prod);

  if (!mat33_approx_eq(prod, I, 1e-5f))
  {
    printf("  S × S⁻¹:\n");
    for (int i = 0; i < 3; i++)
    {
      printf("    [%d]  %8.5f  %8.5f  %8.5f\n", i,
             (double)prod[i][0], (double)prod[i][1], (double)prod[i][2]);
    }
    FAIL("T-EKF-10: S × S⁻¹ not close to identity");
  }

  PASS("T-EKF-10: mat33_inverse — round-trip S × S⁻¹ ≈ I");
  return 0;
}

/* ================================================================== */
int main(void)
{
  int result = 0;
  result |= test_init();
  result |= test_predict_propagation();
  result |= test_convergence();
  result |= test_bias_estimation();
  result |= test_degenerate_accel();
  result |= test_covariance_symmetry();
  result |= test_mat33_inverse_identity();
  result |= test_mat33_inverse_diagonal();
  result |= test_mat33_inverse_singular();
  result |= test_mat33_inverse_roundtrip();

  if (result == 0)
  {
    printf("All EKF tests passed.\n");
  }
  return result;
}
