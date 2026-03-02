#ifndef REACTION_WHEEL_H
#define REACTION_WHEEL_H

typedef struct
{
  float omega;  // rad/s
  float inertia;
} reaction_wheel_t;

void reaction_wheel_init(reaction_wheel_t *rw);
void reaction_wheel_apply_torque(reaction_wheel_t *rw, float torque, float dt);

#endif  // REACTION_WHEEL_H
