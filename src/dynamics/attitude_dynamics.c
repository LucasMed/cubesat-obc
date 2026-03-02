#include "attitude_dynamics.h"

#include "config.h"

#include <string.h>

void attitude_dynamics_init(attitude_dyn_t *ad)
{
  memset(ad, 0, sizeof(*ad));
  ad->inertia[0] = ad->inertia[1] = 0.01f;
  ad->inertia[2] = 0.005f;
}

void attitude_dynamics_step(attitude_dyn_t *ad, const float torque[3], float dt)
{
  // Simple Euler integration: rates_dot = torque / I
  for (int i = 0; i < 3; i++)
  {
    float accel = torque[i] / ad->inertia[i];
    ad->rates[i] += accel * dt;
    ad->attitude[i] += ad->rates[i] * dt;
  }
}
