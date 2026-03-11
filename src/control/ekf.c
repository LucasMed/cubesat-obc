/**
 * @file ekf.c
 * @brief Extended Kalman Filter — attitude estimation with gyro-bias correction.
 *
 * See ekf.h for the state/measurement model description.
 *
 * All matrix operations are implemented as static helpers using fixed-size
 * 6×6 stack arrays.  No dynamic memory allocation is used.
 */

#include "ekf.h"
#include "quaternion.h"

#include <math.h>
#include <string.h>

/* =========================================================================
 * Default noise tuning parameters
 * ========================================================================= */

/** Process noise: attitude integration driven by gyro measurement noise. */
#define EKF_Q_ATT 1e-4f
/** Process noise: gyro bias random walk (very slow drift). */
#define EKF_Q_BIAS 1e-6f
/** Measurement noise: accelerometer-derived tilt angles. */
#define EKF_R_ATT 1e-2f
/** Magnetometer yaw measurement noise [rad²]. */
#define EKF_R_MAG 1e-1f
/** Minimum horizontal field magnitude [µT] for valid yaw update. */
#define EKF_MAG_EPSILON 1.0f
/** Initial attitude covariance (large — cold start). */
#define EKF_P0_ATT 1.0f
/** Initial bias covariance. */
#define EKF_P0_BIAS 1e-2f

/* =========================================================================
 * Internal 6×6 matrix helpers
 * All operate on float[6][6] passed by pointer.
 * ========================================================================= */

/** Zero a 7×7 matrix. */
static void mat77_zero(float A[7][7])
{
  (void)memset(A, 0, sizeof(float) * 49u);
}

/**
 * C = A * B  (7×7 × 7×7)
 */
static void mat77_mul(float A[7][7], float B[7][7], float C[7][7])
{
  float tmp[7][7];
  for (int i = 0; i < 7; i++)
  {
    for (int j = 0; j < 7; j++)
    {
      tmp[i][j] = 0.0f;
      for (int k = 0; k < 7; k++)
      {
        tmp[i][j] += A[i][k] * B[k][j];
      }
    }
  }
  (void)memcpy(C, tmp, sizeof(float) * 49u);
}

/**
 * C = A * B^T  (7×7 × 7×7, B transposed)
 */
static void mat77_mul_T(float A[7][7], float B[7][7], float C[7][7])
{
  float tmp[7][7];
  for (int i = 0; i < 7; i++)
  {
    for (int j = 0; j < 7; j++)
    {
      tmp[i][j] = 0.0f;
      for (int k = 0; k < 7; k++)
      {
        tmp[i][j] += A[i][k] * B[j][k];
      }
    }
  }
  (void)memcpy(C, tmp, sizeof(float) * 49u);
}

/** A += B  (element-wise, in-place). */

/* =========================================================================
 * Public API
 * ========================================================================= */

void ekf_init(ekf_t *ekf)
{
  (void)memset(ekf, 0, sizeof(*ekf));

  /* Initial state: identity quaternion, zero bias */
  ekf->x[0] = 1.0f;

  /* Initial state covariance P0 (7x7) */
  for (int i = 0; i < 4; i++)
  {
    ekf->P[i][i] = EKF_P0_ATT;
  }
  for (int i = 0; i < 3; i++)
  {
    ekf->P[4 + i][4 + i] = EKF_P0_BIAS;
  }

  /* Process noise Q (7x7, diagonal) */
  for (int i = 0; i < 4; i++)
  {
    ekf->Q[i][i] = EKF_Q_ATT;
  }
  for (int i = 0; i < 3; i++)
  {
    ekf->Q[4 + i][4 + i] = EKF_Q_BIAS;
  }

  /* Measurement noise R (3x3, diagonal) */
  for (int i = 0; i < 3; i++)
  {
    ekf->R[i][i] = EKF_R_ATT;
  }

  /* Magnetometer noise */
  ekf->r_mag = EKF_R_MAG;
}

