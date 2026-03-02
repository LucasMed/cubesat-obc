#include "../../include/attitude_dynamics.h"

#include <math.h>
#include <stdio.h>

#define PASS(msg) printf("[PASS] %s\n", msg)
#define FAIL(msg) do { printf("[FAIL] %s\n", msg); return 1; } while (0)

/* ------------------------------------------------------------------ */
/* T-DYN-01: Basic sanity — rates and attitude update under torque     */
/* ------------------------------------------------------------------ */
static int test_basic_update(void)
{
  attitude_dyn_t dyn;
  attitude_dynamics_init(&dyn);
  float torque[3] = {0.1f, 0.0f, 0.0f};
  attitude_dynamics_step(&dyn, torque, 0.1f);
  if (!isfinite(dyn.rates[0]) || fabsf(dyn.rates[0]) < 1e-6f)
  {
    FAIL("T-DYN-01: rates[0] not updated");
  }
  if (!isfinite(dyn.attitude[0]) || fabsf(dyn.attitude[0]) < 1e-9f)
  {
    FAIL("T-DYN-01: attitude[0] not updated");
  }
  /* Axes 1 and 2 must stay zero */
  if (fabsf(dyn.rates[1]) > 1e-9f || fabsf(dyn.rates[2]) > 1e-9f)
  {
    FAIL("T-DYN-01: unexpected coupling to axes 1/2");
  }
  PASS("T-DYN-01: basic rates/attitude update");
  return 0;
}

/* ------------------------------------------------------------------ */
/* T-DYN-02: Analytical accuracy — RK2 must match exact solution for  */
/*           constant torque to within 1e-4 rad after 10 s @ 50 Hz.  */
/*                                                                    */
/*  Exact:  rates[0](t) = (T/I) * t                                  */
/*          att[0](t)   = 0.5 * (T/I) * t²                           */
/* ------------------------------------------------------------------ */
static int test_analytical_accuracy(void)
{
  attitude_dyn_t dyn;
  attitude_dynamics_init(&dyn);  /* inertia[0] = 0.01 kg·m² */

  const float T = 1e-3f;   /* torque [N·m]  */
  const float I = 0.01f;   /* inertia [kg·m²] */
  const float dt = 0.02f;  /* 50 Hz */
  const float t_end = 10.0f;
  const int steps = (int)(t_end / dt);

  float torque[3] = {T, 0.0f, 0.0f};
  for (int i = 0; i < steps; i++)
  {
    attitude_dynamics_step(&dyn, torque, dt);
  }

  float t = steps * dt;
  float rates_exact = (T / I) * t;
  float att_exact = 0.5f * (T / I) * t * t;

  if (fabsf(dyn.rates[0] - rates_exact) > 1e-4f)
  {
    printf("  rates error = %e (limit 1e-4)\n", fabsf(dyn.rates[0] - rates_exact));
    FAIL("T-DYN-02: rates accuracy exceeds 1e-4 rad/s");
  }
  if (fabsf(dyn.attitude[0] - att_exact) > 1e-4f)
  {
    printf("  attitude error = %e (limit 1e-4)\n", fabsf(dyn.attitude[0] - att_exact));
    FAIL("T-DYN-02: attitude accuracy exceeds 1e-4 rad");
  }
  PASS("T-DYN-02: RK2 matches analytical solution within 1e-4 rad");
  return 0;
}

/* ------------------------------------------------------------------ */
/* T-DYN-03: Energy conservation — with zero torque, rotational KE    */
/*           must be constant (no numerical dissipation).             */
/* ------------------------------------------------------------------ */
static int test_energy_conservation(void)
{
  attitude_dyn_t dyn;
  attitude_dynamics_init(&dyn);

  /* Give it initial rates */
  dyn.rates[0] = 0.05f;  /* rad/s */
  dyn.rates[1] = 0.03f;
  dyn.rates[2] = 0.02f;

  /* KE = 0.5 * sum(I[i] * rates[i]²) */
  float ke_init = 0.0f;
  for (int i = 0; i < 3; i++)
  {
    ke_init += 0.5f * dyn.inertia[i] * dyn.rates[i] * dyn.rates[i];
  }

  float torque[3] = {0.0f, 0.0f, 0.0f};
  const float dt = 0.02f;
  for (int i = 0; i < 500; i++) /* 10 s */
  {
    attitude_dynamics_step(&dyn, torque, dt);
  }

  float ke_final = 0.0f;
  for (int i = 0; i < 3; i++)
  {
    ke_final += 0.5f * dyn.inertia[i] * dyn.rates[i] * dyn.rates[i];
  }

  if (fabsf(ke_final - ke_init) > 1e-9f)
  {
    printf("  KE drift = %e J\n", fabsf(ke_final - ke_init));
    FAIL("T-DYN-03: kinetic energy not conserved under zero torque");
  }
  PASS("T-DYN-03: kinetic energy conserved under zero torque");
  return 0;
}

/* ------------------------------------------------------------------ */
/* T-DYN-04: Multi-axis independence — torques on separate axes must  */
/*           not couple to each other.                                */
/* ------------------------------------------------------------------ */
static int test_multiaxis_independence(void)
{
  attitude_dyn_t dyn;
  attitude_dynamics_init(&dyn);

  /* Apply torque only on yaw (axis 2) */
  float torque[3] = {0.0f, 0.0f, 5e-4f};
  const float dt = 0.05f;
  for (int i = 0; i < 100; i++)
  {
    attitude_dynamics_step(&dyn, torque, dt);
  }

  if (fabsf(dyn.rates[0]) > 1e-9f || fabsf(dyn.rates[1]) > 1e-9f)
  {
    FAIL("T-DYN-04: yaw torque spuriously coupled to roll/pitch rates");
  }
  if (fabsf(dyn.attitude[0]) > 1e-9f || fabsf(dyn.attitude[1]) > 1e-9f)
  {
    FAIL("T-DYN-04: yaw torque spuriously coupled to roll/pitch attitude");
  }
  PASS("T-DYN-04: axes are independent under single-axis torque");
  return 0;
}

/* ------------------------------------------------------------------ */
/* T-DYN-05: Finite-values guard — output must remain finite after    */
/*           1000 steps of maximum torque.                            */
/* ------------------------------------------------------------------ */
static int test_finite_values(void)
{
  attitude_dyn_t dyn;
  attitude_dynamics_init(&dyn);
  float torque[3] = {0.1f, 0.1f, 0.05f};
  const float dt = 0.05f;
  for (int i = 0; i < 1000; i++)
  {
    attitude_dynamics_step(&dyn, torque, dt);
  }
  for (int i = 0; i < 3; i++)
  {
    if (!isfinite(dyn.rates[i]) || !isfinite(dyn.attitude[i]))
    {
      FAIL("T-DYN-05: non-finite value after 1000 steps");
    }
  }
  PASS("T-DYN-05: all values remain finite after 1000 steps");
  return 0;
}

/* ------------------------------------------------------------------ */

int main(void)
{
  int result = 0;
  result |= test_basic_update();
  result |= test_analytical_accuracy();
  result |= test_energy_conservation();
  result |= test_multiaxis_independence();
  result |= test_finite_values();

  if (result == 0)
  {
    printf("All dynamics tests passed.\n");
  }
  return result;
}
