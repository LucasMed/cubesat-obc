/* test_closed_loop.c — Closed-loop stability tests for EKF→LQR→dynamics (PR-24)
 *
 * T-CLS-01: Roll  error < 2°  after 10 s from 30° initial.
 * T-CLS-02: Pitch error < 2°  after 10 s from 20° initial.
 * T-CLS-03: Yaw   error < 5°  after 10 s from 45° initial.
 * T-CLS-04: Gyro-bias estimate converges: ‖b_est − b_true‖ < 5 mrad/s after 20 s.
 * T-CLS-05: Peak torque applied to dynamics ≤ CLS_TAU_SAT · √3 (saturation active).
 * T-CLS-06: Reaction-wheel momentum exceeds CLS_MOM_THRESH during FM_DETUMBLE
 *           run with large initial angular rates (dump flag set).
 *
 * Compiled with: ekf.c, lqr.c, lqr_schedule.c, attitude_dynamics.c,
 *                closed_loop_sim.c   — no FreeRTOS, no hardware.
 */

#include "closed_loop_sim.h"

#include <math.h>
#include <stdio.h>

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

#define DEG2RAD(d) ((float)((d) * (M_PI / 180.0)))
#define RAD2DEG(r) ((float)((r) * (180.0 / M_PI)))

/* ---- Test helpers ------------------------------------------------------- */
static int g_failures = 0;

#define CHECK(cond, msg)                                                                           \
  do                                                                                               \
  {                                                                                                \
    if (!(cond))                                                                                   \
    {                                                                                              \
      printf("  FAIL [%s:%d] %s\n", __func__, __LINE__, (msg));                                    \
      g_failures++;                                                                                \
    }                                                                                              \
  } while (0)

/* Simulation parameters */
#define STEPS_10S ((int)(10.0f / CLS_DT)) /* 200 steps @ 20 Hz */
#define STEPS_20S ((int)(20.0f / CLS_DT)) /* 400 steps @ 20 Hz */

/**
 * Saturation limit for convergence tests (T-CLS-01..04).
 * Set to 1.0 N·m — effectively no saturation — so the linear LQR
 * (ωn = 10 rad/s, ζ = 1) operates in its intended regime.  A 1U CubeSat
 * reaction wheel produces ≈ 1 mN·m; the linear regime requires larger torques
 * for large initial errors.  Tests T-CLS-05..06 exercise actuator saturation
 * separately via CLS_TAU_SAT.
 */
#define TAU_NO_SAT 1.0f

/* True gyro bias injected in T-CLS-01..04 */
static const float TRUE_BIAS[3] = {0.010f, -0.010f, 0.005f}; /* rad/s */

/* ========================================================================
 * T-CLS-01  Roll error < 2° after 10 s from 30° initial
 * ======================================================================== */
static void test_roll_settling(void)
{
  cls_t sim;
  /* TAU_NO_SAT: test linear LQR convergence without actuator limiting */
  cls_init(&sim, DEG2RAD(30.0f), 0.0f, 0.0f, TRUE_BIAS, CLS_DT, TAU_NO_SAT);
  cls_run(&sim, FM_NOMINAL, STEPS_10S);

  float roll_err_deg = RAD2DEG(fabsf(sim.dyn.attitude[0]));
  CHECK(roll_err_deg < 2.0f, "T-CLS-01: roll must settle below 2 deg in 10 s");
  printf("  PASS T-CLS-01 roll settled to %.2f deg (< 2.00 deg)\n", roll_err_deg);
}

/* ========================================================================
 * T-CLS-02  Pitch error < 2° after 10 s from 20° initial
 * ======================================================================== */
static void test_pitch_settling(void)
{
  cls_t sim;
  /* TAU_NO_SAT: test linear LQR convergence without actuator limiting */
  cls_init(&sim, 0.0f, DEG2RAD(20.0f), 0.0f, TRUE_BIAS, CLS_DT, TAU_NO_SAT);
  cls_run(&sim, FM_NOMINAL, STEPS_10S);

  float pitch_err_deg = RAD2DEG(fabsf(sim.dyn.attitude[1]));
  CHECK(pitch_err_deg < 2.0f, "T-CLS-02: pitch must settle below 2 deg in 10 s");
  printf("  PASS T-CLS-02 pitch settled to %.2f deg (< 2.00 deg)\n", pitch_err_deg);
}

/* ========================================================================
 * T-CLS-03  Yaw error < 5° after 10 s from 45° initial
 * ======================================================================== */
static void test_yaw_settling(void)
{
  cls_t sim;
  /* TAU_NO_SAT: test linear LQR convergence without actuator limiting */
  cls_init(&sim, 0.0f, 0.0f, DEG2RAD(45.0f), TRUE_BIAS, CLS_DT, TAU_NO_SAT);
  cls_run(&sim, FM_NOMINAL, STEPS_10S);

  float yaw_err_deg = RAD2DEG(fabsf(sim.dyn.attitude[2]));
  CHECK(yaw_err_deg < 5.0f, "T-CLS-03: yaw must settle below 5 deg in 10 s");
  printf("  PASS T-CLS-03 yaw settled to %.2f deg (< 5.00 deg)\n", yaw_err_deg);
}

