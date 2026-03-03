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

/** Zero a 6×6 matrix. */
static void mat66_zero(float A[6][6])
{
  (void)memset(A, 0, sizeof(float) * 36u);
}

/**
 * C = A * B  (6×6 × 6×6)
 * Uses a local temporary so A or B may alias C.
 */
static void mat66_mul(float A[6][6], float B[6][6], float C[6][6])
{
  float tmp[6][6];
  for (int i = 0; i < 6; i++)
  {
    for (int j = 0; j < 6; j++)
    {
      tmp[i][j] = 0.0f;
      for (int k = 0; k < 6; k++)
      {
        tmp[i][j] += A[i][k] * B[k][j];
      }
    }
  }
  (void)memcpy(C, tmp, sizeof(float) * 36u);
}

/**
 * C = A * B^T  (6×6 × 6×6, B transposed)
 * Uses a local temporary so A may alias C.
 */
static void mat66_mul_T(float A[6][6], float B[6][6], float C[6][6])
{
  float tmp[6][6];
  for (int i = 0; i < 6; i++)
  {
    for (int j = 0; j < 6; j++)
    {
      tmp[i][j] = 0.0f;
      for (int k = 0; k < 6; k++)
      {
        tmp[i][j] += A[i][k] * B[j][k];
      }
    }
  }
  (void)memcpy(C, tmp, sizeof(float) * 36u);
}

/** A += B  (element-wise, in-place). */
static void mat66_add(float A[6][6], float B[6][6])
{
  for (int i = 0; i < 6; i++)
  {
    for (int j = 0; j < 6; j++)
    {
      A[i][j] += B[i][j];
    }
  }
}

/* =========================================================================
 * Public API
 * ========================================================================= */

void ekf_init(ekf_t *ekf)
{
  (void)memset(ekf, 0, sizeof(*ekf));

  /* Initial state covariance P0 */
  for (int i = 0; i < 3; i++)
  {
    ekf->P[i][i] = EKF_P0_ATT;
    ekf->P[3 + i][3 + i] = EKF_P0_BIAS;
  }

  /* Process noise Q (diagonal) */
  for (int i = 0; i < 3; i++)
  {
    ekf->Q[i][i] = EKF_Q_ATT;
    ekf->Q[3 + i][3 + i] = EKF_Q_BIAS;
  }

  /* Measurement noise R (diagonal) */
  ekf->R[0][0] = EKF_R_ATT;
  ekf->R[1][1] = EKF_R_ATT;

  /* Magnetometer yaw noise */
  ekf->r_mag = EKF_R_MAG;
}

void ekf_predict(ekf_t *ekf, const float gyro[3], float dt)
{
  /* ---- State prediction ----------------------------------------------- *
   * attitude_dot[i] = gyro[i] - bias[i]                                   *
   * bias_dot[i]     = 0                                                    */
  for (int i = 0; i < 3; i++)
  {
    ekf->x[i] += dt * (gyro[i] - ekf->x[3 + i]);
  }
  /* bias states unchanged */

  /* ---- Build discrete state-transition matrix Φ ----------------------- *
   *                                                                        *
   *  Continuous F = [ 0_{3x3}   -I3  ]                                    *
   *                 [ 0_{3x3}    0   ]                                     *
   *                                                                        *
   *  Φ = I + F*dt = [ I3    -dt*I3 ]                                      *
   *                 [ 0_{3x3}   I3 ]                                       */
  float Phi[6][6];
  mat66_zero(Phi);
  for (int i = 0; i < 6; i++)
  {
    Phi[i][i] = 1.0f;
  }
  for (int i = 0; i < 3; i++)
  {
    Phi[i][3 + i] = -dt;
  }

  /* ---- Covariance prediction: P = Φ * P * Φ^T + Q -------------------- */
  float tmp[6][6];
  mat66_mul(Phi, ekf->P, tmp);
  mat66_mul_T(tmp, Phi, ekf->P);
  mat66_add(ekf->P, ekf->Q);
}

