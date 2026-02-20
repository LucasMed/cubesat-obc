#include "config.h"
#include "attitude_control_task.h"
#include "attitude_control.h"
#include "attitude_dynamics.h"
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"

static attitude_ctrl_t g_ctrl;
static attitude_dyn_t g_dyn;

// Core logic for attitude control (independent of FreeRTOS task loop)
void vAttitudeControlTask_Step(void) {
    float target[3] = {0.0f, 0.0f, 0.0f}; // From ground station or internal setpoint
    float current[3] = {0.0f, 0.0f, 0.0f}; // From sensor fusion
    float rates[3] = {0.0f, 0.0f, 0.0f};   // From gyro
    float outputs[3];
    const float dt = 1.0f / CONTROL_LOOP_HZ;

    // Compute control
    attitude_ctrl_update(&g_ctrl, target, current, rates, outputs, dt);

    // Step dynamics with computed torque
    float torque[3] = {outputs[0], outputs[1], outputs[2]};
    attitude_dynamics_step(&g_dyn, torque, dt);
}

// Attitude control task: runs control loop at 20 Hz
void vAttitudeControlTask(void *pvParameters) {
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(50); // 20 Hz

    // Initialize control and dynamics
    attitude_ctrl_init(&g_ctrl);
    attitude_dynamics_init(&g_dyn);

    printf("[attitude_control_task] Started\n");

    while (1) {
        vAttitudeControlTask_Step();
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}
