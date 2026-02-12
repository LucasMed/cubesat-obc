#include "../../include/pid.h"
#include <stdio.h>
#include <math.h>

int main(void) {
    pid_t p;
    pid_init(&p, 1.0f, 0.1f, 0.01f);
    float dt = 0.1f;
    float out1 = pid_update(&p, 1.0f, dt);
    float out2 = pid_update(&p, 1.0f, dt);
    if (!isfinite(out1) || !isfinite(out2)) {
        printf("PID produced non-finite output\n");
        return 1;
    }
    // Basic sanity: outputs should be finite numbers
    printf("PID outputs: %f, %f\n", out1, out2);
    printf("PID test passed\n");
    return 0;
}
