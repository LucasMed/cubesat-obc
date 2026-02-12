#include "../../include/reaction_wheel.h"
#include "../../include/magnetorquer.h"
#include <stdio.h>
#include <math.h>

int main(void) {
    reaction_wheel_t rw;
    reaction_wheel_init(&rw);
    magnetorquer_t mq;
    magnetorquer_init(&mq);

    reaction_wheel_apply_torque(&rw, 0.05f, 0.1f);
    if (!isfinite(rw.omega) || fabsf(rw.omega) < 1e-9f) {
        printf("Reaction wheel test failed\n");
        return 1;
    }

    magnetorquer_set_moment(&mq, 0.01f, 0.0f, 0.0f);
    if (!isfinite(mq.moment[0]) || fabsf(mq.moment[0]-0.01f) > 1e-9f) {
        printf("Magnetorquer test failed\n");
        return 1;
    }

    printf("Actuators test passed\n");
    return 0;
}