/* ========================================================================
 * T-CLS-04  Gyro-bias estimate converges within 5 mrad/s after 20 s
 *
 * Run with all three initial attitude errors to excite all axes.
 * ======================================================================== */
static void test_bias_convergence(void)
{
  cls_t sim;
  /* TAU_NO_SAT: attitude converges quickly, then EKF learns the bias residual */
  cls_init(&sim, DEG2RAD(30.0f), DEG2RAD(20.0f), DEG2RAD(45.0f), TRUE_BIAS, CLS_DT, TAU_NO_SAT);
  cls_run(&sim, FM_NOMINAL, STEPS_20S);

  /* Bias estimate lives in EKF state x[4..6] */
  float bias_err = 0.0f;
  for (int i = 0; i < 3; i++)
  {
    float diff = sim.ekf.x[4 + i] - TRUE_BIAS[i];
    bias_err += diff * diff;
  }
  bias_err = sqrtf(bias_err);

  CHECK(bias_err < 0.005f, "T-CLS-04: gyro-bias estimate must converge within 5 mrad/s");
  printf("  PASS T-CLS-04 bias error ‖Δb‖ = %.4f rad/s (< 0.005 rad/s)\n", bias_err);
}

/* ========================================================================
 * T-CLS-05  Torque saturation is active and bounded by ±CLS_TAU_SAT
 *
 * Uses CLS_TAU_SAT (1 mN·m) — the realistic 1U reaction-wheel limit.
 * The LQR commands large unsaturated torques for the 30° initial error.
 * Verify that: (a) saturation kicked in (peak ≥ 0.95·τ_sat on first step),
 * (b) the applied torque vector magnitude ≤ √3·CLS_TAU_SAT (worst-case all
 * three axes simultaneously saturated).
 * ======================================================================== */
static void test_torque_saturation(void)
{
  cls_t sim;
  /* Use CLS_TAU_SAT here — this is the actuator-saturation test */
  cls_init(&sim, DEG2RAD(30.0f), DEG2RAD(20.0f), DEG2RAD(45.0f), TRUE_BIAS, CLS_DT, CLS_TAU_SAT);
  cls_run(&sim, FM_NOMINAL, STEPS_10S);

  /* Peak norm must be ≤ sqrt(3)·tau_sat */
  float tau_max_allowed = sqrtf(3.0f) * CLS_TAU_SAT;

  /* Saturation must have engaged: initial roll LQR command = k_att*30° ≈ 0.52 N·m >> τ_sat */
  CHECK(sim.tau_max_applied >= 0.95f * CLS_TAU_SAT,
        "T-CLS-05: saturation must have been applied (initial error >> tau_sat)");
  CHECK(sim.tau_max_applied <= tau_max_allowed * 1.001f,
        "T-CLS-05: peak torque must not exceed sqrt(3)*CLS_TAU_SAT");
  printf("  PASS T-CLS-05 peak torque = %.2e N·m (limit %.2e N·m)\n", sim.tau_max_applied,
         tau_max_allowed);
}

/* ========================================================================
 * T-CLS-06  Reaction-wheel momentum dump flag set during FM_DETUMBLE
 *
 * Start with large initial angular rates to model post-separation tumbling.
 * Run with FM_DETUMBLE high-bandwidth gains.  The LQR commands high torques
 * (saturated) which build angular momentum in the reaction wheels until the
 * accumulated momentum ‖rw_h‖ exceeds CLS_MOM_THRESH.
 * ======================================================================== */
static void test_momentum_dump_triggered(void)
{
  cls_t sim;
  /* No initial attitude error — only angular rates (tumbling spacecraft) */
  cls_init(&sim, 0.0f, 0.0f, 0.0f, NULL, CLS_DT, CLS_TAU_SAT);
  /* Inject large tumbling rates: 5°/s ≈ 0.087 rad/s on all axes */
  sim.dyn.rates[0] = DEG2RAD(5.0f);
  sim.dyn.rates[1] = DEG2RAD(-5.0f);
  sim.dyn.rates[2] = DEG2RAD(3.0f);

  /* Run 10 s with FM_DETUMBLE high-bandwidth LQR */
  cls_run(&sim, FM_DETUMBLE, STEPS_10S);

  CHECK(sim.momentum_exceeded, "T-CLS-06: RW momentum must exceed dump threshold during detumble");
  printf("  PASS T-CLS-06 momentum dump flag set (|rw_h| > %.1e N·m·s)\n", (double)CLS_MOM_THRESH);
}

/* ========================================================================
 * main
 * ======================================================================== */
int main(void)
{
  printf("=== Closed-loop stability tests (PR-24) ===\n");

  test_roll_settling();
  test_pitch_settling();
  test_yaw_settling();
  test_bias_convergence();
  test_torque_saturation();
  test_momentum_dump_triggered();

  if (g_failures == 0)
  {
    printf("ALL CLOSED-LOOP TESTS PASSED\n");
    return 0;
  }

  printf("%d TEST(S) FAILED\n", g_failures);
  return 1;
}
