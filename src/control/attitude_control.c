#include "attitude_control.h"

#include "config.h"
#include "pid.h"

#include <stdio.h>

void attitude_ctrl_init(attitude_ctrl_t *ac)
{
  pid_init(&ac->pid_roll, DEFAULT_KP, DEFAULT_KI, DEFAULT_KD);
  pid_init(&ac->pid_pitch, DEFAULT_KP, DEFAULT_KI, DEFAULT_KD);
  pid_init(&ac->pid_yaw, DEFAULT_KP, DEFAULT_KI, DEFAULT_KD);
}

void attitude_ctrl_update(attitude_ctrl_t *ac, const float target[3], const float current[3],
                          const float rates[3], float outputs[3], float dt)
{
  (void)rates;
  // Simple decoupled PID per axis
  float err_roll = target[0] - current[0];
  float err_pitch = target[1] - current[1];
  float err_yaw = target[2] - current[2];

  outputs[0] = pid_update(&ac->pid_roll, err_roll, dt);
  outputs[1] = pid_update(&ac->pid_pitch, err_pitch, dt);
  outputs[2] = pid_update(&ac->pid_yaw, err_yaw, dt);
}
