#include "config.h"
#include "telemetry_task.h"
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"

// Core logic for telemetry (independent of FreeRTOS task loop)
void vTelemetryTask_Step(void) {
    // Package telemetry and transmit via WiFi/UART
    // In real code: gather attitude, rates, actuator state, power info, etc.
    printf("[telemetry_task] Sending telemetry frame\n");
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