void ekf_predict(ekf_t *ekf, const float gyro[3], float dt)
{
  float q0 = ekf->x[0];
  float q1 = ekf->x[1];
  float q2 = ekf->x[2];
  float q3 = ekf->x[3];
  float bx = ekf->x[4];
  float by = ekf->x[5];
  float bz = ekf->x[6];

  /* Corrected angular rate: omega = gyro - bias */
  float wx = gyro[0] - bx;
  float wy = gyro[1] - by;
  float wz = gyro[2] - bz;

  /* ---- State prediction (Quaternion Kinematics) ----------------------- *
   *  q_dot = 0.5 * q ⊗ [0, wx, wy, wz]                                     */
  float dq0 = 0.5f * (-q1 * wx - q2 * wy - q3 * wz);
  float dq1 = 0.5f * (q0 * wx + q2 * wz - q3 * wy);
  float dq2 = 0.5f * (q0 * wy - q1 * wz + q3 * wx);
  float dq3 = 0.5f * (q0 * wz + q1 * wy - q2 * wx);

  ekf->x[0] += dq0 * dt;
  ekf->x[1] += dq1 * dt;
  ekf->x[2] += dq2 * dt;
  ekf->x[3] += dq3 * dt;

  /* Re-normalise quaternion to maintain unit length constraint */
  float n = sqrtf(ekf->x[0] * ekf->x[0] + ekf->x[1] * ekf->x[1] +
                  ekf->x[2] * ekf->x[2] + ekf->x[3] * ekf->x[3]);
  if (n > 1e-6f)
  {
    ekf->x[0] /= n;
    ekf->x[1] /= n;
    ekf->x[2] /= n;
    ekf->x[3] /= n;
  }

  /* bias states [4..6] unchanged in prediction */

  /* ---- Build state-transition matrix Φ (7x7) ------------------------- *
   *  F = [ 0.5*Omega(w)   -0.5*Xi(q) ]                                    *
   *      [ 0_{3x4}         0_{3x3}   ]                                    *
   *  Φ = I + F*dt                                                         */
  float Phi[7][7];
  mat77_zero(Phi);
  for (int i = 0; i < 7; i++)
  {
    Phi[i][i] = 1.0f;
  }

  /* d(q_dot)/dq = 0.5 * Omega(w) */
  float s = 0.5f * dt;
  Phi[0][1] = -wx * s;
  Phi[0][2] = -wy * s;
  Phi[0][3] = -wz * s;
  Phi[1][0] = wx * s;
  Phi[1][2] = wz * s;
  Phi[1][3] = -wy * s;
  Phi[2][0] = wy * s;
  Phi[2][1] = -wz * s;
  Phi[2][3] = wx * s;
  Phi[3][0] = wz * s;
  Phi[3][1] = wy * s;
  Phi[3][2] = -wx * s;

  /* d(q_dot)/dbias = -0.5 * Xi(q) */
  Phi[0][4] = q1 * s;
  Phi[0][5] = q2 * s;
  Phi[0][6] = q3 * s;
  Phi[1][4] = -q0 * s;
  Phi[1][5] = q3 * s;
  Phi[1][6] = -q2 * s;
  Phi[2][4] = -q3 * s;
  Phi[2][5] = -q0 * s;
  Phi[2][6] = q1 * s;
  Phi[3][4] = q2 * s;
  Phi[3][5] = -q1 * s;
  Phi[3][6] = -q0 * s;

  /* ---- Covariance prediction: P = Φ * P * Φ^T + Q -------------------- */
  float tmp[7][7];
  mat77_mul(Phi, ekf->P, tmp);
  mat77_mul_T(tmp, Phi, ekf->P);
  for (int i = 0; i < 7; i++)
  {
    for (int j = 0; j < 7; j++)
    {
      ekf->P[i][j] += ekf->Q[i][j];
    }
  }
}

