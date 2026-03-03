#include "attitude_dynamics.h"

#include "config.h"

#include <string.h>

void attitude_dynamics_init(attitude_dyn_t *ad)
{
  (void)memset(ad, 0, sizeof(*ad));
  ad->inertia[0] = 0.01f;
  ad->inertia[1] = 0.01f;
  ad->inertia[2] = 0.005f;
}

/**
 * @brief Propagate rigid-body rotational dynamics one step using
 *        2nd-order Runge-Kutta (midpoint method).
 *
 * State vector: x = [attitude[3], rates[3]]
 * Equations of motion:
 *   attitude_dot[i] = rates[i]
 *   rates_dot[i]    = torque[i] / inertia[i]
 *
 * Midpoint RK2:
 *   k1       = f(x)
 *   x_mid    = x + (dt/2) * k1
 *   k2       = f(x_mid)
 *   x_new    = x + dt * k2
 *
 * For constant torque the midpoint method is equivalent to the exact
 * analytical solution, eliminating the O(dt²) truncation error of
 * forward Euler.
 */
void attitude_dynamics_step(attitude_dyn_t *ad, const float torque[3], float dt)
{
  float rates_mid[3];

  /* --- Step 1: compute midpoint rates (k2_att in RK2 notation) ---
   *
   *  k1_rates[i] = torque[i] / inertia[i]
   *  k1_att[i]   = rates[i]
   *
   *  rates_mid[i] = rates[i] + (dt/2) * k1_rates[i]   ← used as k2_att
   */
  for (int i = 0; i < 3; i++)
  {
    rates_mid[i] = ad->rates[i] + 0.5f * dt * (torque[i] / ad->inertia[i]);
  }

  /* --- Step 2: advance full state using k2 ---
   *
   *  att_new[i]   = att[i]   + dt * k2_att[i]   = att[i]   + dt * rates_mid[i]
   *  rates_new[i] = rates[i] + dt * k2_rates[i] = rates[i] + dt * torque[i]/I[i]
   *
   * For constant torque this matches the exact analytical solution:
   *   att_new = att + dt*rates + 0.5*dt²*(T/I)
   */
  for (int i = 0; i < 3; i++)
  {
    ad->attitude[i] = ad->attitude[i] + dt * rates_mid[i];
    ad->rates[i] = ad->rates[i] + dt * (torque[i] / ad->inertia[i]);
  }
}
