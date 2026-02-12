#include "../../include/attitude_dynamics.h"
#include <stdio.h>
#include <math.h>

int main(void) {
    attitude_dyn_t dyn;
    attitude_dynamics_init(&dyn);
    float torque[3] = {0.1f, 0.0f, 0.0f};
    attitude_dynamics_step(&dyn, torque, 0.1f);
    if (!isfinite(dyn.rates[0]) || fabsf(dyn.rates[0]) < 1e-6f) {
        printf("Dynamics test failed: rates not updated\n");
        return 1;
    }
    printf("Dynamics test passed\n");
    return 0;
}