void ekf_update(ekf_t *ekf, const float accel[3])
{
  float ax = accel[0];
  float ay = accel[1];
  float az = accel[2];

  /* Normalise accelerometer measurement to unit vector (direction of gravity) */
  float a_norm = sqrtf(ax * ax + ay * ay + az * az);
  if (a_norm < 1e-6f)
  {
    return;
  }
  ax /= a_norm;
  ay /= a_norm;
  az /= a_norm;

  float q0 = ekf->x[0];
  float q1 = ekf->x[1];
  float q2 = ekf->x[2];
  float q3 = ekf->x[3];

  /* ---- Measurement model: h(x) = R(q)*[0,0,1]^T (Unit gravity in body frame) *
   *  Assuming world gravity is [0, 0, 1] for normalization.                     *
   *  h1 = 2*(q1*q3 - q0*q2)                                                     *
   *  h2 = 2*(q2*q3 + q0*q1)                                                     *
   *  h3 = q0*q0 - q1*q1 - q2*q2 + q3*q3                                         */
  float h1 = 2.0f * (q1 * q3 - q0 * q2);
  float h2 = 2.0f * (q2 * q3 + q0 * q1);
  float h3 = q0 * q0 - q1 * q1 - q2 * q2 + q3 * q3;

  /* ---- Innovation: y = z - h(x) --------------------------------------- */
  float y0 = ax - h1;
  float y1 = ay - h2;
  float y2 = az - h3;

  /* ---- Measurement Jacobian H (3x7) ----------------------------------- *
   *  H = [ dh/dq  0_{3x3} ]                                               */
  float H[3][7];
  (void)memset(H, 0, sizeof(H));

  H[0][0] = -2.0f * q2;
  H[0][1] = 2.0f * q3;
  H[0][2] = -2.0f * q0;
  H[0][3] = 2.0f * q1;

  H[1][0] = 2.0f * q1;
  H[1][1] = 2.0f * q0;
  H[1][2] = 2.0f * q3;
  H[1][3] = 2.0f * q2;

  H[2][0] = 2.0f * q0;
  H[2][1] = -2.0f * q1;
  H[2][2] = -2.0f * q2;
  H[2][3] = 2.0f * q3;

  /* ---- Innovation covariance: S = H*P*H^T + R (3x3) ------------------- */
  float S[3][3];
  for (int i = 0; i < 3; i++)
  {
    for (int j = 0; j < 3; j++)
    {
      S[i][j] = 0.0f;
      for (int k = 0; k < 7; k++)
      {
        for (int l = 0; l < 7; l++)
        {
          S[i][j] += H[i][k] * ekf->P[k][l] * H[j][l];
        }
      }
      if (i == j)
      {
        S[i][j] += ekf->R[i][j];
      }
    }
  }

  /* ---- S^{-1} (3x3 manual inverse) ------------------------------------ */
  float det = S[0][0] * (S[1][1] * S[2][2] - S[1][2] * S[2][1]) -
              S[0][1] * (S[1][0] * S[2][2] - S[1][2] * S[2][0]) +
              S[0][2] * (S[1][0] * S[2][1] - S[1][1] * S[2][0]);

  if (fabsf(det) < 1e-12f)
  {
    return;
  }
  float inv_det = 1.0f / det;
  float Si[3][3];
  Si[0][0] = (S[1][1] * S[2][2] - S[1][2] * S[2][1]) * inv_det;
  Si[0][1] = (S[0][2] * S[2][1] - S[0][1] * S[2][2]) * inv_det;
  Si[0][2] = (S[0][1] * S[1][2] - S[0][2] * S[1][1]) * inv_det;
  Si[1][0] = (S[1][2] * S[2][0] - S[1][0] * S[2][2]) * inv_det;
  Si[1][1] = (S[0][0] * S[2][2] - S[0][2] * S[2][0]) * inv_det;
  Si[1][2] = (S[1][0] * S[0][2] - S[0][0] * S[1][2]) * inv_det;
  Si[2][0] = (S[1][0] * S[2][1] - S[1][1] * S[2][0]) * inv_det;
  Si[2][1] = (S[2][0] * S[0][1] - S[0][0] * S[2][1]) * inv_det;
  Si[2][2] = (S[0][0] * S[1][1] - S[1][0] * S[0][1]) * inv_det;

  /* ---- Kalman gain: K = P * H^T * S^{-1} (7x3) ------------------------ */
  float K[7][3];
  for (int i = 0; i < 7; i++)
  {
    for (int j = 0; j < 3; j++)
    {
      K[i][j] = 0.0f;
      for (int k = 0; k < 3; k++)
      {
        float PHt_ik = 0.0f;
        for (int l = 0; l < 7; l++)
        {
          PHt_ik += ekf->P[i][l] * H[k][l];
        }
        K[i][j] += PHt_ik * Si[k][j];
      }
    }
  }

  /* ---- State update: x = x + K * y ------------------------------------ */
  ekf->x[0] += K[0][0] * y0 + K[0][1] * y1 + K[0][2] * y2;
  ekf->x[1] += K[1][0] * y0 + K[1][1] * y1 + K[1][2] * y2;
  ekf->x[2] += K[2][0] * y0 + K[2][1] * y1 + K[2][2] * y2;
  ekf->x[3] += K[3][0] * y0 + K[3][1] * y1 + K[3][2] * y2;
  ekf->x[4] += K[4][0] * y0 + K[4][1] * y1 + K[4][2] * y2;
  ekf->x[5] += K[5][0] * y0 + K[5][1] * y1 + K[5][2] * y2;
  ekf->x[6] += K[6][0] * y0 + K[6][1] * y1 + K[6][2] * y2;

  /* Re-normalise quaternion */
  float n = sqrtf(ekf->x[0] * ekf->x[0] + ekf->x[1] * ekf->x[1] +
                  ekf->x[2] * ekf->x[2] + ekf->x[3] * ekf->x[3]);
  if (n > 1e-6f)
  {
    ekf->x[0] /= n;
    ekf->x[1] /= n;
    ekf->x[2] /= n;
    ekf->x[3] /= n;
  }

  /* ---- Covariance update: P = (I - K*H) * P --------------------------- */
  float P_new[7][7];
  for (int i = 0; i < 7; i++)
  {
    for (int j = 0; j < 7; j++)
    {
      float KH_ij = 0.0f;
      for (int k = 0; k < 3; k++)
      {
        KH_ij += K[i][k] * H[k][j];
      }
      P_new[i][j] = ekf->P[i][j];
      for (int k = 0; k < 7; k++)
      {
        /* This is actually P_new = P - K*H*P, which is more stable in this form:
         * P_new[i][j] = P[i][j] - sum_k( (sum_l K[i][l]*H[l][k]) * P[k][j] )
         */
      }
      /* Simple (I-KH)P implementation: */
    }
  }

  /* Re-implementing P = (I-KH)P safely: */
  for (int i = 0; i < 7; i++)
  {
    for (int j = 0; j < 7; j++)
    {
      float KH_row_i_col_j = 0.0f;
      for (int k = 0; k < 3; k++)
      {
        for (int l = 0; l < 7; l++)
        {
          KH_row_i_col_j += K[i][k] * H[k][l] * ekf->P[l][j];
        }
      }
      P_new[i][j] = ekf->P[i][j] - KH_row_i_col_j;
    }
  }
  (void)memcpy(ekf->P, P_new, sizeof(P_new));
}

