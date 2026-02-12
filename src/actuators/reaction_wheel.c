
#include "config.h"
#include "reaction_wheel.h"
#include <stdio.h>

void reaction_wheel_init(reaction_wheel_t *rw) {
    rw->omega = 0.0f;
    rw->inertia = RW_INERTIA;
}

void reaction_wheel_apply_torque(reaction_wheel_t *rw, float torque, float dt) {
    // omega_dot = torque / I
    float omega_dot = torque / rw->inertia;
    rw->omega += omega_dot * dt;
}
