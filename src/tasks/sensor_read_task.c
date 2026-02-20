#include "config.h"
#include "sensor_read_task.h"
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"

// Core logic for sensor reading (independent of FreeRTOS task loop)
void vSensorReadTask_Step(void) {
    // Read IMU (simulated or real)
    // printf("[sensor_read_task] Reading sensors...\n");
}

// Sensor read task: reads IMU at 10 Hz
void vSensorReadTask(void *pvParameters) {
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(100); // 10 Hz

    printf("[sensor_read_task] Started\n");

    while (1) {
        vSensorReadTask_Step();
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}
