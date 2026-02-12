#include "config.h"
#include "health_monitor_task.h"
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"

// Health monitor task: monitors system every 5 seconds
void vHealthMonitorTask(void *pvParameters) {
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(5000); // 5 Hz (0.2 Hz logical)

    printf("[health_monitor_task] Started\n");

    while (1) {
        // Check system health: memory, actuator saturation, power, temps
        printf("[health_monitor_task] Health check\n");

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}