void ekf_update(ekf_t *ekf, const float accel[3])
{
  float ax = accel[0];
  float ay = accel[1];
  float az = accel[2];

  /* Guard against degenerate accelerometer (near-zero magnitude) */
  float ay2_az2 = ay * ay + az * az;
  if (ay2_az2 < 1.0e-10f)
  {
    return;
  }

  /* ---- Measurement: derive roll and pitch from accelerometer ---------- *
   *  roll  = atan2( ay,  az )                                             *
   *  pitch = atan2(-ax,  sqrt(ay² + az²) )                               */
  float z_roll = atan2f(ay, az);
  float z_pitch = atan2f(-ax, sqrtf(ay2_az2));

  /* ---- Innovation: y = z − H*x  (H selects rows 0 and 1) ------------ */
  float y0 = z_roll - ekf->x[0];
  float y1 = z_pitch - ekf->x[1];

  /* ---- Innovation covariance: S = H*P*H^T + R  ----------------------- *
   *  H = [1 0 0 0 0 0]   →  H*P*H^T = P[0:2, 0:2]                       *
   *      [0 1 0 0 0 0]                                                    */
  float S00 = ekf->P[0][0] + ekf->R[0][0];
  float S01 = ekf->P[0][1];
  float S10 = ekf->P[1][0];
  float S11 = ekf->P[1][1] + ekf->R[1][1];

  /* ---- S^{-1} (2×2 analytic inverse) --------------------------------- */
  float det = S00 * S11 - S01 * S10;
  if (fabsf(det) < 1.0e-12f)
  {
    return; /* numerically singular — skip update */
  }
  float inv_det = 1.0f / det;
  float Si00 = S11 * inv_det;
  float Si01 = -S01 * inv_det;
  float Si10 = -S10 * inv_det;
  float Si11 = S00 * inv_det;

  /* ---- Kalman gain: K = P * H^T * S^{-1}  (6×2) -------------------- *
   *  P*H^T = first 2 columns of P  (P[:,0] and P[:,1])                   *
   *  K[i][0] = P[i][0]*Si00 + P[i][1]*Si10                              *
   *  K[i][1] = P[i][0]*Si01 + P[i][1]*Si11                              */
  float K[6][2];
  for (int i = 0; i < 6; i++)
  {
    K[i][0] = ekf->P[i][0] * Si00 + ekf->P[i][1] * Si10;
    K[i][1] = ekf->P[i][0] * Si01 + ekf->P[i][1] * Si11;
  }

  /* ---- State update: x = x + K * y ----------------------------------- */
  for (int i = 0; i < 6; i++)
  {
    ekf->x[i] += K[i][0] * y0 + K[i][1] * y1;
  }

  /* ---- Covariance update: P = (I − K*H) * P ------------------------- *
   *  K*H has non-zero values only in the first 2 columns:                *
   *    (K*H)[i][0] = K[i][0],  (K*H)[i][1] = K[i][1]                   *
   *                                                                       *
   *  P_new[i][j] = P[i][j] − K[i][0]*P[0][j] − K[i][1]*P[1][j]         */
  float P_new[6][6];
  for (int i = 0; i < 6; i++)
  {
    for (int j = 0; j < 6; j++)
    {
      P_new[i][j] = ekf->P[i][j] - K[i][0] * ekf->P[0][j] - K[i][1] * ekf->P[1][j];
    }
  }
  (void)memcpy(ekf->P, P_new, sizeof(P_new));
}

void ekf_get_attitude(const ekf_t *ekf, float att[3])
{
  att[0] = ekf->x[0];
  att[1] = ekf->x[1];
  att[2] = ekf->x[2];
}

void ekf_get_bias(const ekf_t *ekf, float bias[3])
{
  bias[0] = ekf->x[3];
  bias[1] = ekf->x[4];
  bias[2] = ekf->x[5];
}

void ekf_update_mag(ekf_t *ekf, const float mag_field_uT[3], float declination_rad)
{
  float r = ekf->x[0]; /* current roll  estimate */
  float p = ekf->x[1]; /* current pitch estimate */

  /* ---- Tilt-compensated magnetic field (horizontal plane) ------------- *
   *  Bh_x = Bx*cos(p) + By*sin(r)*sin(p) + Bz*cos(r)*sin(p)             *
   *  Bh_y = By*cos(r) - Bz*sin(r)                                        */
  float cr = cosf(r);
  float sr = sinf(r);
  float cp = cosf(p);
  float sp = sinf(p);

  float Bh_x = mag_field_uT[0] * cp + mag_field_uT[1] * sr * sp + mag_field_uT[2] * cr * sp;
  float Bh_y = mag_field_uT[1] * cr - mag_field_uT[2] * sr;

  /* Guard against degenerate horizontal field magnitude */
  float Bh_mag = sqrtf(Bh_x * Bh_x + Bh_y * Bh_y);
  if (Bh_mag < EKF_MAG_EPSILON)
  {
    return;
  }

  /* ---- Measured yaw --------------------------------------------------- */
  float yaw_meas = atan2f(-Bh_y, Bh_x) + declination_rad;

  /* ---- Innovation: wrap to [-π, +π] -----------------------------------  */
  float y = yaw_meas - ekf->x[2];
  while (y > (float)M_PI)
  {
    y -= 2.0f * (float)M_PI;
  }
  while (y < -(float)M_PI)
  {
    y += 2.0f * (float)M_PI;
  }

  /* ---- Innovation covariance: S = P[2][2] + r_mag (scalar) ----------- */
  float S = ekf->P[2][2] + ekf->r_mag;
  if (fabsf(S) < 1.0e-12f)
  {
    return;
  }

  /* ---- Kalman gain: K = P[:,2] / S  (6×1 vector) --------------------- */
  float K[6];
  for (int i = 0; i < 6; i++)
  {
    K[i] = ekf->P[i][2] / S;
  }

  /* ---- State update: x = x + K * y ------------------------------------ */
  for (int i = 0; i < 6; i++)
  {
    ekf->x[i] += K[i] * y;
  }

  /* ---- Covariance update: P = (I − K*H) * P  (H = [0,0,1,0,0,0]) ----- *
   *  P_new[i][j] = P[i][j] − K[i] * P[2][j]                              */
  float P_new[6][6];
  for (int i = 0; i < 6; i++)
  {
    for (int j = 0; j < 6; j++)
    {
      P_new[i][j] = ekf->P[i][j] - K[i] * ekf->P[2][j];
    }
  }
  (void)memcpy(ekf->P, P_new, sizeof(P_new));
}