void ekf_get_quaternion(const ekf_t *ekf, float q[4])
{
  q[0] = ekf->x[0];
  q[1] = ekf->x[1];
  q[2] = ekf->x[2];
  q[3] = ekf->x[3];
}

void ekf_get_attitude(const ekf_t *ekf, float att[3])
{
  /* Convert internal quaternion to Euler angles for legacy API callers */
  quat_t q = {ekf->x[0], ekf->x[1], ekf->x[2], ekf->x[3]};
  q_to_euler(q, &att[0], &att[1], &att[2]);
}

void ekf_get_bias(const ekf_t *ekf, float bias[3])
{
  bias[0] = ekf->x[4];
  bias[1] = ekf->x[5];
  bias[2] = ekf->x[6];
}

void ekf_update_mag(ekf_t *ekf, const float mag_field_uT[3], float declination_rad)
{
  float Bx = mag_field_uT[0];
  float By = mag_field_uT[1];
  float Bz = mag_field_uT[2];

  /* Magnetometer reference in world frame. */
  float B_mag = sqrtf(Bx * Bx + By * By + Bz * Bz);
  float Bh_mag = sqrtf(Bx * Bx + By * By); /* body horizontal magnitude */
  if (B_mag < EKF_MAG_EPSILON || Bh_mag < EKF_MAG_EPSILON)
  {
    return;
  }

  float Bw[3];
  Bw[0] = cosf(declination_rad);
  Bw[1] = sinf(declination_rad);
  Bw[2] = 0.0f; /* Simplified horizontal reference */

  /* Normalise measurements for unit vector update */
  float b0 = Bx / B_mag;
  float b1 = By / B_mag;
  float b2 = Bz / B_mag;

  float q0 = ekf->x[0];
  float q1 = ekf->x[1];
  float q2 = ekf->x[2];
  float q3 = ekf->x[3];

  /* ---- Measurement model: h(x) = R(q) * Bw ---------------------------- *
   *  Using R(q) rows because z is in body frame.                          */
  float r11 = q0 * q0 + q1 * q1 - q2 * q2 - q3 * q3;
  float r12 = 2.0f * (q1 * q2 + q0 * q3);
  float r13 = 2.0f * (q1 * q3 - q0 * q2);
  float r21 = 2.0f * (q1 * q2 - q0 * q3);
  float r22 = q0 * q0 - q1 * q1 + q2 * q2 - q3 * q3;
  float r23 = 2.0f * (q2 * q3 + q0 * q1);
  float r31 = 2.0f * (q1 * q3 + q0 * q2);
  float r32 = 2.0f * (q2 * q3 - q0 * q1);
  float r33 = q0 * q0 - q1 * q1 - q2 * q2 + q3 * q3;

  float h1 = r11 * Bw[0] + r12 * Bw[1] + r13 * Bw[2];
  float h2 = r21 * Bw[0] + r22 * Bw[1] + r23 * Bw[2];
  float h3 = r31 * Bw[0] + r32 * Bw[1] + r33 * Bw[2];

  /* ---- Innovation ----------------------------------------------------- */
  float y0 = b0 - h1;
  float y1 = b1 - h2;
  float y2 = b2 - h3;

  /* ---- Jacobian H (3x7) ----------------------------------------------- *
   *  H = [ dh/dq  0_{3x3} ]                                               */
  float H[3][7];
  (void)memset(H, 0, sizeof(H));

  /* dh/dq for h = R(q)*Bw */
  /* This is a complex derivation, using simplified partials for Bw=[Bxw, Byw, 0]: */
  float Bxw = Bw[0];
  float Byw = Bw[1];

  H[0][0] = 2.0f * (q0 * Bxw + q3 * Byw);
  H[0][1] = 2.0f * (q1 * Bxw + q2 * Byw);
  H[0][2] = 2.0f * (-q2 * Bxw + q1 * Byw);
  H[0][3] = 2.0f * (-q3 * Bxw + q0 * Byw);

  H[1][0] = 2.0f * (-q3 * Bxw + q0 * Byw);
  H[1][1] = 2.0f * (q2 * Bxw - q1 * Byw);
  H[1][2] = 2.0f * (q1 * Bxw + q2 * Byw);
  H[1][3] = 2.0f * (-q0 * Bxw - q3 * Byw);

  H[2][0] = 2.0f * (q2 * Bxw - q1 * Byw);
  H[2][1] = 2.0f * (q3 * Bxw + q0 * Byw);
  H[2][2] = 2.0f * (q0 * Bxw - q3 * Byw);
  H[2][3] = 2.0f * (q1 * Bxw + q2 * Byw);

  /* ---- EKF Update steps (Reuse the logic from ekf_update) ------------ */
  /* (For efficiency in a real project this would be a shared helper function) */
  
  /* Innovation covariance S = HPH' + R */
  float S[3][3];
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      S[i][j] = 0.0f;
      for (int k = 0; k < 7; k++) {
        for (int l = 0; l < 7; l++) {
          S[i][j] += H[i][k] * ekf->P[k][l] * H[j][l];
        }
      }
      if (i == j) S[i][j] += ekf->r_mag; /* use scalar r_mag for all components */
    }
  }

  /* Inversion and Gain calculation omitted for brevity in this block, 
   * but follows same 3x3 pattern as accel update. */
  /* Actually I MUST implement it to have valid code. */
  float det = S[0][0]*(S[1][1]*S[2][2] - S[1][2]*S[2][1]) -
              S[0][1]*(S[1][0]*S[2][2] - S[1][2]*S[2][0]) +
              S[0][2]*(S[1][0]*S[2][1] - S[1][1]*S[2][0]);
  if (fabsf(det) < 1e-12f) return;
  float inv_det = 1.0f / det;
  float Si[3][3];
  Si[0][0] = (S[1][1]*S[2][2] - S[1][2]*S[2][1]) * inv_det;
  Si[0][1] = (S[0][2]*S[2][1] - S[0][1]*S[2][2]) * inv_det;
  Si[0][2] = (S[0][1]*S[1][2] - S[0][2]*S[1][1]) * inv_det;
  Si[1][0] = (S[1][2]*S[2][0] - S[1][0]*S[2][2]) * inv_det;
  Si[1][1] = (S[0][0]*S[2][2] - S[0][2]*S[2][0]) * inv_det;
  Si[1][2] = (S[1][0]*S[0][2] - S[0][0]*S[1][2]) * inv_det;
  Si[2][0] = (S[1][0]*S[2][1] - S[1][1]*S[2][0]) * inv_det;
  Si[2][1] = (S[2][0]*S[0][1] - S[0][0]*S[2][1]) * inv_det;
  Si[2][2] = (S[0][0]*S[1][1] - S[1][0]*S[0][1]) * inv_det;

  float K[7][3];
  for (int i = 0; i < 7; i++) {
    for (int j = 0; j < 3; j++) {
      K[i][j] = 0.0f;
      for (int k = 0; k < 3; k++) {
        float PHt_ik = 0.0f;
        for (int l = 0; l < 7; l++) PHt_ik += ekf->P[i][l] * H[k][l];
        K[i][j] += PHt_ik * Si[k][j];
      }
    }
  }

  /* Update state */
  float dy[3] = {y0, y1, y2};
  for (int i = 0; i < 7; i++) {
    for (int j = 0; j < 3; j++) ekf->x[i] += K[i][j] * dy[j];
  }

  /* Re-normalise */
  float n = sqrtf(ekf->x[0]*ekf->x[0] + ekf->x[1]*ekf->x[1] + ekf->x[2]*ekf->x[2] + ekf->x[3]*ekf->x[3]);
  if (n > 1e-6f) { ekf->x[0]/=n; ekf->x[1]/=n; ekf->x[2]/=n; ekf->x[3]/=n; }

  /* Update covariance P = (I-KH)P */
  float P_new[7][7];
  for (int i = 0; i < 7; i++) {
    for (int j = 0; j < 7; j++) {
      float KH_row_i_col_j = 0.0f;
      for (int k = 0; k < 3; k++) {
        for (int l = 0; l < 7; l++) KH_row_i_col_j += K[i][k] * H[k][l] * ekf->P[l][j];
      }
      P_new[i][j] = ekf->P[i][j] - KH_row_i_col_j;
    }
  }
  (void)memcpy(ekf->P, P_new, sizeof(P_new));
}
