#include "config.h"
#include "telemetry_task.h"
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"

#include "system_state.h"

// Core logic for telemetry (independent of FreeRTOS task loop)
void vTelemetryTask_Step(void) {
    system_state_t state;
    system_state_get(&state);

    printf("[telemetry] ATT[%.1f, %.1f, %.1f] GYRO[%.1f, %.1f, %.1f] TEMP[%.1f C]\n",
           state.attitude[0], state.attitude[1], state.attitude[2],
           state.rates[0], state.rates[1], state.rates[2],
           state.temp);
}

// Telemetry task: sends telemetry at 1 Hz
void vTelemetryTask(void *pvParameters) {
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(1000); // 1 Hz

    printf("[telemetry_task] Started\n");

    while (1) {
        vTelemetryTask_Step();
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}
