#ifndef ATTITUDE_DYNAMICS_H
#define ATTITUDE_DYNAMICS_H

typedef struct {
    float attitude[3];
    float rates[3];
    float inertia[3];
} attitude_dyn_t;

void attitude_dynamics_init(attitude_dyn_t *ad);
void attitude_dynamics_step(attitude_dyn_t *ad, const float torque[3], float dt);

#endif // ATTITUDE_DYNAMICS_H
