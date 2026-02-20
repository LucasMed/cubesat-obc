#include "config.h"
#include "attitude_control_task.h"
#include "attitude_control.h"
#include "attitude_dynamics.h"
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"

#include "system_state.h"
#ifdef PICO_BUILD
#include "pico/time.h"
#endif

static attitude_ctrl_t g_ctrl;
static attitude_dyn_t g_dyn;

// Core logic for attitude control (independent of FreeRTOS task loop)
void vAttitudeControlTask_Step(void) {
    float target[3] = {0.0f, 0.0f, 0.0f}; // From ground station or internal setpoint
    float outputs[3];
    const float dt = 1.0f / CONTROL_LOOP_HZ;

    // Get current state from thread-safe storage
    system_state_t state;
    system_state_get(&state);

    // Compute control based on real IMU feedback
    attitude_ctrl_update(&g_ctrl, target, state.attitude, state.rates, outputs, dt);

    // Step dynamics with computed torque
    float torque[3] = {outputs[0], outputs[1], outputs[2]};
    attitude_dynamics_step(&g_dyn, torque, dt);
}

// Attitude control task: runs control loop at 20 Hz
void vAttitudeControlTask(void *pvParameters) {
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(50); // 20 Hz

#ifdef PICO_BUILD
    uint64_t last_wake = time_us_64();
    uint64_t min_int = 0xFFFFFFFFFFFFFFFF;
    uint64_t max_int = 0;
    uint64_t sum_int = 0;
    uint32_t samples = 0;
#endif

    // Initialize control and dynamics
    attitude_ctrl_init(&g_ctrl);
    attitude_dynamics_init(&g_dyn);

    printf("[attitude_control_task] Started\n");

    while (1) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

#ifdef PICO_BUILD
        uint64_t now = time_us_64();
        uint64_t interval = now - last_wake;
        last_wake = now;

        if (samples > 0) {
            if (interval < min_int) min_int = interval;
            if (interval > max_int) max_int = interval;
            sum_int += interval;
        }
        samples++;

        if (samples >= 201) { // 200 samples for 20Hz (~10 seconds)
            printf("[attitude_control_task] Timing (200 samples): min=%llu, max=%llu, avg=%llu us\n",
                   min_int, max_int, sum_int / 200);
            min_int = 0xFFFFFFFFFFFFFFFF;
            max_int = 0;
            sum_int = 0;
            samples = 1;
        }
#endif

        vAttitudeControlTask_Step();
    }
}
